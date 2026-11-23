/* SPDX-License-Identifier: Apache-2.0 */

#ifndef KIDO_SECURITY_CORECRYPTO_CCN_H
#define KIDO_SECURITY_CORECRYPTO_CCN_H

#include_next <corecrypto/ccn.h>

CC_BEGIN_DECLS

/* r = s + v, returns the carry */
cc_unit ccn_add1(cc_size n, cc_unit *r, const cc_unit *s, cc_unit v);

/* compares s of ns units with t of nt units, returns -1, 0 or 1 */
int ccn_cmpn(cc_size ns, const cc_unit *s, cc_size nt, const cc_unit *t);

void ccn_seti(cc_size n, cc_unit *r, cc_unit v);

/* bytes in the big-endian form of s without leading zeros */
size_t ccn_write_uint_size(cc_size n, const cc_unit *s);

/*
 * Writes the leading out_size bytes of the big-endian form of s without leading zeros,
 * zero-filling the rest of out when it is longer.
 */
void ccn_write_uint(cc_size n, const cc_unit *s, size_t out_size, void *out);

/* bytes in the two's complement form of the non-negative s, at least 1 */
size_t ccn_write_int_size(cc_size n, const cc_unit *s);

/* ccn_write_uint for the two's complement form */
void ccn_write_int(cc_size n, const cc_unit *s, size_t out_size, void *out);

CC_END_DECLS

#endif
