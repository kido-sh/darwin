/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The error singletons that connection handlers receive. Each has type
 * XPC_TYPE_ERROR and holds one entry, XPC_ERROR_KEY_DESCRIPTION, so the
 * dictionary getters read its reason. They live in read-only memory with a
 * global reference count, so retain and release leave them alone.
 */

#include <sys/types.h>
#include <mach/mach.h>
#include <xpc/xpc.h>
#include "xpc_internal.h"

OS_OBJECT_OBJC_CLASS_DECL(xpc_object);

#define ERROR_DESCRIPTION_KEY "XPCErrorDescription"

const char *const _xpc_error_key_description = ERROR_DESCRIPTION_KEY;

struct _xpc_dictionary_s {
	struct xpc_object object;
};

#define GLOBAL_OBJECT_HEADER { \
	.isa = &OS_xpc_object_class, \
	.ref_cnt = _OS_OBJECT_GLOBAL_REFCNT, \
	.xref_cnt = _OS_OBJECT_GLOBAL_REFCNT, \
}

#define DEFINE_ERROR(name, reason) \
	static const struct xpc_object name##_reason = { \
		.header = GLOBAL_OBJECT_HEADER, \
		.xo_xpc_type = XPC_TYPE_STRING, \
		.xo_size = sizeof(reason) - 1, \
		.xo_u = { .str = reason }, \
	}; \
	static const struct xpc_dict_pair name##_entry = { \
		.key = ERROR_DESCRIPTION_KEY, \
		.value = (struct xpc_object *)&name##_reason, \
		.xo_link = { \
			.tqe_next = NULL, \
			.tqe_prev = (struct xpc_dict_pair **)&name.object.xo_dict.tqh_first, \
		}, \
	}; \
	const struct _xpc_dictionary_s name = { .object = { \
		.header = GLOBAL_OBJECT_HEADER, \
		.xo_xpc_type = XPC_TYPE_ERROR, \
		.xo_size = 1, \
		.xo_u = { .dict = { \
			.tqh_first = (struct xpc_dict_pair *)&name##_entry, \
			.tqh_last = (struct xpc_dict_pair **)&name##_entry.xo_link.tqe_next, \
		} }, \
	} }

DEFINE_ERROR(_xpc_error_connection_interrupted, "Connection interrupted");
DEFINE_ERROR(_xpc_error_connection_invalid, "Connection invalid");
DEFINE_ERROR(_xpc_error_termination_imminent, "Termination imminent");
