/**
 * log.c
 * Author: Zoe Knox      Created: 2026-02-15
 *
 * Copyright (C) 2026 Zoe Knox. All rights reserved.
 * SPDX: BSD-2-Clause
 *
 * This is an original implementation of Apple's libsystem_trace.dylib based on
 * open-source code, including ASL, XNU, Swift, and other sources, and the API
 * specs on developer.apple.com. It is not based on decompilation or diassembly
 * of any closed-source object files.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* We really want to use the structs internally */
#undef OS_OBJECT_USE_OBJC
#define OS_OBJECT_USE_OBJC 0

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <dlfcn.h>
#include <errno.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/un.h>
#include <mach-o/loader.h>
/* <objc/runtime.h> intentionally not included: nothing here uses the ObjC
 * runtime, and objc4 links against libSystem, so it cannot be a build
 * dependency of libSystem. */
/* CoreFoundation types come from init.h - see the note there on why the real
 * CoreFoundation headers are not included. */
#include "log_internal.h"
#include <os/log_private.h>
/*
 * <os/log_encode_types.h> and <os/log_encode.h> are not included: the copies in
 * Kernel/xnu/libkern are the kernel-side encoder and pull in <os/log_mem.h>,
 * whose logmem_s embeds lck_spin_t. The only declaration wanted from them here
 * is the argument-chunk header walked by os_log_pack_size(), defined below.
 */

/*
 * Layout of one entry in the packed argument chunk: a descriptor byte, a
 * payload size byte, then payload_size bytes of data.
 */
typedef struct {
    uint8_t descriptor;
    uint8_t payload_size;
} _os_log_arg_header;
#include <limits.h>
#include "init.h"


struct os_log_s _os_log_default;
struct os_log_s _os_log_disabled;
struct os_log_s _os_log_null;

// libdispatch's OS_object, linked into the same libSystem
extern char _os_log_object_class[] __asm__("_OBJC_CLASS_$_OS_object");

// an OS_object with the global refcount, so objc_retain, os_retain and their releases do nothing
static void
_os_log_make_immortal(struct os_log_s *log)
{
    struct { void *isa; int volatile ref_cnt; int volatile xref_cnt; } *header = (void *)log->pad;

    header->isa = _os_log_object_class;
    header->ref_cnt = INT_MAX;
    header->xref_cnt = INT_MAX;
}

__attribute__((constructor))
static void
_os_log_init_statics(void)
{
    _os_log_make_immortal(&_os_log_default);
    _os_log_make_immortal(&_os_log_disabled);
    _os_log_make_immortal(&_os_log_null);
}

__BEGIN_DECLS

os_log_t
os_log_create(const char* subsystem, const char* category)
{
    struct os_log_s* log = calloc(1, sizeof(struct os_log_s));
    if (!log)
        return (os_log_t)NULL;

    log->subsystem = subsystem != NULL ? strdup(subsystem) : NULL;
    log->category = category != NULL ? strdup(category) : NULL;
    log->sink_type = OS_LOG_SINK_TYPE_FD;
    log->sink_dest = STDOUT_FILENO;
    _os_log_make_immortal(log);

    // immortal, so callers that cache it in a static and ARC callers that retain it both work
    return log;
}

bool
os_log_shim_enabled(os_log_t log, os_log_type_t type)
{
    switch (type) {
        case OS_LOG_TYPE_DEFAULT:
        case OS_LOG_TYPE_DEBUG:
        case OS_LOG_TYPE_INFO:
            return true;
        default:
            return false;
    }
    return true;
}

bool os_log_type_enabled(os_log_t log, os_log_type_t type)
{
    static _Atomic(int) resolved;
    static os_log_type_t lowest_enabled;

    if (log == &_os_log_disabled)
        return false;

    if (!atomic_load_explicit(&resolved, memory_order_acquire)) {
        const char *mode = getenv("OS_ACTIVITY_MODE");
        os_log_type_t lowest = OS_LOG_TYPE_DEFAULT;

        if (mode != NULL) {
            if (strcmp(mode, "debug") == 0) {
                lowest = OS_LOG_TYPE_DEBUG;
            } else if (strcmp(mode, "info") == 0) {
                lowest = OS_LOG_TYPE_INFO;
            }
        }
        lowest_enabled = lowest;
        atomic_store_explicit(&resolved, 1, memory_order_release);
    }

    /* The type values are not ordered by severity: debug is 0x02 and info is
     * 0x01, both below default (0x00) in importance but above it numerically. */
    switch (type) {
        case OS_LOG_TYPE_DEBUG:
            return lowest_enabled == OS_LOG_TYPE_DEBUG;
        case OS_LOG_TYPE_INFO:
            return lowest_enabled == OS_LOG_TYPE_DEBUG
                || lowest_enabled == OS_LOG_TYPE_INFO;
        default:
            /* default, error and fault are always emitted. */
            return true;
    }
}

bool
os_log_info_enabled(os_log_t log)
{
    return os_log_shim_enabled(log, OS_LOG_TYPE_INFO)
        && os_log_type_enabled(log, OS_LOG_TYPE_INFO);
}

bool
os_log_debug_enabled(os_log_t log)
{
    return os_log_shim_enabled(log, OS_LOG_TYPE_DEBUG)
        && os_log_type_enabled(log, OS_LOG_TYPE_DEBUG);
}

/**
 * @function os_log_encode
 * @abstract Encodes the provided variable arguments using the provided
 *           format string into the memory pointed to by buffer. It will
 *           write at most buffer_size bytes. The encoded buffer can then
 *           be passed to os_log_pack_size() and os_log_pack_fill().
 * @param buffer       Pointer to destination buffer
 * @param buffer_size  Size of the destination buffer
 * @param format       printf-style format string
 * @param args         Varargs list to fill in format string
 * @param flags        Currently not used
 * @return The number of bytes written to the buffer. Zero is returned
 *         if buffer is NULL, buffer_size is too small, or an error
 *         occurred in serialization.
 */
size_t os_log_encode(void* buffer,
    size_t buffer_size,
    const char* format,
    va_list args,
    uint32_t flags)
{
    /*
     * Serialising into the binary os_log tracepoint format needs
     * _os_log_encode() from libtrace's private encoder, which PureDarwin does
     * not have: the only encoder in tree is XNU's kernel-side
     * os_log_context_encode(), which requires a logmem_t arena and a
     * preinitialised os_log_context_s that only the kernel can supply.
     *
     * Returning 0 is the documented "nothing was encoded" answer and is what
     * every caller here already handles; the human-readable paths below do not
     * route through this function.
     */
    (void)buffer; (void)buffer_size; (void)format; (void)args; (void)flags;
    return 0;
}

/* Wrapper used by os/assumes.h */
size_t __os_log_encode(void* buffer,
    size_t buffer_size,
    const char* format,
    ...)
{
    va_list args;
    va_start(args, format);
    size_t size = os_log_encode(buffer, buffer_size, format, args, 0);
    va_end(args);
    return size;
}

struct pd_out {
    char* p;
    size_t rem;
    size_t len;
};

static void pd_out_advance(struct pd_out* o, size_t n)
{
    size_t copied = 0;

    if (o->rem > 1)
        copied = n < o->rem - 1 ? n : o->rem - 1;
    o->p += copied;
    o->rem -= copied;
    o->len += n;
}

static void pd_out_append(struct pd_out* o, const char* s, size_t n)
{
    if (o->rem > 1)
        memcpy(o->p, s, n < o->rem - 1 ? n : o->rem - 1);
    pd_out_advance(o, n);
}

#define PD_OUT_PRINTF(o, ...) do { \
        int pd_n_ = snprintf((o)->rem ? (o)->p : NULL, (o)->rem, __VA_ARGS__); \
        if (pd_n_ > 0) \
            pd_out_advance((o), (size_t)pd_n_); \
    } while (0)

/*
 * %@ is CoreFoundation's specifier, not printf's, so rendering it means asking
 * CF for the object's description. libsystem_trace sits below CoreFoundation
 * and cannot link it, so look the three entry points up at runtime instead:
 * processes without CF (and there are several here) simply get the pointer.
 */
static void pd_append_cf_object(struct pd_out* o, const void* cf)
{
    static void*    (*copy_description)(const void*);
    static int      (*get_cstring)(const void*, char*, long, uint32_t);
    static void     (*cf_release)(const void*);
    static int      looked_up;
    char            desc[512];
    void*           str;

    if (!looked_up) {
        copy_description = dlsym(RTLD_DEFAULT, "CFCopyDescription");
        get_cstring      = dlsym(RTLD_DEFAULT, "CFStringGetCString");
        cf_release       = dlsym(RTLD_DEFAULT, "CFRelease");
        looked_up = 1;
    }

    if (cf == NULL) {
        pd_out_append(o, "(null)", 6);
        return;
    }

    // an Objective-C object, CFStrings included, says what -description says, as NSLog prints it
    {
        static void* (*send)(const void*, void*, ...);
        static void* (*sel)(const char*);
        static int   objc_looked_up;

        if (!objc_looked_up) {
            send = dlsym(RTLD_DEFAULT, "objc_msgSend");
            sel  = dlsym(RTLD_DEFAULT, "sel_registerName");
            objc_looked_up = 1;
        }
        if (send != NULL && sel != NULL) {
            void* text = send(cf, sel("description"));
            const char* utf8 = text != NULL ? send(text, sel("UTF8String")) : NULL;

            if (utf8 != NULL) {
                pd_out_append(o, utf8, strlen(utf8));
                return;
            }
        }
    }

    if (copy_description == NULL || get_cstring == NULL || cf_release == NULL) {
        PD_OUT_PRINTF(o, "<%p>", cf);
        return;
    }

    str = copy_description(cf);
    if (str == NULL) {
        PD_OUT_PRINTF(o, "<%p>", cf);
        return;
    }

    /* 0x08000100 is kCFStringEncodingUTF8. */
    if (get_cstring(str, desc, (long)sizeof(desc), 0x08000100)) {
        pd_out_append(o, desc, strlen(desc));
    } else {
        PD_OUT_PRINTF(o, "<%p>", cf);
    }
    cf_release(str);
}

/* where a render takes its arguments from: a va_list, or the buffer os_log's macros pack, two header bytes
 * (flags, count) and per argument a descriptor byte, a size byte and the value. shim marks syslog(3) and
 * asl, whose arguments macOS records as they are */
struct pd_os_log_args {
    va_list* ap;
    const uint8_t* at;
    const uint8_t* end;
    bool shim;
    int saved_errno;
};

static void pd_os_log_packed(struct pd_os_log_args* a, const uint8_t* buffer, uint32_t size, int saved_errno)
{
    a->ap = NULL;
    a->at = buffer != NULL && size >= 2 ? buffer + 2 : NULL;
    a->end = buffer != NULL && size >= 2 ? buffer + size : NULL;
    a->shim = false;
    a->saved_errno = saved_errno;
}

static const uint8_t* pd_os_log_item(struct pd_os_log_args* a, uint8_t* size)
{
    const uint8_t* value;

    if (a->at == NULL || a->at + 2 > a->end) {
        *size = 0;
        return NULL;
    }
    *size = a->at[1];
    value = a->at + 2;
    if (value + *size > a->end) {
        a->at = a->end;
        *size = 0;
        return NULL;
    }
    a->at = value + *size;
    return value;
}

static int64_t pd_os_log_packed_int(struct pd_os_log_args* a)
{
    uint8_t size;
    const uint8_t* v = pd_os_log_item(a, &size);
    int8_t b;
    int16_t h;
    int32_t w;
    int64_t q = 0;

    switch (v != NULL ? size : 0) {
    case 1: memcpy(&b, v, 1); return b;
    case 2: memcpy(&h, v, 2); return h;
    case 4: memcpy(&w, v, 4); return w;
    case 8: memcpy(&q, v, 8); return q;
    }
    return 0;
}

static double pd_os_log_packed_double(struct pd_os_log_args* a)
{
    uint8_t size;
    const uint8_t* v = pd_os_log_item(a, &size);
    float f;
    double d = 0;

    if (v != NULL && size == 4) {
        memcpy(&f, v, 4);
        return f;
    }
    if (v != NULL && size == 8)
        memcpy(&d, v, 8);
    return d;
}

static const void* pd_os_log_packed_pointer(struct pd_os_log_args* a)
{
    uint8_t size;
    const uint8_t* v = pd_os_log_item(a, &size);
    const void* ptr = NULL;

    if (v != NULL && size == sizeof(ptr))
        memcpy(&ptr, v, sizeof(ptr));
    return ptr;
}

#define ARG_i(a) pd_os_log_packed_int(a)
#define ARG_d(a) pd_os_log_packed_double(a)
#define ARG_p(a) pd_os_log_packed_pointer(a)
#define ARG(type, kind) (a->ap != NULL ? va_arg(*a->ap, type) : (type)ARG_##kind(a))

/* what the log keeps and what a caller composes hide private values, the stderr mirror shows them */
enum pd_os_log_mode {
    PD_OS_LOG_REDACTED,
    PD_OS_LOG_PLAIN,
};

enum { PD_PRIVACY_DEFAULT, PD_PRIVACY_PUBLIC, PD_PRIVACY_PRIVATE };
enum { PD_STYLE_NONE, PD_STYLE_BOOL, PD_STYLE_YESNO, PD_STYLE_ERRNO, PD_STYLE_UUID };

static const char* pd_os_log_annotation(const char* p, int* privacy, int* style)
{
    const char* end = strchr(p, '}');

    if (end == NULL)
        return p + 1;
    for (p++; p < end; p++) {
        const char* word;
        size_t len;

        while (p < end && (*p == ' ' || *p == ','))
            p++;
        word = p;
        while (p < end && *p != ',')
            p++;
        len = (size_t)(p - word);
        while (len > 0 && word[len - 1] == ' ')
            len--;
#define PD_IS(w) (len == sizeof(w) - 1 && memcmp(word, w, len) == 0)
        if (PD_IS("public"))
            *privacy = PD_PRIVACY_PUBLIC;
        else if (PD_IS("private") || PD_IS("sensitive"))
            *privacy = PD_PRIVACY_PRIVATE;
        else if (PD_IS("bool"))
            *style = PD_STYLE_BOOL;
        else if (PD_IS("BOOL"))
            *style = PD_STYLE_YESNO;
        else if (PD_IS("errno") || PD_IS("darwin.errno"))
            *style = PD_STYLE_ERRNO;
        else if (PD_IS("uuid_t"))
            *style = PD_STYLE_UUID;
#undef PD_IS
    }
    return end + 1;
}

// macOS's log store keeps a string or object only when it lies in an image's __TEXT, as a literal does
static bool pd_os_log_static(const void* ptr)
{
    const struct mach_header_64* mh;
    const struct load_command* lc;
    Dl_info info;

    if (!dladdr(ptr, &info) || info.dli_fbase == NULL)
        return false;
    mh = info.dli_fbase;
    if (mh->magic != MH_MAGIC_64)
        return false;
    lc = (const struct load_command*)(mh + 1);
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        if (lc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64* seg = (const struct segment_command_64*)lc;

            if (strcmp(seg->segname, SEG_TEXT) == 0)
                return (uintptr_t)ptr - (uintptr_t)mh < seg->vmsize;
        }
        lc = (const struct load_command*)((const char*)lc + lc->cmdsize);
    }
    return false;
}

enum { PD_VALUE_SCALAR, PD_VALUE_STRING, PD_VALUE_DATA };

static bool pd_os_log_hidden(const struct pd_os_log_args* a, enum pd_os_log_mode mode,
    int privacy, int kind, const void* value)
{
    if (mode == PD_OS_LOG_PLAIN || privacy == PD_PRIVACY_PUBLIC)
        return false;
    if (privacy == PD_PRIVACY_PRIVATE)
        return true;
    if (a->shim || kind == PD_VALUE_SCALAR || value == NULL)
        return false;
    return kind == PD_VALUE_DATA || !pd_os_log_static(value);
}

static void pd_os_log_bytes(struct pd_out* o, const unsigned char* bytes, size_t n, int style)
{
    if (style == PD_STYLE_UUID && n >= 16) {
        PD_OUT_PRINTF(o, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
            bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
        return;
    }
    pd_out_append(o, "'", 1);
    for (size_t i = 0; i < n; i++)
        PD_OUT_PRINTF(o, i == 0 ? "%02x" : " %02x", bytes[i]);
    pd_out_append(o, "'", 1);
}

/*
 * Render a format string and its arguments to text, the way mode asks for private values. Returns the
 * length of the whole text, which may be more than out_size can hold.
 *
 * vsnprintf cannot be used on the whole string because os_log formats may carry
 * %@ and annotations, and once one specifier has to be handled here the rest must be too - a
 * va_list cannot be re-entered partway through. So walk the format, hand each
 * ordinary specifier to snprintf on its own, and expand %@ via CoreFoundation.
 */
static size_t pd_os_log_render(char* out, size_t out_size, const char* format,
    struct pd_os_log_args* a, enum pd_os_log_mode mode)
{
    struct pd_out o = { out, out_size, 0 };
    const char* p = format != NULL ? format : "";

    while (*p != '\0') {
        char spec[128];
        size_t slen = 1, n;
        int privacy = PD_PRIVACY_DEFAULT, style = PD_STYLE_NONE;
        /* 0 = int, 1 = long or wide, 2 = 64-bit */
        int lenmod = 0;
        int prec = -1;
        bool prec_literal = false;
        const char* lit;
        char conv;

        if (*p != '%') {
            lit = p;
            while (*p != '\0' && *p != '%')
                p++;
            pd_out_append(&o, lit, (size_t)(p - lit));
            continue;
        }
        p++;
        if (*p == '%') {
            pd_out_append(&o, "%", 1);
            p++;
            continue;
        }
        if (*p == '{')
            p = pd_os_log_annotation(p, &privacy, &style);

        spec[0] = '%';
        for (n = 0; *p != '\0' && strchr("-+ #0'", *p) != NULL; p++)
            if (n++ < 20)
                spec[slen++] = *p;
        if (*p == '*') {
            slen += (size_t)snprintf(spec + slen, 16, "%d", (int)ARG(int, i));
            p++;
        } else {
            for (n = 0; *p >= '0' && *p <= '9'; p++)
                if (n++ < 20)
                    spec[slen++] = *p;
        }
        if (*p == '.') {
            spec[slen++] = *p++;
            if (*p == '*') {
                prec = (int)ARG(int, i);
                slen += (size_t)snprintf(spec + slen, 16, "%d", prec);
                p++;
            } else {
                prec_literal = true;
                for (prec = 0, n = 0; *p >= '0' && *p <= '9'; p++) {
                    prec = prec * 10 + (*p - '0');
                    if (n++ < 20)
                        spec[slen++] = *p;
                }
            }
        }
        for (n = 0; *p != '\0' && strchr("hljztLq", *p) != NULL; p++) {
            if (*p == 'l')
                lenmod = lenmod == 1 ? 2 : 1;
            else if (*p == 'j' || *p == 'z' || *p == 't' || *p == 'q')
                lenmod = 2;
            if (n++ < 4)
                spec[slen++] = *p;
        }
        conv = *p;
        if (conv == '\0')
            break;
        p++;
        spec[slen++] = conv;
        spec[slen] = '\0';

        switch (conv) {
        case 'd': case 'i': case 'o': case 'u': case 'x': case 'X': {
            bool is_signed = conv == 'd' || conv == 'i';
            long long v;

            if (lenmod == 2)
                v = is_signed ? ARG(long long, i) : (long long)ARG(unsigned long long, i);
            else if (lenmod == 1)
                v = is_signed ? ARG(long, i) : (long long)ARG(unsigned long, i);
            else
                v = is_signed ? ARG(int, i) : (long long)ARG(unsigned int, i);
            if (pd_os_log_hidden(a, mode, privacy, PD_VALUE_SCALAR, NULL))
                pd_out_append(&o, "<private>", 9);
            else if (style == PD_STYLE_BOOL)
                PD_OUT_PRINTF(&o, "%s", v ? "true" : "false");
            else if (style == PD_STYLE_YESNO)
                PD_OUT_PRINTF(&o, "%s", v ? "YES" : "NO");
            else if (style == PD_STYLE_ERRNO)
                PD_OUT_PRINTF(&o, "[%lld: %s]", v, strerror((int)v));
            else if (lenmod == 2)
                PD_OUT_PRINTF(&o, spec, v);
            else if (lenmod == 1)
                PD_OUT_PRINTF(&o, spec, (long)v);
            else
                PD_OUT_PRINTF(&o, spec, (int)v);
            break;
        }
        case 'c': case 'C': {
            int v = ARG(int, i);

            if (pd_os_log_hidden(a, mode, privacy, PD_VALUE_SCALAR, NULL))
                pd_out_append(&o, "<private>", 9);
            else
                PD_OUT_PRINTF(&o, spec, v);
            break;
        }
        case 's': case 'S': {
            const void* v;

            /* clang packs the literal precision of %.Ns and %.NP as an item of its own */
            if (a->ap == NULL && prec_literal)
                (void)pd_os_log_packed_int(a);
            v = ARG(const void*, p);

            if (pd_os_log_hidden(a, mode, privacy, PD_VALUE_STRING, v))
                pd_out_append(&o, "<private>", 9);
            else if (v == NULL)
                pd_out_append(&o, "(null)", 6);
            else if (conv == 'S' || lenmod == 1)
                PD_OUT_PRINTF(&o, spec, (const wchar_t*)v);
            else
                PD_OUT_PRINTF(&o, spec, (const char*)v);
            break;
        }
        case 'p': {
            const void* v = ARG(const void*, p);

            if (pd_os_log_hidden(a, mode, privacy, PD_VALUE_SCALAR, NULL))
                pd_out_append(&o, "<private>", 9);
            else
                PD_OUT_PRINTF(&o, spec, v);
            break;
        }
        case 'P': {
            const unsigned char* v;

            if (a->ap == NULL && prec_literal)
                (void)pd_os_log_packed_int(a);
            v = ARG(const void*, p);

            if (pd_os_log_hidden(a, mode, privacy, PD_VALUE_DATA, v))
                pd_out_append(&o, "<private>", 9);
            else if (v == NULL)
                pd_out_append(&o, "(null)", 6);
            else
                pd_os_log_bytes(&o, v, prec > 0 ? (size_t)prec : 0, style);
            break;
        }
        case '@': {
            const void* v = ARG(const void*, p);

            if (pd_os_log_hidden(a, mode, privacy, PD_VALUE_STRING, v))
                pd_out_append(&o, "<private>", 9);
            else
                pd_append_cf_object(&o, v);
            break;
        }
        case 'm': {
            uint8_t size;

            if (a->ap == NULL)
                (void)pd_os_log_item(a, &size);
            if (pd_os_log_hidden(a, mode, privacy, PD_VALUE_SCALAR, NULL))
                pd_out_append(&o, "<private>", 9);
            else
                PD_OUT_PRINTF(&o, "%s", strerror(a->saved_errno));
            break;
        }
        case 'f': case 'F': case 'e': case 'E':
        case 'g': case 'G': case 'a': case 'A': {
            double v = ARG(double, d);

            if (pd_os_log_hidden(a, mode, privacy, PD_VALUE_SCALAR, NULL))
                pd_out_append(&o, "<private>", 9);
            else
                PD_OUT_PRINTF(&o, spec, v);
            break;
        }
        case 'n':
            /* Never write through a caller pointer from a log format. */
            (void)ARG(void*, p);
            break;
        default:
            pd_out_append(&o, spec, slen);
            break;
        }
    }

    if (o.rem > 0)
        *o.p = '\0';
    return o.len;
}

#define PD_OS_LOG_SOCKET "/var/run/syslog"
#define PD_OS_LOG_LINE_MAX 2048

// OS_ACTIVITY_DT_MODE in the environment, whatever its value, mirrors os_log to stderr on macOS
static bool pd_os_log_dt_mode(void)
{
    static _Atomic(int) mode;
    int m = atomic_load_explicit(&mode, memory_order_relaxed);

    if (m == 0) {
        m = getenv("OS_ACTIVITY_DT_MODE") != NULL ? 2 : 1;
        atomic_store_explicit(&mode, m, memory_order_relaxed);
    }
    return m == 2;
}

static const char* pd_os_log_subsystem(os_log_t log)
{
    const struct os_log_s* s = (const struct os_log_s*)log;

    return s != NULL && s->subsystem != NULL ? s->subsystem : "";
}

static const char* pd_os_log_category(os_log_t log)
{
    const struct os_log_s* s = (const struct os_log_s*)log;

    return s != NULL && s->category != NULL ? s->category : "";
}

/* one line for logd on the syslog socket, in the shape of `log show --style compact` */
static void pd_os_log_record(os_log_t log, os_log_type_t type, const char* message)
{
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    char line[PD_OS_LOG_LINE_MAX];
    const char* tag;
    struct timespec now;
    struct tm tm;
    uint64_t tid = 0;
    int severity, n, fd;

    switch (type) {
    case OS_LOG_TYPE_INFO: tag = "I "; severity = LOG_INFO; break;
    case OS_LOG_TYPE_DEBUG: tag = "Db"; severity = LOG_DEBUG; break;
    case OS_LOG_TYPE_ERROR: tag = "E "; severity = LOG_ERR; break;
    case OS_LOG_TYPE_FAULT: tag = "F "; severity = LOG_CRIT; break;
    default: tag = "Df"; severity = LOG_NOTICE; break;
    }
    clock_gettime(CLOCK_REALTIME, &now);
    localtime_r(&now.tv_sec, &tm);
    pthread_threadid_np(NULL, &tid);
    n = snprintf(line, sizeof(line), "<%d>%04d-%02d-%02d %02d:%02d:%02d.%03ld %s %s[%d:%llx] ",
        LOG_USER | severity, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
        tm.tm_sec, now.tv_nsec / 1000000, tag, getprogname(), getpid(), (unsigned long long)tid);
    if (n < 0 || (size_t)n >= sizeof(line))
        return;
    if (*pd_os_log_subsystem(log) != '\0') {
        int m = snprintf(line + n, sizeof(line) - (size_t)n, *pd_os_log_category(log) != '\0' ? "[%s:%s] " : "[%s] ",
            pd_os_log_subsystem(log), pd_os_log_category(log));

        if (m < 0 || (size_t)m >= sizeof(line) - (size_t)n)
            return;
        n += m;
    }
    strlcpy(line + n, message, sizeof(line) - (size_t)n);

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0)
        return;
    strlcpy(addr.sun_path, PD_OS_LOG_SOCKET, sizeof(addr.sun_path));
    (void)sendto(fd, line, strlen(line), MSG_DONTWAIT, (const struct sockaddr*)&addr, sizeof(addr));
    (void)close(fd);
}

static void pd_os_log_stderr(os_log_t log, const char* message)
{
    char line[PD_OS_LOG_LINE_MAX];
    struct timespec now;
    struct tm tm;
    uint64_t tid = 0;
    long off;
    int n;

    clock_gettime(CLOCK_REALTIME, &now);
    localtime_r(&now.tv_sec, &tm);
    pthread_threadid_np(NULL, &tid);
    off = labs(tm.tm_gmtoff);
    n = snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d:%02d.%06ld%c%02ld%02ld %s[%d:%llu] %s%s%s%s\n",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
        now.tv_nsec / 1000, tm.tm_gmtoff < 0 ? '-' : '+', off / 3600, off % 3600 / 60,
        getprogname(), getpid(), (unsigned long long)tid,
        *pd_os_log_category(log) != '\0' ? "[" : "", pd_os_log_category(log),
        *pd_os_log_category(log) != '\0' ? "] " : "", message);
    if (n < 0)
        return;
    if ((size_t)n >= sizeof(line)) {
        n = sizeof(line) - 1;
        line[n - 1] = '\n';
    }
    (void)write(STDERR_FILENO, line, (size_t)n);
}

static void pd_os_log_send_message(os_log_t log, os_log_type_t type, const char* message)
{
    int saved_errno = errno;

    if (message != NULL && os_log_type_enabled(log, type)) {
        pd_os_log_record(log, type, message);
        if (pd_os_log_dt_mode())
            pd_os_log_stderr(log, message);
    }
    errno = saved_errno;
}

static void pd_os_log_render_copy(char* out, size_t out_size, const char* format,
    const struct pd_os_log_args* source, enum pd_os_log_mode mode)
{
    struct pd_os_log_args a = *source;
    va_list ap;

    if (source->ap != NULL) {
        va_copy(ap, *source->ap);
        a.ap = &ap;
    }
    pd_os_log_render(out, out_size, format, &a, mode);
    if (source->ap != NULL)
        va_end(ap);
}

static void pd_os_log_send(os_log_t log, os_log_type_t type, const char* format,
    const struct pd_os_log_args* source)
{
    char message[PD_OS_LOG_LINE_MAX];
    int saved_errno = errno;

    if (format != NULL && os_log_type_enabled(log, type)) {
        pd_os_log_render_copy(message, sizeof(message), format, source, PD_OS_LOG_REDACTED);
        pd_os_log_record(log, type, message);
        if (pd_os_log_dt_mode()) {
            pd_os_log_render_copy(message, sizeof(message), format, source, PD_OS_LOG_PLAIN);
            pd_os_log_stderr(log, message);
        }
    }
    errno = saved_errno;
}

__OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0)
OS_EXPORT OS_NOTHROW
void
os_log_with_args(os_log_t oslog, os_log_type_t type, const char* format, va_list args, void* ret_addr)
{
    struct pd_os_log_args a = { &args, NULL, NULL, true, errno };

    (void)ret_addr;
    pd_os_log_send(oslog, type, format, &a);
}

__WATCHOS_AVAILABLE(3.0) __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0)
OS_EXPORT OS_NOTHROW
void
_os_log_internal(void* dso, os_log_t log, os_log_type_t type, const char* message, ...)
{
    struct pd_os_log_args a = { NULL, NULL, NULL, false, errno };
    va_list ap;

    (void)dso;
    va_start(ap, message);
    a.ap = &ap;
    pd_os_log_send(log, type, message, &a);
    va_end(ap);
}

__WATCHOS_AVAILABLE(3.0) __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0)
OS_EXPORT OS_NOTHROW
int
_os_log_internal_driverKit(void* dso, os_log_t log, os_log_type_t type, const char* message, ...)
{
    struct pd_os_log_args a = { NULL, NULL, NULL, false, errno };
    va_list ap;

    (void)dso;
    va_start(ap, message);
    a.ap = &ap;
    pd_os_log_send(log, type, message, &a);
    va_end(ap);
    return 0;
}

/*
 * The pack payload holds the rendered message rather than the format string
 * plus encoded arguments: without libtrace's binary tracepoint encoder (see
 * os_log_encode() above) the arguments cannot be serialised, and discarding
 * them left every %@/%s to reach the console literally. Reserve a fixed payload
 * so the rendering has somewhere to go.
 */
#define PD_OS_LOG_RENDER_MAX 1024

size_t os_log_pack_size(const char* format, ...)
{
    if (!format) {
        return 0;
    }
    return sizeof(struct os_log_pack_s) + PD_OS_LOG_RENDER_MAX;
}

uint8_t* os_log_pack_fill(void* pack,
    size_t pack_size,
    int saved_errno,
    const char* format, ...)
{
    const void* dso = __builtin_return_address(0);
    Dl_info info;
    struct os_log_pack_s *s;
    uint8_t* payload;
    size_t avail;
    va_list ap;

    if (!pack || !format || pack_size <= sizeof(struct os_log_pack_s)) {
        return NULL;
    }

    s = (struct os_log_pack_s *)pack;
    payload = (uint8_t*)pack + sizeof(struct os_log_pack_s);
    avail = pack_size - sizeof(struct os_log_pack_s);

    s->olp_continuous_time = mach_continuous_time();
    clock_gettime(CLOCK_REALTIME, &s->olp_wall_time);

    /* Find the mach_header */
    if (dladdr(dso, &info)) {
        s->olp_mh = info.dli_fbase;
    }
    s->olp_pc = dso;

    va_start(ap, format);
    {
        struct pd_os_log_args args = { &ap, NULL, NULL, false, saved_errno };

        pd_os_log_render((char*)payload, avail, format, &args, PD_OS_LOG_REDACTED);
    }
    va_end(ap);

    s->olp_format = (const char *)payload;

    return payload + strlen((const char*)payload) + 1;
}

/* The message os_log_pack_fill() rendered. */
char* os_log_pack_compose(os_log_pack_t pack,
    os_log_t log,
    os_log_type_t type,
    char* buffer,
    size_t buffer_size)
{
    (void)log; (void)type;

    if (buffer == NULL || buffer_size == 0)
        return NULL;

    if (pack == NULL || pack->olp_format == NULL) {
        buffer[0] = '\0';
        return buffer;
    }

    strlcpy(buffer, pack->olp_format, buffer_size);
    return buffer;
}

char* os_log_pack_send_and_compose(os_log_pack_t pack,
    os_log_t log,
    os_log_type_t type,
    char* buffer,
    size_t buffer_size)
{
    os_log_pack_send(pack, log, type);
    return os_log_pack_compose(pack, log, type, buffer, buffer_size);
}

void os_log_pack_send(os_log_pack_t pack,
    os_log_t log,
    os_log_type_t type)
{
    if (pack != NULL)
        pd_os_log_send_message(log, type, pack->olp_format);
}

void __os_log_impl(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    struct pd_os_log_args args;

    (void)dso;
    pd_os_log_packed(&args, buffer, buffer_size, errno);
    pd_os_log_send(log, type, format, &args);
}

/*
 * The symbols <os/log.h>'s os_log_debug()/os_log_error() macros actually emit
 * carry a single leading underscore; the double-underscore variants below are
 * the ones this file already provided. Both spellings appear in the wild
 * depending on which SDK a caller was compiled against, so provide each.
 */
void _os_log_debug_impl(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    __os_log_impl(dso, log, type, format, buffer, buffer_size);
}

void _os_log_impl(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    __os_log_impl(dso, log, type, format, buffer, buffer_size);
}

void _os_log_fault_impl(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    __os_log_impl(dso, log, type, format, buffer, buffer_size);
}

void _os_log_error_impl(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    __os_log_impl(dso, log, type, format, buffer, buffer_size);
}

void __os_log_error_impl(void* dso,
    os_log_t log,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    __os_log_impl(dso, log, OS_LOG_TYPE_ERROR, format, buffer, buffer_size);
}

void __os_log_fault_impl(void* dso,
    os_log_t log,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    __os_log_impl(dso, log, OS_LOG_TYPE_FAULT, format, buffer, buffer_size);
}

size_t _os_log_pack_init(void* pack,
    size_t pack_size,
    int saved_errno,
    const void* dso,
    const char* format,
    va_list args)
{

}

size_t _os_trace_encode(void* buffer,
    size_t buffer_size,
    const char* format,
    va_list args,
    uint32_t flags)
{
    return os_log_encode(buffer, buffer_size, format, args, flags);
}

void _os_trace_encode_and_send(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* format,
    va_list args,
    uint32_t flags)
{
    struct pd_os_log_args a = { &args, NULL, NULL, false, errno };

    (void)dso; (void)flags;
    pd_os_log_send(log, type, format, &a);
}

void _os_trace_debug(const char* message)
{
    pd_os_log_send_message(OS_LOG_DEFAULT, OS_LOG_TYPE_DEBUG, message);
}

void _os_trace_error(const char* message)
{
    pd_os_log_send_message(OS_LOG_DEFAULT, OS_LOG_TYPE_ERROR, message);
}

void _os_log_simple(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* message)
{
    (void)dso;
    pd_os_log_send_message(log, type, message);
}

void _os_log_simple_impl(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* message)
{
    (void)dso;
    pd_os_log_send_message(log, type, message);
}

void _os_log_error(void* dso, os_log_t log, const char* format, ...)
{
    struct pd_os_log_args a = { NULL, NULL, NULL, false, errno };
    va_list ap;

    (void)dso;
    va_start(ap, format);
    a.ap = &ap;
    pd_os_log_send(log, OS_LOG_TYPE_ERROR, format, &a);
    va_end(ap);
}

void _os_log_fault(void* dso, os_log_t log, const char* format, ...)
{
    struct pd_os_log_args a = { NULL, NULL, NULL, false, errno };
    va_list ap;

    (void)dso;
    va_start(ap, format);
    a.ap = &ap;
    pd_os_log_send(log, OS_LOG_TYPE_FAULT, format, &a);
    va_end(ap);
}

void _os_log_info(void* dso, os_log_t log, const char* format, ...)
{
    struct pd_os_log_args a = { NULL, NULL, NULL, false, errno };
    va_list ap;

    (void)dso;
    va_start(ap, format);
    a.ap = &ap;
    pd_os_log_send(log, OS_LOG_TYPE_INFO, format, &a);
    va_end(ap);
}

void _os_log_debug(void* dso, os_log_t log, const char* format, ...)
{
    struct pd_os_log_args a = { NULL, NULL, NULL, false, errno };
    va_list ap;

    (void)dso;
    va_start(ap, format);
    a.ap = &ap;
    pd_os_log_send(log, OS_LOG_TYPE_DEBUG, format, &a);
    va_end(ap);
}

void _os_log_set_mode(uint32_t mode)
{
}

uint32_t _os_log_get_mode(void)
{
    return 0; // FIXME: what should this return?
}

void _os_log_preferences_refresh(void)
{
}

// Heuristic: treat small or obviously sensitive data as private
static bool
isLikelyPrivateData(const UInt8 *bytes, CFIndex len)
{
    if (len == 0 || len > 4096) return false;

    // Simple heuristic: check for ASCII ranges typical of passwords/tokens
    int printable = 0;
    for (CFIndex i = 0; i < len; i++) {
        if (bytes[i] >= 32 && bytes[i] <= 126)
            printable++;
    }

    // If mostly printable and short -> likely a secret
    if (len <= 64 && printable > (len * 0.8))
        return true;

    return false;
}

bool
_NSCF2data(const void *obj, char *string_value, size_t string_sz, bool *is_private)
{
    if (!string_value || string_sz == 0)
        return false;

    string_value[0] = '\0';
    if (is_private) *is_private = false;

    // Check if this is toll-free bridged CFData/NSData
    /* CoreFoundation is resolved lazily (see init.h); if it never loaded, or
     * this is not a CFData, there is nothing to decode. */
    if (!obj || !_CFGetTypeID || !_CFDataGetTypeID || !_CFDataGetLength ||
        !_CFDataGetBytePtr ||
        _CFGetTypeID(obj) != _CFDataGetTypeID()) {
        snprintf(string_value, string_sz, "<not CFData>");
        return false;
    }

    CFDataRef data = (CFDataRef)obj;
    CFIndex len = _CFDataGetLength(data);
    const UInt8 *bytes = _CFDataGetBytePtr(data);

    if (!bytes) {
        snprintf(string_value, string_sz, "<null bytes>");
        return false;
    }

    // Determine privacy
    bool privateFlag = isLikelyPrivateData(bytes, len);
    if (is_private) *is_private = privateFlag;

    if (privateFlag) {
        // Redacted output similar to Apple logging
        snprintf(string_value, string_sz,
                 "<NSData %ld bytes: private>", (long)len);
        return true;
    }

    // Non-private: produce a hex dump (truncated to fit buffer)
    size_t maxBytes = (string_sz - 1) / 2; // 2 chars per byte
    if (maxBytes > (size_t)len) maxBytes = len;

    char *p = string_value;
    for (size_t i = 0; i < maxBytes; i++) {
        snprintf(p, 3, "%02X", bytes[i]);
        p += 2;
    }

    // If truncated, append ellipsis
    if (maxBytes < (size_t)len && (p - string_value + 4) < string_sz) {
        strcpy(p, "...");
    }

    return true;
}

bool _os_log_string_is_public(const char* str)
{
    return true; // FIXME
}
