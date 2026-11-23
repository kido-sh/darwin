/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
 * Modified by the cctools-port project and by the PureDarwin project in 2021-2026; see docs/upstream.md.
 */
/*
 * @OSF_COPYRIGHT@
 */

#ifndef _MACH_ARM_THREAD_STATE_H_
#define _MACH_ARM_THREAD_STATE_H_

#define ARM_THREAD_STATE_MAX	(272)

#if defined (__arm__)
#define THREAD_STATE_MAX	ARM_THREAD_STATE_MAX
#endif

#if defined(__arm64__) || defined(__aarch64__) && !defined(THREAD_STATE_MAX)
#define THREAD_STATE_MAX	ARM_THREAD_STATE_MAX
#endif

#endif	/* _MACH_ARM_THREAD_STATE_H_ */
