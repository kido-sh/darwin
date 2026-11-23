/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

/* What launchd and its clients share beyond the public XPC interface. */

#ifndef KIDO_XPC_LAUNCHD_H
#define KIDO_XPC_LAUNCHD_H

#include <xpc/xpc.h>

__BEGIN_DECLS

/* Error codes in launchd's XPC replies; xpc_strerror describes them. */
#define EXNOMEM 1
#define EXINVAL 2
#define EXSRCH 3
#define EXMAX EXSRCH

__XPC_SPI const char *xpc_strerror(int error);

__XPC_SPI int xpc_pipe_try_receive(mach_port_t port, xpc_object_t *message, mach_port_t *reply_port,
    boolean_t (*demux)(mach_msg_header_t *request, mach_msg_header_t *reply), mach_msg_size_t max_size,
    int flags);
__XPC_SPI int xpc_pipe_routine_reply(xpc_object_t reply);
__XPC_SPI kern_return_t xpc_call_wakeup(mach_port_t port, int status);

__XPC_SPI void xpc_dictionary_get_audit_token(xpc_object_t dictionary, audit_token_t *token);
__XPC_SPI void xpc_dictionary_set_mach_recv(xpc_object_t dictionary, const char *key, mach_port_t port);
__XPC_SPI void xpc_dictionary_set_mach_send(xpc_object_t dictionary, const char *key, mach_port_t port);
__XPC_SPI mach_port_t xpc_dictionary_copy_mach_send(xpc_object_t dictionary, const char *key);

__XPC_SPI xpc_object_t xpc_copy_entitlement_for_token(const char *entitlement, audit_token_t *token);
__XPC_SPI xpc_object_t xpc_copy_entitlements_for_pid(pid_t pid);

__END_DECLS

#endif
