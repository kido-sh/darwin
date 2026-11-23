/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/types.h>
#include <sys/reason.h>
#include <mach/mach.h>
#include <xpc/xpc.h>
#include <CrashReporterClient.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include "xpc_internal.h"

#define LIBXPC_EXIT_REASON_API_MISUSE 1

static _Atomic(const char *) api_misuse_info;

static void
write_to_console(const char *message)
{
	int fd = open("/dev/console", O_WRONLY | O_NOCTTY | O_CLOEXEC);

	if (fd < 0)
		return;
	dprintf(fd, "libxpc: fatal: %s\n", message);
	close(fd);
}

void
xpc_api_misuse(const char *format, ...)
{
	const char *message = "API misuse (the message could not be formatted)";
	char *formatted;
	va_list ap;

	va_start(ap, format);
	if (vasprintf(&formatted, format, ap) >= 0)
		message = formatted;
	va_end(ap);

	api_misuse_info = message;
	CRSetCrashLogMessage(message);
	write_to_console(message);
	abort_with_reason(OS_REASON_LIBXPC, LIBXPC_EXIT_REASON_API_MISUSE, message,
	    OS_REASON_FLAG_GENERATE_CRASH_REPORT);
}

const char *
xpc_debugger_api_misuse_info(void)
{
	return api_misuse_info;
}
