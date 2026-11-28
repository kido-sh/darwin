/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __XCSELECT_H__
#define __XCSELECT_H__

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/_types/_errno_t.h>

typedef uint32_t xcselect_host_sdk_policy_t;

enum {
	XCSELECT_HOST_SDK_POLICY_MATCHING_PREFERRED = 1,
	XCSELECT_HOST_SDK_POLICY_MATCHING_ONLY      = 2,
	XCSELECT_HOST_SDK_POLICY_LATEST             = 3,
};

__BEGIN_DECLS

/*
 * Locate a macOS SDK path suitable for building host software according to
 * sdk_policy. On success, allocates a C string with malloc(3) and writes it
 * to *path; the caller must free(3) it. Returns 0 on success, EINVAL on
 * invalid arguments (leaving *path unmodified), ENOENT if no matching SDK is
 * available (setting *path to NULL), or ENOMEM on allocation failure.
 */
errno_t xcselect_host_sdk_path(xcselect_host_sdk_policy_t sdk_policy,
                               char **path);

__END_DECLS

#endif /* __XCSELECT_H__ */
