/* SPDX-License-Identifier: Apache-2.0 */

#include <corecrypto/ccn.h>
#include <string.h>

cc_unit ccn_add1(cc_size n, cc_unit *r, const cc_unit *s, cc_unit v)
{
    cc_unit carry = v;
    for (cc_size i = 0; i < n; i++) {
        cc_unit sum = s[i] + carry;
        carry = sum < carry;
        r[i] = sum;
    }
    return carry;
}

int ccn_cmpn(cc_size ns, const cc_unit *s, cc_size nt, const cc_unit *t)
{
    if (ns > nt) {
        if (ccn_n(ns - nt, s + nt) != 0)
            return 1;
        ns = nt;
    } else if (nt > ns) {
        if (ccn_n(nt - ns, t + ns) != 0)
            return -1;
    }
    return ccn_cmp(ns, s, t);
}

void ccn_seti(cc_size n, cc_unit *r, cc_unit v)
{
    if (n == 0)
        return;
    ccn_zero(n, r);
    r[0] = v;
}

static uint8_t ccn_byte(const cc_unit *s, size_t i)
{
    return (uint8_t)(s[i / CC_UNIT_SIZE] >> (8 * (i % CC_UNIT_SIZE)));
}

size_t ccn_write_uint_size(cc_size n, const cc_unit *s)
{
    return (ccn_bitlen(n, s) + 7) / 8;
}

void ccn_write_uint(cc_size n, const cc_unit *s, size_t out_size, void *out)
{
    uint8_t *o = out;
    size_t len = ccn_write_uint_size(n, s);
    size_t count = out_size < len ? out_size : len;

    for (size_t i = 0; i < count; i++)
        o[i] = ccn_byte(s, len - 1 - i);
    memset(o + count, 0, out_size - count);
}

size_t ccn_write_int_size(cc_size n, const cc_unit *s)
{
    size_t len = ccn_write_uint_size(n, s);
    if (len == 0)
        return 1;
    return (ccn_byte(s, len - 1) & 0x80) ? len + 1 : len;
}

void ccn_write_int(cc_size n, const cc_unit *s, size_t out_size, void *out)
{
    uint8_t *o = out;
    size_t len = ccn_write_int_size(n, s);
    size_t count = out_size < len ? out_size : len;

    for (size_t i = 0; i < count; i++) {
        size_t pos = len - 1 - i;
        o[i] = pos < ccn_sizeof_n(n) ? ccn_byte(s, pos) : 0;
    }
    memset(o + count, 0, out_size - count);
}
