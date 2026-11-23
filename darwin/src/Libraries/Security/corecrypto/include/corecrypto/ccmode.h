/* SPDX-License-Identifier: Apache-2.0 */

#ifndef KIDO_SECURITY_CORECRYPTO_CCMODE_H
#define KIDO_SECURITY_CORECRYPTO_CCMODE_H

#include_next <corecrypto/ccmode.h>

/* the external tree's cccbc_one_shot takes nblocks before iv, Security passes iv first */
#define cccbc_one_shot(_cbc_, _key_len_, _key_, _iv_, _nblocks_, _in_, _out_) \
    cccbc_one_shot(_cbc_, _key_len_, _key_, _nblocks_, _iv_, _in_, _out_)

#endif
