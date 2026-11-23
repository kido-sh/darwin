/* SPDX-License-Identifier: Apache-2.0 */

#ifndef KIDO_SECURITY_CORECRYPTO_CCDH_H
#define KIDO_SECURITY_CORECRYPTO_CCDH_H

#include <corecrypto/cczp.h>
#include <corecrypto/ccrng.h>

CC_BEGIN_DECLS

/*
 * A group is a zp for the prime p (n units and n + 1 units of precomputation), then the n-unit
 * generator g and one unit with the private key length in bits, 0 for any length.
 */
struct ccdh_gp {
    cc_size n;
    cc_unit bitlen;
    cczp_funcs_t funcs;
    cc_unit u[];
};
typedef struct ccdh_gp *ccdh_gp_t;
typedef const struct ccdh_gp *ccdh_const_gp_t;

#define ccdh_gp_size(_nbytes_)  (sizeof(struct ccdh_gp) + 3 * ccn_sizeof_size(_nbytes_) + 2 * sizeof(cc_unit))
#define ccdh_gp_n(_gp_)         ((_gp_)->n)
#define CCDH_GP_N(_gp_)         ((_gp_)->n)
#define CCDH_GP_ZP(_gp_)        ((cczp_t)(_gp_))
#define CCDH_GP_PRIME(_gp_)     ((_gp_)->u)
#define CCDH_GP_RECIP(_gp_)     ((_gp_)->u + (_gp_)->n)
#define CCDH_GP_G(_gp_)         ((_gp_)->u + 2 * (_gp_)->n + 1)
#define CCDH_GP_L(_gp_)         ((_gp_)->u[3 * (_gp_)->n + 1])

/* a key is its group and the public y; a full key adds the private x after y */
struct ccdh_pub_ctx {
    ccdh_const_gp_t gp;
    cc_unit xy[];
};
struct ccdh_full_ctx {
    ccdh_const_gp_t gp;
    cc_unit xy[];
};
typedef struct ccdh_pub_ctx *ccdh_pub_ctx_t;
typedef struct ccdh_full_ctx *ccdh_full_ctx_t;

#define ccdh_pub_ctx_size(_nbytes_)   (sizeof(struct ccdh_pub_ctx) + ccn_sizeof_size(_nbytes_))
#define ccdh_full_ctx_size(_nbytes_)  (sizeof(struct ccdh_full_ctx) + 2 * ccn_sizeof_size(_nbytes_))
#define ccdh_pub_ctx_decl_gp(_gp_, _name_) \
    cc_ctx_decl(struct ccdh_pub_ctx, ccdh_pub_ctx_size(ccn_sizeof_n(ccdh_gp_n(_gp_))), _name_)
#define ccdh_full_ctx_decl_gp(_gp_, _name_) \
    cc_ctx_decl(struct ccdh_full_ctx, ccdh_full_ctx_size(ccn_sizeof_n(ccdh_gp_n(_gp_))), _name_)

#define ccdh_ctx_gp(_key_)      ((_key_)->gp)
#define ccdh_ctx_n(_key_)       ccdh_gp_n(ccdh_ctx_gp(_key_))
#define ccdh_ctx_y(_key_)       ((_key_)->xy)
#define ccdh_ctx_x(_key_)       ((_key_)->xy + ccdh_ctx_n(_key_))
#define ccdh_ctx_public(_key_)  ((ccdh_pub_ctx_t)(_key_))

int ccdh_generate_key(ccdh_const_gp_t gp, struct ccrng_state *rng, ccdh_full_ctx_t key);

/* big-endian y, checked to lie in [2, p - 2] */
int ccdh_import_pub(ccdh_const_gp_t gp, size_t in_len, const uint8_t *in, ccdh_pub_ctx_t key);

/* r = y^x mod p of ccdh_gp_n units, refusing degenerate peer keys and results */
int ccdh_compute_key(ccdh_full_ctx_t private_key, ccdh_pub_ctx_t public_key, cc_unit *r);

CC_END_DECLS

#endif
