/* SPDX-License-Identifier: Apache-2.0 */

#ifndef KIDO_SECURITY_CORECRYPTO_CCDER_H
#define KIDO_SECURITY_CORECRYPTO_CCDER_H

#include <corecrypto/ccn.h>

CC_BEGIN_DECLS

/* a tag is its single identifier octet: class, constructed bit and a tag number below 31 */
typedef unsigned long ccder_tag;

enum {
    CCDER_EOL               = 0x00,
    CCDER_BOOLEAN           = 0x01,
    CCDER_INTEGER           = 0x02,
    CCDER_BIT_STRING        = 0x03,
    CCDER_OCTET_STRING      = 0x04,
    CCDER_NULL              = 0x05,
    CCDER_OBJECT_IDENTIFIER = 0x06,
    CCDER_REAL              = 0x09,
    CCDER_ENUMERATED        = 0x0a,
    CCDER_UTF8_STRING       = 0x0c,
    CCDER_SEQUENCE          = 0x10,
    CCDER_SET               = 0x11,
    CCDER_NUMERIC_STRING    = 0x12,
    CCDER_PRINTABLE_STRING  = 0x13,
    CCDER_T61_STRING        = 0x14,
    CCDER_IA5_STRING        = 0x16,
    CCDER_UTC_TIME          = 0x17,
    CCDER_GENERALIZED_TIME  = 0x18,
    CCDER_VISIBLE_STRING    = 0x1a,
    CCDER_UNIVERSAL_STRING  = 0x1c,
    CCDER_BMP_STRING        = 0x1e,
    CCDER_TAGNUM_MASK       = 0x1f,

    CCDER_PRIMITIVE         = 0x00,
    CCDER_CONSTRUCTED       = 0x20,
    CCDER_UNIVERSAL         = 0x00,
    CCDER_APPLICATION       = 0x40,
    CCDER_CONTEXT_SPECIFIC  = 0x80,
    CCDER_PRIVATE           = 0xc0,
    CCDER_CLASS_MASK        = 0xc0,

    CCDER_CONSTRUCTED_SEQUENCE = CCDER_SEQUENCE | CCDER_CONSTRUCTED,
    CCDER_CONSTRUCTED_SET      = CCDER_SET | CCDER_CONSTRUCTED,
};

size_t ccder_sizeof_tag(ccder_tag tag);
size_t ccder_sizeof_len(size_t len);
/* size of a tag, its length and len body bytes */
size_t ccder_sizeof(ccder_tag tag, size_t len);
size_t ccder_sizeof_raw_octet_string(size_t s_size);
/* size of an INTEGER holding the non-negative s */
size_t ccder_sizeof_integer(cc_size n, const cc_unit *s);

/*
 * Encoders write backwards: each one places its output right before der_end, returns where that
 * output starts and returns NULL when it does not fit after der or when der_end is NULL, so calls
 * nest from the last element to the first.
 */
uint8_t *ccder_encode_tag(ccder_tag tag, const uint8_t *der, uint8_t *der_end);
uint8_t *ccder_encode_len(size_t len, const uint8_t *der, uint8_t *der_end);
uint8_t *ccder_encode_tl(ccder_tag tag, size_t len, const uint8_t *der, uint8_t *der_end);
/* reserves size bytes for the caller to fill */
uint8_t *ccder_encode_body_nocopy(size_t size, const uint8_t *der, uint8_t *der_end);
uint8_t *ccder_encode_body(size_t size, const uint8_t *body, const uint8_t *der, uint8_t *der_end);
/* the tag and the length of the body that runs from der_end to body_end */
uint8_t *ccder_encode_constructed_tl(ccder_tag tag, const uint8_t *body_end,
                                     const uint8_t *der, uint8_t *der_end);
uint8_t *ccder_encode_integer(cc_size n, const cc_unit *s, const uint8_t *der, uint8_t *der_end);
uint8_t *ccder_encode_raw_octet_string(size_t s_size, const uint8_t *s,
                                       const uint8_t *der, uint8_t *der_end);

/* Decoders return the position after what they read, or NULL; a NULL der passes through. */
const uint8_t *ccder_decode_tag(ccder_tag *tagp, const uint8_t *der, const uint8_t *der_end);
const uint8_t *ccder_decode_len(size_t *lenp, const uint8_t *der, const uint8_t *der_end);
/* expects tag, returns the start of the body and its length */
const uint8_t *ccder_decode_tl(ccder_tag expected_tag, size_t *lenp,
                               const uint8_t *der, const uint8_t *der_end);
/* expects tag, returns the start of the body and where it ends */
const uint8_t *ccder_decode_constructed_tl(ccder_tag expected_tag, const uint8_t **body_end,
                                           const uint8_t *der, const uint8_t *der_end);
const uint8_t *ccder_decode_sequence_tl(const uint8_t **body_end,
                                        const uint8_t *der, const uint8_t *der_end);
/* units the non-negative INTEGER at der needs */
const uint8_t *ccder_decode_uint_n(cc_size *n, const uint8_t *der, const uint8_t *der_end);
/* a non-negative INTEGER that fits in n units */
const uint8_t *ccder_decode_uint(cc_size n, cc_unit *r, const uint8_t *der, const uint8_t *der_end);
/* SEQUENCE { INTEGER r, INTEGER s }, returns the end of the sequence */
const uint8_t *ccder_decode_seqii(cc_size n, cc_unit *r, cc_unit *s,
                                  const uint8_t *der, const uint8_t *der_end);

CC_END_DECLS

#endif
