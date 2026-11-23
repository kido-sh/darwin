/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KIDO_XPC_DEBUG_H
#define KIDO_XPC_DEBUG_H

#include <xpc/base.h>

__BEGIN_DECLS

/* For a debugger: why libxpc aborted the process, or NULL. Not exported. */
__XPC_DEBUGGER_ONLY const char *xpc_debugger_api_misuse_info(void);

__END_DECLS

#endif
