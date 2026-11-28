/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Minimal dyld version-check SPI declarations for libresolv res_state.c.
 */
#ifndef _PD_LIBRESOLV_MACH_O_DYLD_PRIV_H_
#define _PD_LIBRESOLV_MACH_O_DYLD_PRIV_H_

#include <stdbool.h>
#include <stdint.h>
#include <mach-o/loader.h>

typedef uint32_t dyld_platform_t;

typedef struct {
	dyld_platform_t platform;
	uint32_t        version;
} dyld_build_version_t;

extern bool dyld_program_minos_at_least(dyld_build_version_t version);

#define dyld_2024_SU_E_os_versions \
	((dyld_build_version_t){ PLATFORM_MACOS, 0x000f0400 })

#endif /* _PD_LIBRESOLV_MACH_O_DYLD_PRIV_H_ */
