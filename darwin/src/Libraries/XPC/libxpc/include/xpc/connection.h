/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KIDO_XPC_CONNECTION_H
#define KIDO_XPC_CONNECTION_H

#include <xpc/xpc.h>

#ifndef __BLOCKS__
#error "<xpc/connection.h> needs a compiler with blocks support (-fblocks)"
#endif

__BEGIN_DECLS

/* Flags for xpc_connection_create_mach_service */
#define XPC_CONNECTION_MACH_SERVICE_LISTENER (1 << 0)
#define XPC_CONNECTION_MACH_SERVICE_PRIVILEGED (1 << 1)

/*
 * Delivered to event and reply handlers. Each is a singleton of type
 * XPC_TYPE_ERROR, so callers compare pointers.
 */
__XPC_API const struct _xpc_dictionary_s _xpc_error_connection_interrupted;
__XPC_API const struct _xpc_dictionary_s _xpc_error_connection_invalid;
__XPC_API const struct _xpc_dictionary_s _xpc_error_termination_imminent;
#define XPC_ERROR_CONNECTION_INTERRUPTED __XPC_GLOBAL(_xpc_error_connection_interrupted)
#define XPC_ERROR_CONNECTION_INVALID __XPC_GLOBAL(_xpc_error_connection_invalid)
#define XPC_ERROR_TERMINATION_IMMINENT __XPC_GLOBAL(_xpc_error_termination_imminent)

typedef void (*xpc_finalizer_t)(void *context);

__XPC_API XPC_RETURNS_RETAINED xpc_connection_t xpc_connection_create(const char *name,
    dispatch_queue_t queue);
__XPC_API XPC_RETURNS_RETAINED xpc_connection_t xpc_connection_create_mach_service(const char *name,
    dispatch_queue_t queue, uint64_t flags);
__XPC_API XPC_RETURNS_RETAINED xpc_connection_t xpc_connection_create_from_endpoint(xpc_endpoint_t endpoint);

__XPC_API void xpc_connection_set_target_queue(xpc_connection_t connection, dispatch_queue_t queue);
__XPC_API void xpc_connection_set_event_handler(xpc_connection_t connection, xpc_handler_t handler);
__XPC_API void xpc_connection_set_context(xpc_connection_t connection, void *context);
__XPC_API void *xpc_connection_get_context(xpc_connection_t connection);
__XPC_API void xpc_connection_set_finalizer_f(xpc_connection_t connection, xpc_finalizer_t finalizer);

__XPC_API void xpc_connection_activate(xpc_connection_t connection);
__XPC_API void xpc_connection_suspend(xpc_connection_t connection);
__XPC_API void xpc_connection_resume(xpc_connection_t connection);
__XPC_API void xpc_connection_cancel(xpc_connection_t connection);

__XPC_API void xpc_connection_send_message(xpc_connection_t connection, xpc_object_t message);
__XPC_API void xpc_connection_send_barrier(xpc_connection_t connection, dispatch_block_t barrier);
__XPC_API void xpc_connection_send_message_with_reply(xpc_connection_t connection, xpc_object_t message,
    dispatch_queue_t reply_queue, xpc_handler_t handler);
__XPC_API XPC_RETURNS_RETAINED xpc_object_t xpc_connection_send_message_with_reply_sync(
    xpc_connection_t connection, xpc_object_t message);

/* The peer's name and credentials */
__XPC_API const char *xpc_connection_get_name(xpc_connection_t connection);
__XPC_API uid_t xpc_connection_get_euid(xpc_connection_t connection);
__XPC_API gid_t xpc_connection_get_egid(xpc_connection_t connection);
__XPC_API pid_t xpc_connection_get_pid(xpc_connection_t connection);
__XPC_API au_asid_t xpc_connection_get_asid(xpc_connection_t connection);

__END_DECLS

#endif
