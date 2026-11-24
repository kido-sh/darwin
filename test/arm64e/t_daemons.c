/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libproc.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/proc_info.h>

#include "../libsystem/check.h"

struct remote_image_infos {
    uint32_t version;
    uint32_t count;
    uint64_t array;
};

struct remote_image_info {
    uint64_t load_address;
    uint64_t path;
    uint64_t mod_date;
};

static int
remote_read(task_t task, uint64_t addr, void *buf, size_t len)
{
    mach_vm_size_t got = 0;

    return mach_vm_read_overwrite(task, addr, len, (mach_vm_address_t)(uintptr_t)buf, &got) == KERN_SUCCESS &&
        got == len;
}

static pid_t
find_pid(const char *name)
{
    int n = proc_listallpids(NULL, 0);
    pid_t *pids = calloc((size_t)n + 64, sizeof(pid_t));
    pid_t found = -1;

    n = proc_listallpids(pids, (int)(((size_t)n + 64) * sizeof(pid_t)));
    for (int i = 0; i < n && found < 0; i++) {
        char pname[2 * MAXCOMLEN + 1] = "";
        if (proc_name(pids[i], pname, sizeof(pname)) > 0 && strcmp(pname, name) == 0) {
            found = pids[i];
        }
    }
    free(pids);
    return found;
}

static int
is_arm64e(pid_t pid)
{
    struct proc_archinfo arch;

    if (proc_pidinfo(pid, PROC_PIDARCHINFO, 0, &arch, sizeof(arch)) != sizeof(arch)) {
        return -1;
    }
    return arch.p_cputype == CPU_TYPE_ARM64 && (arch.p_cpusubtype & ~CPU_SUBTYPE_MASK) == CPU_SUBTYPE_ARM64E;
}

static int
main_executable(task_t task, uint64_t *load_address)
{
    task_dyld_info_data_t info;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    struct remote_image_infos all;

    if (task_info(task, TASK_DYLD_INFO, (task_info_t)&info, &count) != KERN_SUCCESS ||
        !remote_read(task, info.all_image_info_addr, &all, sizeof(all)) || all.array == 0) {
        return 0;
    }
    for (uint32_t i = 0; i < all.count; i++) {
        struct remote_image_info image;
        struct mach_header_64 mh;
        if (remote_read(task, all.array + i * sizeof(image), &image, sizeof(image)) &&
            remote_read(task, image.load_address, &mh, sizeof(mh)) && mh.filetype == MH_EXECUTE) {
            *load_address = image.load_address;
            return 1;
        }
    }
    return 0;
}

static void
auth_got(task_t task, int *entries, int *signed_entries)
{
    uint64_t load = 0, text = 0, got = 0, got_size = 0;
    struct mach_header_64 mh;

    *entries = *signed_entries = 0;
    if (!main_executable(task, &load) || !remote_read(task, load, &mh, sizeof(mh))) {
        return;
    }
    char *cmds = malloc(mh.sizeofcmds);
    if (cmds == NULL || !remote_read(task, load + sizeof(mh), cmds, mh.sizeofcmds)) {
        free(cmds);
        return;
    }
    const char *p = cmds;
    for (uint32_t i = 0; i < mh.ncmds; i++) {
        const struct load_command *lc = (const struct load_command *)p;
        if (lc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *seg = (const struct segment_command_64 *)p;
            const struct section_64 *sect = (const struct section_64 *)(seg + 1);
            if (strncmp(seg->segname, "__TEXT", sizeof(seg->segname)) == 0) {
                text = seg->vmaddr;
            }
            for (uint32_t s = 0; s < seg->nsects; s++) {
                if (strncmp(sect[s].sectname, "__auth_got", sizeof(sect[s].sectname)) == 0) {
                    got = sect[s].addr;
                    got_size = sect[s].size;
                }
            }
        }
        p += lc->cmdsize;
    }
    free(cmds);
    for (uint64_t off = 0; got != 0 && off + 8 <= got_size; off += 8) {
        uint64_t v = 0;
        if (remote_read(task, got - text + load + off, &v, sizeof(v)) && v != 0) {
            (*entries)++;
            *signed_entries += (v >> 47) != 0;
        }
    }
}

int
main(void)
{
    const char *daemons[] = { "notifyd", "logd" };

    CHECK(is_arm64e(1) == 0);
    for (size_t d = 0; d < sizeof(daemons) / sizeof(daemons[0]); d++) {
        pid_t pid = find_pid(daemons[d]);
        task_t task = MACH_PORT_NULL;
        int entries = 0, signed_entries = 0;

        CHECK(pid > 0);
        if (pid <= 0) {
            continue;
        }
        CHECK(is_arm64e(pid) == 1);
        CHECK(task_for_pid(mach_task_self(), pid, &task) == KERN_SUCCESS);
        if (task != MACH_PORT_NULL) {
            auth_got(task, &entries, &signed_entries);
            mach_port_deallocate(mach_task_self(), task);
        }
        printf("%s: pid %d, %d of %d __auth_got pointers signed\n", daemons[d], pid, signed_entries, entries);
        CHECK(entries > 0 && signed_entries * 10 >= entries * 9);
    }
    return checks_done("arm64e daemons");
}
