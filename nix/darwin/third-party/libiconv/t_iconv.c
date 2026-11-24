/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <iconv.h>
#include <localcharset.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"

static void
hex(const char *tag, const char *p, size_t n)
{
    printf("%s", tag);
    for (size_t i = 0; i < n; i++) {
        printf("%02x", (unsigned char)p[i]);
    }
    printf("\n");
}

static size_t
convert(const char *to, const char *from, const char *in, size_t inlen, size_t outsize)
{
    char out[4096];
    iconv_t cd = iconv_open(to, from);
    printf("%s <- %s:", to, from);
    if (cd == (iconv_t)-1) {
        printf(" open errno %d\n", errno);
        return (size_t)-2;
    }
    char *ip = (char *)in, *op = out;
    size_t il = inlen, ol = outsize < sizeof(out) ? outsize : sizeof(out);
    errno = 0;
    size_t r = iconv(cd, &ip, &il, &op, &ol);
    int e = r == (size_t)-1 ? errno : 0;
    size_t f = iconv(cd, NULL, NULL, &op, &ol);
    printf(" ret %ld errno %d consumed %zu flush %ld ", (long)r, e, inlen - il, (long)f);
    hex("out ", out, (size_t)(op - out));
    iconv_close(cd);
    return r;
}

#define CONV(to, from, s) convert(to, from, s, sizeof(s) - 1, 4096)

static int
list_names(unsigned int count, const char *const *names, void *arg)
{
    unsigned *total = arg;
    *total += count;
    return 0;
}

static void
check_iconvctl(void)
{
    iconv_t cd = iconv_open("UTF-8", "UTF-8");
    int v = -1;
    CHECK(cd != (iconv_t)-1);
    CHECK(iconvctl(cd, ICONV_TRIVIALP, &v) == 0);
    printf("trivial UTF-8 <- UTF-8 %d\n", v);
    iconv_close(cd);

    cd = iconv_open("ISO-8859-1", "UTF-8");
    CHECK(iconvctl(cd, ICONV_TRIVIALP, &v) == 0);
    printf("trivial ISO-8859-1 <- UTF-8 %d\n", v);
    CHECK(iconvctl(cd, ICONV_GET_TRANSLITERATE, &v) == 0);
    printf("transliterate default %d\n", v);
    v = 1;
    CHECK(iconvctl(cd, ICONV_SET_TRANSLITERATE, &v) == 0);
    CHECK(iconvctl(cd, ICONV_GET_TRANSLITERATE, &v) == 0);
    printf("transliterate set %d\n", v);
    CHECK(iconvctl(cd, ICONV_GET_DISCARD_ILSEQ, &v) == 0);
    printf("discard default %d\n", v);
    v = 1;
    CHECK(iconvctl(cd, ICONV_SET_DISCARD_ILSEQ, &v) == 0);
    char in[] = "a\xe2\x82\xac" "b", out[16], *ip = in, *op = out;
    size_t il = sizeof(in) - 1, ol = sizeof(out);
    errno = 0;
    size_t r = iconv(cd, &ip, &il, &op, &ol);
    printf("discard ret %ld errno %d ", (long)r, r == (size_t)-1 ? errno : 0);
    hex("out ", out, (size_t)(op - out));
    printf("iconvctl unknown request %d errno %d\n", iconvctl(cd, 999, &v), errno);
    iconv_close(cd);
}

static void
check_lists(void)
{
    unsigned total = 0;
    iconvlist(list_names, &total);
    printf("iconvlist names %u\n", total);
    CHECK(total > 100);

    char **names = NULL;
    size_t count = 0;
    CHECK(__iconv_get_list(&names, &count, 1) == 0);
    printf("__iconv_get_list %zu\n", count);
    size_t utf = 0;
    for (size_t i = 0; i < count; i++) {
        if (strncmp(names[i], "UTF-", 4) == 0) {
            utf++;
        }
    }
    printf("__iconv_get_list UTF- %zu\n", utf);
    __iconv_free_list(names, count);

    static const char *aliases[] = { "latin1", "UTF8", "utf-8", "SJIS", "eucJP", "ISO_8859-1:1987", "CP1252",
                                     "MACROMAN", "ASCII", "US-ASCII", "UCS-2", "UNICODELITTLE", "no-such", NULL };
    for (int i = 0; aliases[i] != NULL; i++) {
        const char *c = iconv_canonicalize(aliases[i]);
        printf("canonicalize %s %s\n", aliases[i], c ? c : "NULL");
    }
}

static void
check_low_level(void)
{
    iconv_t cd = iconv_open("ISO-8859-1", "UTF-8");
    char in[] = "x\xe2\x82\xac" "y", out[16], *ip = in, *op = out;
    size_t il = sizeof(in) - 1, ol = sizeof(out), invalids = 0;
    size_t r = __iconv(cd, &ip, &il, &op, &ol, __ICONV_F_HIDE_INVALID, &invalids);
    printf("__iconv hide-invalid ret %ld invalids %zu ", (long)r, invalids);
    hex("out ", out, (size_t)(op - out));
    iconv_close(cd);

    iconv_allocation_t space;
    iconv_t into = (iconv_t)&space;
    CHECK(iconv_open_into("UTF-16BE", "UTF-8", &space) == 0);
    char in2[] = "\xc3\xa9", out2[8], *ip2 = in2, *op2 = out2;
    size_t il2 = 2, ol2 = sizeof(out2);
    CHECK(iconv(into, &ip2, &il2, &op2, &ol2) == 0);
    hex("open_into ", out2, (size_t)(op2 - out2));
}

int
main(void)
{
    printf("_libiconv_version %#x\n", _libiconv_version);
    CHECK(CONV("ISO-8859-1", "UTF-8", "caf\xc3\xa9") == 0);
    CONV("ASCII", "UTF-8", "caf\xc3\xa9");
    CONV("ASCII//TRANSLIT", "UTF-8", "caf\xc3\xa9 \xe2\x82\xac \xc3\x9f \xe2\x80\x9cq\xe2\x80\x9d \xef\xac\x81");
    CONV("ASCII//IGNORE", "UTF-8", "caf\xc3\xa9!");
    CONV("ASCII//TRANSLIT//IGNORE", "UTF-8", "\xe2\x98\x83 snow");
    CONV("ISO-8859-1//TRANSLIT", "UTF-8", "\xe2\x82\xac\xe2\x80\x94\xc5\x93");
    CONV("UTF-16", "UTF-8", "A\xc3\xa9\xf0\x9f\x98\x80");
    CONV("UTF-16LE", "UTF-8", "A\xc3\xa9\xf0\x9f\x98\x80");
    CONV("UTF-8", "UTF-16", "\xfe\xff\x00\x41\xd8\x3d\xde\x00");
    CONV("UTF-8", "UTF-16", "\xff\xfe\x41\x00\x3d\xd8\x00\xde");
    CONV("UTF-8", "UTF-16BE", "\xd8\x3d\x00\x41");
    CONV("UCS-4BE", "UTF-8", "\xf0\x9f\x98\x80");
    CONV("UTF-32", "UTF-8", "Z");
    CONV("UCS-2", "UTF-8", "\xf0\x9f\x98\x80");
    CONV("UTF-7", "UTF-8", "a+b \xc3\xa9\xe2\x82\xac");
    CONV("UTF-8", "UTF-7", "Hi Mom -+Jjo--!");
    CONV("SHIFT_JIS", "UTF-8", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xef\xbd\xb1");
    CONV("EUC-JP", "UTF-8", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");
    CONV("ISO-2022-JP", "UTF-8", "a\xe6\x97\xa5\xe6\x9c\xac" "b");
    CONV("UTF-8", "ISO-2022-JP", "\x1b$B" "F|K\\" "\x1b(B" "x");
    CHECK(CONV("ISO-2022-KR", "UTF-8", "a\xea\xb0\x80\xeb\x82\x98" "b") == 0);
    CONV("UTF-8", "ISO-2022-KR", "\x1b$)C" "a\x0e" "0!3*" "\x0f" "b");
    CHECK(CONV("ISO-2022-CN", "UTF-8", "\xe4\xb8\x80" "b") == 0);
    CHECK(CONV("ISO-2022-CN-EXT", "UTF-8", "\xe3\x90\x80") == 0);
    CONV("CP932", "UTF-8", "\xe2\x91\xa0\xe3\x88\xb1");
    CONV("GB18030", "UTF-8", "\xe4\xb8\xad\xf0\x9f\x98\x80");
    CONV("BIG5", "UTF-8", "\xe4\xb8\xad\xe6\x96\x87");
    CONV("EUC-KR", "UTF-8", "\xed\x95\x9c");
    CONV("HZ", "UTF-8", "a\xe4\xb8\xad" "b");
    CONV("CP1252", "UTF-8", "\xe2\x82\xac\xe2\x80\x9c");
    CONV("MACINTOSH", "UTF-8", "\xc3\xa9\xef\xa3\xbf");
    CONV("KOI8-R", "UTF-8", "\xd0\x9f\xd1\x80\xd0\xb8");
    CONV("UTF-8-MAC", "UTF-8", "\xc3\xa9t\xc3\xa9");
    CONV("UTF-8", "UTF-8-MAC", "e\xcc\x81t\xc3\xa9");
    CONV("IBM037", "UTF-8", "Hello");
    CONV("ISO-8859-7", "UTF-8", "\xce\xb1\xce\xb2");
    CONV("UTF-8", "UTF-8", "\xc3\x28");
    CONV("UTF-8", "UTF-8", "ok\xe2\x82");
    CONV("UTF-8", "ISO-8859-1", "\xff\xfe");
    CONV("WCHAR_T", "UTF-8", "\xc3\xa9");
    CONV("UTF-8", "no-such-charset", "x");
    convert("UTF-16LE", "UTF-8", "abcdef", 6, 5);
    convert("UTF-8", "UTF-8", "abc\xc3\xa9", 5, 4);
    check_iconvctl();
    check_lists();
    check_low_level();
    printf("locale_charset C %s\n", locale_charset());
    return checks_done("iconv");
}
