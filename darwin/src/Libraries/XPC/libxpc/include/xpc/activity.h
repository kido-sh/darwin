/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KIDO_XPC_ACTIVITY_H
#define KIDO_XPC_ACTIVITY_H

#include <xpc/xpc.h>

#ifdef __BLOCKS__

__BEGIN_DECLS

#if OS_OBJECT_USE_OBJC
typedef xpc_object_t xpc_activity_t;
#else
typedef struct _xpc_activity_s *xpc_activity_t;
#endif

typedef long xpc_activity_state_t;
enum {
	XPC_ACTIVITY_STATE_RUN = 2
};

typedef void (^xpc_activity_handler_t)(xpc_activity_t activity);

/* Criteria that check in to an activity launchd already holds for the identifier. */
__XPC_API const xpc_object_t XPC_ACTIVITY_CHECK_IN;

__XPC_API void xpc_activity_register(const char *identifier, xpc_object_t criteria,
    xpc_activity_handler_t handler);
__XPC_API xpc_activity_state_t xpc_activity_get_state(xpc_activity_t activity);

__END_DECLS

#endif

#endif
