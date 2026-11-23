/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KIDO_XPC_BASE_H
#define KIDO_XPC_BASE_H

#include <sys/cdefs.h>
#include <Availability.h>

#define __XPC_API extern __attribute__((__visibility__("default")))
#define __XPC_SPI extern
#define __XPC_DEBUGGER_ONLY extern __attribute__((__visibility__("hidden"), __used__))
#define __XPC_NORETURN __attribute__((__noreturn__))
#define __XPC_PRINTF(fmt, first) __attribute__((__format__(__printf__, fmt, first)))

#endif
