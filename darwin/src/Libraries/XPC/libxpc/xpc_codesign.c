/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/types.h>
#include <sys/codesign.h>
#include <mach/mach.h>
#include <xpc/launchd.h>
#include <xpc/private.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

int
_xpc_runtime_is_app_sandboxed(void)
{
	return 0;
}

xpc_object_t
xpc_copy_entitlements_for_pid(pid_t pid)
{
	int saved_errno = errno;
	CS_GenericBlob header = { 0 };
	CS_GenericBlob *blob;
	size_t size, length;
	xpc_object_t entitlements = NULL;

	if (pid == 0)
		pid = getpid();

	/*
	 * Probe with the header alone: success means there are no entitlements,
	 * ERANGE means there are and the header now holds the blob's size.
	 */
	if (csops(pid, CS_OPS_ENTITLEMENTS_BLOB, &header, sizeof(header)) == 0 || errno != ERANGE)
		return NULL;

	size = ntohl(header.length);
	if (size <= sizeof(header))
		return NULL;
	blob = calloc(1, size);
	if (blob == NULL)
		return NULL;

	if (csops(pid, CS_OPS_ENTITLEMENTS_BLOB, blob, size) == 0) {
		length = ntohl(blob->length);
		if (length > sizeof(*blob) && length <= size)
			entitlements = xpc_data_create(blob->data, length - sizeof(*blob));
	}
	free(blob);

	if (entitlements != NULL)
		errno = saved_errno;
	return entitlements;
}
