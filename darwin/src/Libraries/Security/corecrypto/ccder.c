/* SPDX-License-Identifier: Apache-2.0 */

#include <corecrypto/ccder.h>
#include <string.h>

size_t ccder_sizeof_tag(ccder_tag tag)
{
    (void)tag;
    return 1;
}

size_t ccder_sizeof_len(size_t len)
{
    if (len < 0x80)
        return 1;
    size_t size = 1;
    for (; len != 0; len >>= 8)
        size++;
    return size;
}

size_t ccder_sizeof(ccder_tag tag, size_t len)
{
    return ccder_sizeof_tag(tag) + ccder_sizeof_len(len) + len;
}

size_t ccder_sizeof_raw_octet_string(size_t s_size)
{
    return ccder_sizeof(CCDER_OCTET_STRING, s_size);
}

size_t ccder_sizeof_integer(cc_size n, const cc_unit *s)
{
    return ccder_sizeof(CCDER_INTEGER, ccn_write_int_size(n, s));
}

uint8_t *ccder_encode_body_nocopy(size_t size, const uint8_t *der, uint8_t *der_end)
{
    if (der == NULL || der_end == NULL || der_end < der || (size_t)(der_end - der) < size)
        return NULL;
    return der_end - size;
}

uint8_t *ccder_encode_body(size_t size, const uint8_t *body, const uint8_t *der, uint8_t *der_end)
{
    uint8_t *start = ccder_encode_body_nocopy(size, der, der_end);
    if (start != NULL && size != 0)
        memmove(start, body, size);
    return start;
}

uint8_t *ccder_encode_len(size_t len, const uint8_t *der, uint8_t *der_end)
{
    size_t size = ccder_sizeof_len(len);
    uint8_t *start = ccder_encode_body_nocopy(size, der, der_end);
    if (start == NULL)
        return NULL;
    if (size == 1) {
        start[0] = (uint8_t)len;
        return start;
    }
    start[0] = (uint8_t)(0x80 | (size - 1));
    for (size_t i = size - 1; i > 0; i--, len >>= 8)
        start[i] = (uint8_t)len;
    return start;
}

uint8_t *ccder_encode_tag(ccder_tag tag, const uint8_t *der, uint8_t *der_end)
{
    if (tag > 0xff || (tag & CCDER_TAGNUM_MASK) == CCDER_TAGNUM_MASK)
        return NULL;
    uint8_t *start = ccder_encode_body_nocopy(1, der, der_end);
    if (start != NULL)
        start[0] = (uint8_t)tag;
    return start;
}

uint8_t *ccder_encode_tl(ccder_tag tag, size_t len, const uint8_t *der, uint8_t *der_end)
{
    return ccder_encode_tag(tag, der, ccder_encode_len(len, der, der_end));
}

uint8_t *ccder_encode_constructed_tl(ccder_tag tag, const uint8_t *body_end,
                                     const uint8_t *der, uint8_t *der_end)
{
    if (der_end == NULL || body_end == NULL || body_end < der_end)
        return NULL;
    return ccder_encode_tl(tag, (size_t)(body_end - der_end), der, der_end);
}

uint8_t *ccder_encode_integer(cc_size n, const cc_unit *s, const uint8_t *der, uint8_t *der_end)
{
    size_t len = ccn_write_int_size(n, s);
    uint8_t *body = ccder_encode_body_nocopy(len, der, der_end);
    if (body == NULL)
        return NULL;
    ccn_write_int(n, s, len, body);
    return ccder_encode_tl(CCDER_INTEGER, len, der, body);
}

uint8_t *ccder_encode_raw_octet_string(size_t s_size, const uint8_t *s,
                                       const uint8_t *der, uint8_t *der_end)
{
    return ccder_encode_tl(CCDER_OCTET_STRING, s_size, der, ccder_encode_body(s_size, s, der, der_end));
}

const uint8_t *ccder_decode_tag(ccder_tag *tagp, const uint8_t *der, const uint8_t *der_end)
{
    if (der == NULL || der_end == NULL || der >= der_end)
        return NULL;
    if ((der[0] & CCDER_TAGNUM_MASK) == CCDER_TAGNUM_MASK)
        return NULL;
    if (tagp != NULL)
        *tagp = der[0];
    return der + 1;
}

const uint8_t *ccder_decode_len(size_t *lenp, const uint8_t *der, const uint8_t *der_end)
{
    if (der == NULL || der_end == NULL || der >= der_end)
        return NULL;
    size_t len = *der++;
    if (len >= 0x80) {
        size_t count = len & 0x7f;
        if (count == 0 || count > sizeof(size_t) || (size_t)(der_end - der) < count)
            return NULL;
        len = 0;
        while (count-- > 0)
            len = (len << 8) | *der++;
    }
    if (len > (size_t)(der_end - der))
        return NULL;
    if (lenp != NULL)
        *lenp = len;
    return der;
}

const uint8_t *ccder_decode_tl(ccder_tag expected_tag, size_t *lenp,
                               const uint8_t *der, const uint8_t *der_end)
{
    ccder_tag tag;
    der = ccder_decode_tag(&tag, der, der_end);
    if (der == NULL || tag != expected_tag)
        return NULL;
    return ccder_decode_len(lenp, der, der_end);
}

const uint8_t *ccder_decode_constructed_tl(ccder_tag expected_tag, const uint8_t **body_end,
                                           const uint8_t *der, const uint8_t *der_end)
{
    size_t len;
    der = ccder_decode_tl(expected_tag, &len, der, der_end);
    if (der != NULL && body_end != NULL)
        *body_end = der + len;
    return der;
}

const uint8_t *ccder_decode_sequence_tl(const uint8_t **body_end,
                                        const uint8_t *der, const uint8_t *der_end)
{
    return ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, body_end, der, der_end);
}

/* the magnitude bytes of a non-negative INTEGER */
static const uint8_t *ccder_decode_uint_body(const uint8_t **body, size_t *len,
                                             const uint8_t *der, const uint8_t *der_end)
{
    der = ccder_decode_tl(CCDER_INTEGER, len, der, der_end);
    if (der == NULL || *len == 0 || (der[0] & 0x80) != 0)
        return NULL;
    const uint8_t *end = der + *len;
    while (*len > 0 && *der == 0) {
        der++;
        (*len)--;
    }
    *body = der;
    return end;
}

const uint8_t *ccder_decode_uint_n(cc_size *n, const uint8_t *der, const uint8_t *der_end)
{
    const uint8_t *body;
    size_t len;
    der = ccder_decode_uint_body(&body, &len, der, der_end);
    if (der != NULL && n != NULL)
        *n = ccn_nof_size(len);
    return der;
}

const uint8_t *ccder_decode_uint(cc_size n, cc_unit *r, const uint8_t *der, const uint8_t *der_end)
{
    const uint8_t *body;
    size_t len;
    der = ccder_decode_uint_body(&body, &len, der, der_end);
    if (der == NULL || ccn_nof_size(len) > n || ccn_read_uint(n, r, len, body) != 0)
        return NULL;
    return der;
}

const uint8_t *ccder_decode_seqii(cc_size n, cc_unit *r, cc_unit *s,
                                  const uint8_t *der, const uint8_t *der_end)
{
    const uint8_t *body_end = NULL;
    der = ccder_decode_sequence_tl(&body_end, der, der_end);
    der = ccder_decode_uint(n, r, der, body_end);
    der = ccder_decode_uint(n, s, der, body_end);
    return (der != NULL && der == body_end) ? body_end : NULL;
}
