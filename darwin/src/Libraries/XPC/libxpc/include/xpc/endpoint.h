/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KIDO_XPC_ENDPOINT_H
#define KIDO_XPC_ENDPOINT_H

#include <xpc/xpc.h>

__BEGIN_DECLS

/* An endpoint names a connection inside a message; the peer reconnects with it. */
__XPC_API XPC_RETURNS_RETAINED xpc_endpoint_t xpc_endpoint_create(xpc_connection_t connection);

__END_DECLS

#endif
