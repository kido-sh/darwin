/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

/* libxpc interfaces outside the public SDK that this tree's system code calls. */

#ifndef KIDO_XPC_PRIVATE_H
#define KIDO_XPC_PRIVATE_H

#include <uuid/uuid.h>
#include <xpc/xpc.h>
/* Security's SecDb gets pthread through this header. */
#include <pthread.h>

__BEGIN_DECLS

/* Pipes carry synchronous requests and replies over a Mach port; free them with xpc_release. */
typedef struct _xpc_pipe_s *xpc_pipe_t;

#define XPC_PIPE_FLAG_PRIVILEGED 7

__XPC_SPI xpc_pipe_t xpc_pipe_create(const char *name, uint64_t flags);
__XPC_SPI xpc_pipe_t xpc_pipe_create_from_port(mach_port_t port, uint64_t flags);
__XPC_SPI void xpc_pipe_invalidate(xpc_pipe_t pipe);
__XPC_SPI int xpc_pipe_routine(xpc_pipe_t pipe, xpc_object_t request, xpc_object_t *reply);
__XPC_SPI int xpc_pipe_simpleroutine(xpc_pipe_t pipe, xpc_object_t message);
__XPC_SPI int xpc_pipe_routine_reply(xpc_object_t reply);

/* Directory service lookups (libinfo) */
__XPC_SPI xpc_object_t _od_rpc_call(const char *procedure, xpc_object_t payload,
    xpc_pipe_t (*get_pipe)(bool reset));

/* Process-management requests between launchctl and launchd */
#define XPC_PROCESS_ROUTINE_KEY_OP "process-routine-operation"
#define XPC_PROCESS_ROUTINE_KEY_ARGV "argv"
#define XPC_PROCESS_ROUTINE_KEY_ERROR "error"
#define XPC_PROCESS_ROUTINE_KEY_HANDLE "handle"
#define XPC_PROCESS_ROUTINE_KEY_LABEL "label"
#define XPC_PROCESS_ROUTINE_KEY_MEMORY_LIMIT "memory limit"
#define XPC_PROCESS_ROUTINE_KEY_NAME "name"
#define XPC_PROCESS_ROUTINE_KEY_PATH "path"
#define XPC_PROCESS_ROUTINE_KEY_PID "pid"
#define XPC_PROCESS_ROUTINE_KEY_PRIORITY_BAND "priority band"
#define XPC_PROCESS_ROUTINE_KEY_RCDATA "rcdata"
#define XPC_PROCESS_ROUTINE_KEY_SIGNAL "signal"
#define XPC_PROCESS_ROUTINE_KEY_TYPE "type"

enum {
	XPC_PROCESS_JETSAM_SET_BAND = 0,
	XPC_PROCESS_JETSAM_SET_MEMORY_LIMIT = 1,
	XPC_PROCESS_SERVICE_ATTACH = 2,
	XPC_PROCESS_SERVICE_DETACH = 3,
	XPC_PROCESS_SERVICE_GET_PROPERTIES = 4,
	XPC_PROCESS_SERVICE_KILL = 5
};

/* Dictionaries built from a template such as "{%string: %int64}" */
__XPC_API xpc_object_t xpc_create_with_format(const char *format, ...);
__XPC_API xpc_object_t xpc_create_reply_with_format(xpc_object_t request, const char *format, ...);
__XPC_SPI xpc_object_t xpc_create_from_plist(void *data, size_t length);

__XPC_SPI void xpc_dictionary_get_audit_token(xpc_object_t dictionary, audit_token_t *token);
__XPC_SPI void xpc_dictionary_set_mach_send(xpc_object_t dictionary, const char *key, mach_port_t port);

/* Connections */
__XPC_API void xpc_connection_get_audit_token(xpc_connection_t connection, audit_token_t *token);
__XPC_SPI void xpc_connection_set_target_uid(xpc_connection_t connection, uid_t uid);
/* Always NULL: nothing here is code-signed. */
__XPC_SPI xpc_object_t xpc_connection_copy_entitlement_value(xpc_connection_t connection,
    const char *entitlement);

__XPC_SPI int _xpc_runtime_is_app_sandboxed(void);
/* Terminates the process once its open transactions end. */
__XPC_API void xpc_transaction_exit_clean(void);

__END_DECLS

#endif
