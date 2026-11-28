/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 The kido authors
 *
 * Clean-room /usr/bin/sw_vers reading /System/Library/CoreServices/SystemVersion.plist
 * with zero external library dependencies beyond libSystem.
 */

#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEFAULT_PLIST_PATH "/System/Library/CoreServices/SystemVersion.plist"
#define COMPAT_PLIST_PATH  "/System/Library/CoreServices/SystemVersionCompat.plist"

#define FALLBACK_PRODUCT_NAME    "KidoDarwin"
#define FALLBACK_PRODUCT_VERSION "26.5"
#define FALLBACK_BUILD_VERSION   "25F74"

#define MAX_PLIST_BYTES (256u * 1024u)
#define FIELD_MAX       256u

struct sw_vers_info {
    char product_name[FIELD_MAX];
    char product_version[FIELD_MAX];
    char product_version_extra[FIELD_MAX];
    char build_version[FIELD_MAX];
};

static void
print_usage(void)
{
    printf("Usage: sw_vers [--help|--productName|--productVersion|"
           "--productVersionExtra|--buildVersion]\n");
}

static char *
read_file_bounded(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }

    struct stat st;
    if (fstat(fileno(f), &st) != 0 || !S_ISREG(st.st_mode)) {
        fclose(f);
        return NULL;
    }

    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }

    for (;;) {
        if (cap - len < 1024) {
            if (cap >= MAX_PLIST_BYTES + 1) {
                free(buf);
                fclose(f);
                return NULL;
            }
            size_t next_cap = cap * 2;
            if (next_cap > MAX_PLIST_BYTES + 1) {
                next_cap = MAX_PLIST_BYTES + 1;
            }
            char *grown = realloc(buf, next_cap);
            if (grown == NULL) {
                free(buf);
                fclose(f);
                return NULL;
            }
            buf = grown;
            cap = next_cap;
        }
        size_t avail = (MAX_PLIST_BYTES + 1) - len;
        if (avail > cap - len) {
            avail = cap - len;
        }
        size_t got = fread(buf + len, 1, avail, f);
        len += got;
        if (len > MAX_PLIST_BYTES) {
            free(buf);
            fclose(f);
            return NULL;
        }
        if (got < avail) {
            if (ferror(f)) {
                free(buf);
                fclose(f);
                return NULL;
            }
            break;
        }
    }

    fclose(f);
    if (memchr(buf, '\0', len) != NULL) {
        free(buf);
        return NULL;
    }
    buf[len] = '\0';
    return buf;
}

static bool
append_utf8(uint32_t cp, char *dst, size_t *pos, size_t dst_size)
{
    if (cp == 0 || cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        return true;
    }
    size_t need;
    if (cp <= 0x7Fu) {
        need = 1;
    } else if (cp <= 0x7FFu) {
        need = 2;
    } else if (cp <= 0xFFFFu) {
        need = 3;
    } else {
        need = 4;
    }
    if (*pos + need >= dst_size) {
        return false;
    }
    if (need == 1) {
        dst[(*pos)++] = (char)cp;
    } else if (need == 2) {
        dst[(*pos)++] = (char)(0xC0u | (cp >> 6));
        dst[(*pos)++] = (char)(0x80u | (cp & 0x3Fu));
    } else if (need == 3) {
        dst[(*pos)++] = (char)(0xE0u | (cp >> 12));
        dst[(*pos)++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        dst[(*pos)++] = (char)(0x80u | (cp & 0x3Fu));
    } else {
        dst[(*pos)++] = (char)(0xF0u | (cp >> 18));
        dst[(*pos)++] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        dst[(*pos)++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        dst[(*pos)++] = (char)(0x80u | (cp & 0x3Fu));
    }
    return true;
}

static bool
append_raw_bytes(const char *src, size_t len, char *dst, size_t *pos, size_t dst_size)
{
    size_t i = 0;
    while (i < len) {
        unsigned char lead = (unsigned char)src[i];
        size_t seq = 1;
        if ((lead & 0xE0u) == 0xC0u) {
            seq = 2;
        } else if ((lead & 0xF0u) == 0xE0u) {
            seq = 3;
        } else if ((lead & 0xF8u) == 0xF0u) {
            seq = 4;
        }
        if (i + seq > len) {
            seq = 1;
        }
        if (*pos + seq >= dst_size) {
            return false;
        }
        memcpy(dst + *pos, src + i, seq);
        *pos += seq;
        i += seq;
    }
    return true;
}

static bool
xml_decode_append(const char *src, size_t len, char *dst, size_t *out, size_t dst_size)
{
    if (dst_size == 0) {
        return false;
    }
    size_t i = 0;
    while (i < len && *out + 1 < dst_size) {
        if (src[i] == '&') {
            const char *semi = memchr(src + i + 1, ';', len - (i + 1));
            if (semi != NULL) {
                size_t elen = (size_t)(semi - (src + i + 1));
                const char *ent = src + i + 1;
                if (elen == 3 && memcmp(ent, "amp", 3) == 0) {
                    dst[(*out)++] = '&';
                    i = (size_t)(semi - src) + 1;
                    continue;
                }
                if (elen == 2 && memcmp(ent, "lt", 2) == 0) {
                    dst[(*out)++] = '<';
                    i = (size_t)(semi - src) + 1;
                    continue;
                }
                if (elen == 2 && memcmp(ent, "gt", 2) == 0) {
                    dst[(*out)++] = '>';
                    i = (size_t)(semi - src) + 1;
                    continue;
                }
                if (elen == 4 && memcmp(ent, "quot", 4) == 0) {
                    dst[(*out)++] = '"';
                    i = (size_t)(semi - src) + 1;
                    continue;
                }
                if (elen == 4 && memcmp(ent, "apos", 4) == 0) {
                    dst[(*out)++] = '\'';
                    i = (size_t)(semi - src) + 1;
                    continue;
                }
                if (elen >= 2 && ent[0] == '#') {
                    uint32_t cp = 0;
                    bool ok = true;
                    size_t k = 1;
                    int base = 10;
                    /* XML 1.0 [66] allows lowercase 'x' only for hex char refs. */
                    if (ent[1] == 'x') {
                        base = 16;
                        k = 2;
                    }
                    if (k >= elen) {
                        ok = false;
                    }
                    for (; ok && k < elen; k++) {
                        unsigned char c = (unsigned char)ent[k];
                        uint32_t d;
                        if (c >= '0' && c <= '9') {
                            d = (uint32_t)(c - '0');
                        } else if (base == 16 && c >= 'a' && c <= 'f') {
                            d = (uint32_t)(10 + c - 'a');
                        } else if (base == 16 && c >= 'A' && c <= 'F') {
                            d = (uint32_t)(10 + c - 'A');
                        } else {
                            ok = false;
                            break;
                        }
                        if (cp > 0x10FFFFu) {
                            ok = false;
                            break;
                        }
                        cp = cp * (uint32_t)base + d;
                    }
                    if (ok && cp != 0 && cp <= 0x10FFFFu &&
                        !(cp >= 0xD800u && cp <= 0xDFFFu)) {
                        if (!append_utf8(cp, dst, out, dst_size)) {
                            dst[*out] = '\0';
                            return false;
                        }
                        i = (size_t)(semi - src) + 1;
                        continue;
                    }
                }
            }
        }
        /* Copy one UTF-8 character without splitting multi-byte sequences. */
        unsigned char lead = (unsigned char)src[i];
        size_t seq = 1;
        if ((lead & 0xE0u) == 0xC0u) {
            seq = 2;
        } else if ((lead & 0xF0u) == 0xE0u) {
            seq = 3;
        } else if ((lead & 0xF8u) == 0xF0u) {
            seq = 4;
        }
        if (i + seq > len) {
            seq = 1;
        }
        if (*out + seq >= dst_size) {
            dst[*out] = '\0';
            return false;
        }
        memcpy(dst + *out, src + i, seq);
        *out += seq;
        i += seq;
    }
    dst[*out] = '\0';
    return true;
}

static const char *
skip_ws_and_prolog(const char *p)
{
    for (;;) {
        while (*p != '\0' && isspace((unsigned char)*p)) {
            p++;
        }
        if (strncmp(p, "<!--", 4) == 0) {
            const char *end = strstr(p + 4, "-->");
            if (end == NULL) {
                return p + strlen(p);
            }
            p = end + 3;
            continue;
        }
        if (strncmp(p, "<?", 2) == 0) {
            const char *end = strstr(p + 2, "?>");
            if (end == NULL) {
                return p + strlen(p);
            }
            p = end + 2;
            continue;
        }
        if (strncmp(p, "<![CDATA[", 9) == 0) {
            const char *end = strstr(p + 9, "]]>");
            if (end == NULL) {
                return p + strlen(p);
            }
            p = end + 3;
            continue;
        }
        if (strncmp(p, "<!", 2) == 0) {
            /* Skip DOCTYPE declarations including optional [...] internal subsets. */
            const char *q = p + 2;
            char quote = '\0';
            int bracket_depth = 0;
            while (*q != '\0') {
                if (quote != '\0') {
                    if (*q == quote) {
                        quote = '\0';
                    }
                } else if (*q == '"' || *q == '\'') {
                    quote = *q;
                } else if (*q == '[') {
                    bracket_depth++;
                } else if (*q == ']' && bracket_depth > 0) {
                    bracket_depth--;
                } else if (*q == '>' && bracket_depth == 0) {
                    p = q + 1;
                    break;
                }
                q++;
            }
            if (*q == '\0') {
                return q;
            }
            continue;
        }
        return p;
    }
}

static const char *
find_tag_end(const char *p)
{
    char quote = '\0';
    while (*p != '\0') {
        if (quote != '\0') {
            if (*p == quote) {
                quote = '\0';
            }
        } else if (*p == '"' || *p == '\'') {
            quote = *p;
        } else if (*p == '>') {
            return p;
        }
        p++;
    }
    return NULL;
}

static bool
match_open_tag(const char *p, const char *name, bool *self_closing, const char **after)
{
    size_t nlen = strlen(name);
    if (*p != '<' || strncmp(p + 1, name, nlen) != 0) {
        return false;
    }
    char sep = p[1 + nlen];
    if (sep != '>' && sep != '/' && !isspace((unsigned char)sep)) {
        return false;
    }
    const char *gt = find_tag_end(p + 1 + nlen);
    if (gt == NULL) {
        return false;
    }
    const char *prev = gt - 1;
    while (prev > p && isspace((unsigned char)*prev)) {
        prev--;
    }
    *self_closing = (*prev == '/');
    *after = gt + 1;
    return true;
}

static bool
match_close_tag(const char *p, const char *name, const char **after)
{
    size_t nlen = strlen(name);
    if (p[0] != '<' || p[1] != '/' || strncmp(p + 2, name, nlen) != 0) {
        return false;
    }
    const char *q = p + 2 + nlen;
    while (*q != '\0' && isspace((unsigned char)*q)) {
        q++;
    }
    if (*q != '>') {
        return false;
    }
    *after = q + 1;
    return true;
}

/*
 * Read the text content of a simple element (<key> or <string>) up to its
 * matching closing tag (allowing XML 1.0 whitespace before '>'), handling
 * inline <!-- comments --> and <![CDATA[...]]> sections.
 */
static bool
read_element_text(const char *p, const char *tag_name, char *dst, size_t dst_size,
                  const char **after)
{
    size_t out = 0;
    bool full = false;
    if (dst_size > 0) {
        dst[0] = '\0';
    }

    while (*p != '\0') {
        if (*p == '<') {
            if (match_close_tag(p, tag_name, after)) {
                dst[out] = '\0';
                return true;
            }
            if (strncmp(p, "<!--", 4) == 0) {
                const char *end = strstr(p + 4, "-->");
                if (end == NULL) {
                    return false;
                }
                p = end + 3;
                continue;
            }
            if (strncmp(p, "<![CDATA[", 9) == 0) {
                const char *end = strstr(p + 9, "]]>");
                if (end == NULL) {
                    return false;
                }
                if (!full) {
                    if (!append_raw_bytes(p + 9, (size_t)(end - (p + 9)), dst, &out, dst_size)) {
                        full = true;
                    }
                }
                p = end + 3;
                continue;
            }
            return false;
        }
        const char *next_lt = strchr(p, '<');
        if (next_lt == NULL) {
            return false;
        }
        if (!full) {
            if (!xml_decode_append(p, (size_t)(next_lt - p), dst, &out, dst_size)) {
                full = true;
            }
        }
        p = next_lt;
    }
    return false;
}

static void
assign_field(struct sw_vers_info *info, const char *key, const char *val)
{
    char *dst = NULL;
    if (strcmp(key, "ProductName") == 0) {
        dst = info->product_name;
    } else if (strcmp(key, "ProductVersion") == 0) {
        dst = info->product_version;
    } else if (strcmp(key, "ProductVersionExtra") == 0) {
        dst = info->product_version_extra;
    } else if (strcmp(key, "ProductBuildVersion") == 0) {
        dst = info->build_version;
    }
    if (dst != NULL) {
        snprintf(dst, FIELD_MAX, "%s", val);
    }
}

static void
parse_plist(const char *xml, struct sw_vers_info *info)
{
    const char *p = xml;
    bool self_closing = false;
    const char *after = NULL;

    /* Advance to the root <dict> element inside <plist>. */
    while (*p != '\0') {
        p = skip_ws_and_prolog(p);
        if (*p == '\0') {
            return;
        }
        if (*p != '<') {
            p++;
            continue;
        }
        if (match_open_tag(p, "dict", &self_closing, &after)) {
            if (self_closing) {
                return;
            }
            p = after;
            break;
        }
        const char *gt = find_tag_end(p + 1);
        if (gt == NULL) {
            return;
        }
        p = gt + 1;
    }

    int dict_depth = 1;
    int array_depth = 0;
    bool have_key = false;
    char cur_key[FIELD_MAX];
    cur_key[0] = '\0';

    while (*p != '\0' && dict_depth > 0) {
        p = skip_ws_and_prolog(p);
        if (*p == '\0') {
            break;
        }
        if (*p != '<') {
            while (*p != '\0' && *p != '<') {
                p++;
            }
            continue;
        }

        if (match_close_tag(p, "dict", &after)) {
            dict_depth--;
            have_key = false;
            p = after;
            continue;
        }
        if (match_close_tag(p, "array", &after)) {
            if (array_depth > 0) {
                array_depth--;
            }
            have_key = false;
            p = after;
            continue;
        }
        if (match_open_tag(p, "dict", &self_closing, &after)) {
            if (!self_closing) {
                dict_depth++;
            }
            have_key = false;
            p = after;
            continue;
        }
        if (match_open_tag(p, "array", &self_closing, &after)) {
            if (!self_closing) {
                array_depth++;
            }
            have_key = false;
            p = after;
            continue;
        }

        if (dict_depth == 1 && array_depth == 0) {
            if (match_open_tag(p, "key", &self_closing, &after)) {
                if (self_closing) {
                    cur_key[0] = '\0';
                    have_key = true;
                    p = after;
                } else {
                    if (!read_element_text(after, "key", cur_key, sizeof(cur_key), &p)) {
                        break;
                    }
                    have_key = true;
                }
                continue;
            }
            if (have_key && match_open_tag(p, "string", &self_closing, &after)) {
                char val[FIELD_MAX];
                if (self_closing) {
                    val[0] = '\0';
                    p = after;
                } else {
                    if (!read_element_text(after, "string", val, sizeof(val), &p)) {
                        break;
                    }
                }
                assign_field(info, cur_key, val);
                have_key = false;
                continue;
            }
            /* Any non-string scalar value (<true/>, <integer>, etc.) consumes the key. */
            if (p[1] != '/') {
                const char *gt = find_tag_end(p + 1);
                if (gt == NULL) {
                    break;
                }
                const char *prev = gt - 1;
                while (prev > p && isspace((unsigned char)*prev)) {
                    prev--;
                }
                bool sc = (*prev == '/');
                if (!sc) {
                    const char *name_start = p + 1;
                    const char *name_end = name_start;
                    while (name_end < gt && !isspace((unsigned char)*name_end) &&
                           *name_end != '/') {
                        name_end++;
                    }
                    size_t nlen = (size_t)(name_end - name_start);
                    if (nlen > 0 && nlen < 32) {
                        char tag_buf[32];
                        memcpy(tag_buf, name_start, nlen);
                        tag_buf[nlen] = '\0';
                        char discard[16];
                        const char *next_p = NULL;
                        if (read_element_text(gt + 1, tag_buf, discard, sizeof(discard), &next_p)) {
                            p = next_p;
                        } else {
                            p = gt + 1;
                        }
                    } else {
                        p = gt + 1;
                    }
                } else {
                    p = gt + 1;
                }
                have_key = false;
                continue;
            }
        }

        const char *gt = find_tag_end(p + 1);
        if (gt == NULL) {
            break;
        }
        p = gt + 1;
    }
}

static void
load_system_version(struct sw_vers_info *info)
{
    memset(info, 0, sizeof(*info));

    char *xml = NULL;
#ifdef SW_VERS_ALLOW_PLIST_OVERRIDE
    if (issetugid() == 0) {
        const char *override = getenv("SW_VERS_PLIST_PATH");
        if (override != NULL && override[0] != '\0') {
            xml = read_file_bounded(override);
            goto parsed;
        }
    }
#endif
    {
        const char *compat = getenv("SYSTEM_VERSION_COMPAT");
        if (compat != NULL && strcmp(compat, "1") == 0) {
            xml = read_file_bounded(COMPAT_PLIST_PATH);
        }
        if (xml == NULL) {
            xml = read_file_bounded(DEFAULT_PLIST_PATH);
        }
    }

#ifdef SW_VERS_ALLOW_PLIST_OVERRIDE
parsed:
#endif
    if (xml != NULL) {
        parse_plist(xml, info);
        free(xml);
    }

    if (info->product_name[0] == '\0') {
        snprintf(info->product_name, sizeof(info->product_name), "%s", FALLBACK_PRODUCT_NAME);
    }
    if (info->product_version[0] == '\0') {
        snprintf(info->product_version, sizeof(info->product_version), "%s", FALLBACK_PRODUCT_VERSION);
    }
    if (info->build_version[0] == '\0') {
        snprintf(info->build_version, sizeof(info->build_version), "%s", FALLBACK_BUILD_VERSION);
    }
}

int
main(int argc, char **argv)
{
    static const struct option longopts[] = {
        { "help",                no_argument, NULL, 'h' },
        { "productName",         no_argument, NULL, 'n' },
        { "productVersion",      no_argument, NULL, 'v' },
        { "productVersionExtra", no_argument, NULL, 'e' },
        { "buildVersion",        no_argument, NULL, 'b' },
        { NULL, 0, NULL, 0 }
    };

    int mode = 0;
    int ch;

    while ((ch = getopt_long_only(argc, argv, "hnveb", longopts, NULL)) != -1) {
        switch (ch) {
        case 'h':
            print_usage();
            if (fflush(stdout) != 0 || ferror(stdout)) {
                return 1;
            }
            return 0;
        case 'n':
        case 'v':
        case 'e':
        case 'b':
            mode = ch;
            break;
        default:
            print_usage();
            return 1;
        }
    }

    struct sw_vers_info info;
    load_system_version(&info);

    switch (mode) {
    case 0:
        printf("ProductName:\t\t%s\n", info.product_name);
        printf("ProductVersion:\t\t%s\n", info.product_version);
        if (info.product_version_extra[0] != '\0') {
            printf("ProductVersionExtra:\t%s\n", info.product_version_extra);
        }
        printf("BuildVersion:\t\t%s\n", info.build_version);
        break;
    case 'n':
        printf("%s\n", info.product_name);
        break;
    case 'v':
        printf("%s\n", info.product_version);
        break;
    case 'e':
        if (info.product_version_extra[0] != '\0') {
            printf("%s\n", info.product_version_extra);
        }
        break;
    case 'b':
        printf("%s\n", info.build_version);
        break;
    }

    if (fflush(stdout) != 0 || ferror(stdout)) {
        return 1;
    }
    return 0;
}
