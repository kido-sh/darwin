/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <sys/reboot.h>
#include <sys/syscall.h>

static long
reboot_with_message(long howto, const char *message)
{
    register long x0 __asm__("x0") = howto;
    register const char *x1 __asm__("x1") = message;
    register long x16 __asm__("x16") = SYS_reboot;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x1), "r"(x16) : "memory", "cc");
    return x0;
}

int
main(int argc, char **argv)
{
    long rc = reboot_with_message(RB_PANIC | RB_QUICK, argc > 1 ? argv[1] : "t_panic");
    printf("FAIL reboot(RB_PANIC | RB_QUICK) returned %ld\n", rc);
    return 1;
}
