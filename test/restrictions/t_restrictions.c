/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <errno.h>
#include <fcntl.h>
#include <libproc.h>
#include <limits.h>
#include <mach/mach.h>
#include <notify.h>
#include <servers/bootstrap.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/proc_info.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern kern_return_t bootstrap_register2(mach_port_t bp, name_t service_name,
    mach_port_t sp, uint64_t flags);
extern void *vproc_swap_integer(void *vp, int key, int64_t *inval, int64_t *outval);
extern uint32_t notify_register_plain(const char *name, int *out_token);
extern kern_return_t IOCatalogueReset(mach_port_t mainPort, uint32_t flag);
extern kern_return_t IOCatalogueSendData(mach_port_t mainPort, uint32_t flag,
    const char *buffer, uint32_t size);
extern kern_return_t IOCatalogueModuleLoaded(mach_port_t mainPort, io_name_t name);
extern int __mac_syscall(const char *policy, int call, void *arg);
extern int fsctl(const char *path, unsigned long request, void *data, unsigned int options);

#define TEST_UID_NOBODY 65534
#define TEST_UID_DOUBLEAGENTD 283

#define TEST_SYS_POSIX_SPAWN 244
#define TEST_SYS_LEDGER 373
#define TEST_SYS_PID_SUSPEND 433
#define TEST_SYS_PID_SHUTDOWN_SOCKETS 436
#define TEST_SYS_KAS_INFO 439
#define TEST_SYS_MEMORYSTATUS_CONTROL 440
#define TEST_SYS_TELEMETRY 451
#define TEST_SYS_COALITION_INFO 459
#define TEST_SYS_PERSONA 494
#define TEST_SYS_WORK_INTERVAL_CTL 499

#define TEST_MEMORYSTATUS_CMD_SET_JETSAM_TASK_LIMIT 6
#define TEST_SHUTDOWN_SOCKET_LEVEL_DISCONNECT_ALL 2
#define TEST_PERSONA_OP_ALLOC 1
#define TEST_PERSONA_INFO_V1 1
#define TEST_PERSONA_GUEST 1
#define TEST_POSIX_SPAWN_JETSAM_SET 0x8000
#define TEST_POSIX_SPAWN_JETSAM_RELAUNCH_MED 0x20
#define TEST_TELEMETRY_CMD_PMI_SETUP 3
#define TEST_TELEMETRY_CMD_PAGEIN_SETUP 4
#define TEST_LEDGER_INFO 0
#define TEST_COALITION_INFO_RESOURCE_USAGE 1
#define TEST_COALITION_INFO_SET_NAME 2
#define TEST_COALITION_INFO_SET_EFFICIENCY 3
#define TEST_COALITION_INFO_PID_LIST 5
#define TEST_WORK_INTERVAL_OP_CREATE2 4
#define TEST_KAS_INFO_KERNEL_TEXT_SLIDE 0
#define TEST_IOCATALOG_ADD_DRIVERS 1
#define TEST_IOCATALOG_RESET_DEFAULT 1
#define TEST_HOST_TELEMETRY_PORT 20
#define TEST_HOST_ATM_NOTIFICATION_PORT 21
#define TEST_SF_RESTRICTED 0x00080000
#define TEST_UF_DATAVAULT 0x00000080
#define TEST_FSIOC_KERNEL_ROOTAUTH _IOW('h', 51, uint32_t)
#define TEST_AMFI_DYLD_INPUT_RESTRICT_SEG (1ULL << 2)

#define TEST_LAUNCHD_MIG_PARENT 406
#define TEST_LAUNCHD_MIG_GET_ROOT_BOOTSTRAP 436
#define TEST_VPROC_GSK_GLOBAL_UMASK 14
#define TEST_BOOTSTRAP_NOT_PRIVILEGED 1100

#define TEST_AKS_HANDLE_DEVICE ((uint64_t)(int64_t)-6)
#define TEST_AKS_HANDLE_DEVICE_ALT ((uint64_t)(int64_t)-1)
#define TEST_AKS_HANDLE_SESSION ((uint64_t)(int64_t)-3)
#define TEST_AKS_HANDLE_FOREIGN_UID ((uint64_t)(int64_t)-501)
#define TEST_AKS_HANDLE_INT64_MIN ((uint64_t)INT64_MIN)
#define TEST_AKS_ERR_NO_SESSION ((IOReturn)0xe00002c2)

#define TEST_DI_CREATE_SIZE 2112
#define TEST_DI_CREATE_BLOCKS 0x08
#define TEST_DI_CREATE_BLOCK_SIZE 0x10
#define TEST_DI_CREATE_LOCATION 0x17

struct test_kpersona_info {
    uint32_t persona_info_version;
    uid_t persona_id;
    int persona_type;
    gid_t persona_gid;
    uint32_t persona_ngroups;
    gid_t persona_groups[16];
    uid_t persona_gmuid;
    char persona_name[256];
    uid_t persona_uid;
} __attribute__((packed));

_Static_assert(sizeof(struct test_kpersona_info) == 348, "kpersona_info size");

struct test_ledger_info {
    char li_name[32];
    int64_t li_id;
    int64_t li_entries;
};

struct test_work_interval_create_params {
    uint64_t wicp_id;
    mach_port_name_t wicp_port;
    uint32_t wicp_create_flags;
};

struct test_posix_spawnattr {
    short psa_flags;
    short flags_padding;
    sigset_t psa_sigdefault;
    sigset_t psa_sigmask;
    pid_t psa_pgroup;
    cpu_type_t psa_binprefs[4];
    int psa_pcontrol;
    int psa_apptype;
    uint64_t psa_cpumonitor_percent;
    uint64_t psa_cpumonitor_interval;
    uint64_t psa_reserved;
    short psa_jetsam_flags;
    short short_padding;
    int psa_priority;
    int psa_memlimit_active;
    int psa_memlimit_inactive;
    uint64_t psa_qos_clamp;
    int psa_darwin_role;
    int psa_thread_limit;
    uint64_t psa_max_addr;
    bool psa_no_smt;
    bool psa_4k;
    bool psa_tecs;
    int psa_platform;
    cpu_subtype_t psa_subcpuprefs[4];
    uint32_t psa_options;
    uint32_t psa_port_soft_limit;
    uint32_t psa_port_hard_limit;
    uint32_t psa_filedesc_soft_limit;
    uint32_t psa_filedesc_hard_limit;
    uint32_t psa_crash_behavior;
    int psa_dataless_iopolicy;
    uint64_t psa_crash_behavior_deadline;
    uint8_t psa_launch_type;
    uint16_t psa_sec_flags;
    uint32_t psa_crash_count;
    uint32_t psa_throttle_timeout;
    uint32_t psa_kqworkloop_soft_limit;
    uint32_t psa_kqworkloop_hard_limit;
    uint32_t psa_conclave_mem_limit;
    void *psa_ports;
    void *psa_mac_extensions;
    void *psa_coalition_info;
    void *psa_persona_info;
    void *psa_posix_cred_info;
    char *psa_subsystem_root_path;
    char *psa_conclave_id;
};

struct test_posix_spawn_args_desc {
    size_t attr_size;
    struct test_posix_spawnattr *attrp;
    size_t file_actions_size;
    void *file_actions;
    size_t port_actions_size;
    void *port_actions;
    size_t mac_extensions_size;
    void *mac_extensions;
    size_t coal_info_size;
    void *coal_info;
    size_t persona_info_size;
    void *persona_info;
    size_t posix_cred_info_size;
    void *posix_cred_info;
    size_t subsystem_root_path_size;
    char *subsystem_root_path;
    size_t conclave_id_size;
    char *conclave_id;
};

static int g_run;
static int g_fail;

static void
record(bool ok, const char *section, const char *name, const char *detail_fmt, ...)
{
    g_run++;
    if (ok) {
        printf("ok   %s: %s\n", section, name);
        return;
    }
    g_fail++;
    printf("FAIL %s: %s", section, name);
    if (detail_fmt != NULL && detail_fmt[0] != '\0') {
        va_list ap;
        printf(" (");
        va_start(ap, detail_fmt);
        vprintf(detail_fmt, ap);
        va_end(ap);
        printf(")");
    }
    printf("\n");
}

static io_registry_entry_t
find_nvram(void)
{
    io_registry_entry_t entry = IORegistryEntryFromPath(MACH_PORT_NULL, "IODeviceTree:/options");
    if (entry != IO_OBJECT_NULL) {
        return entry;
    }
    return IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching("IODTNVRAM"));
}

static mach_port_t
make_send_receive_port(void)
{
    mach_port_t port = MACH_PORT_NULL;
    if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port) != KERN_SUCCESS) {
        return MACH_PORT_NULL;
    }
    if (mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS) {
        mach_port_destruct(mach_task_self(), port, 0, 0);
        return MACH_PORT_NULL;
    }
    return port;
}

static void
destroy_send_receive_port(mach_port_t port)
{
    if (port != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), port);
        mach_port_destruct(mach_task_self(), port, 0, 0);
    }
}

static kern_return_t
launchd_mig_noargs(mach_port_t bp, mach_msg_id_t req_id, mach_port_t *out_port)
{
    struct {
        mach_msg_header_t hdr;
        union {
            struct {
                NDR_record_t ndr;
                kern_return_t ret;
            } err;
            struct {
                mach_msg_body_t body;
                mach_msg_port_descriptor_t port;
            } ok;
        } u;
        mach_msg_trailer_t trailer;
    } msg;
    mach_port_t reply = MACH_PORT_NULL;
    kern_return_t kr;

    *out_port = MACH_PORT_NULL;
    kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply);
    if (kr != KERN_SUCCESS) {
        return kr;
    }
    memset(&msg, 0, sizeof(msg));
    msg.hdr.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
    msg.hdr.msgh_size = sizeof(mach_msg_header_t);
    msg.hdr.msgh_remote_port = bp;
    msg.hdr.msgh_local_port = reply;
    msg.hdr.msgh_id = req_id;

    kr = mach_msg(&msg.hdr, MACH_SEND_MSG | MACH_RCV_MSG, sizeof(mach_msg_header_t),
        sizeof(msg), reply, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    mach_port_destruct(mach_task_self(), reply, 0, 0);
    if (kr != KERN_SUCCESS) {
        return kr;
    }
    if ((msg.hdr.msgh_bits & MACH_MSGH_BITS_COMPLEX) != 0) {
        *out_port = msg.u.ok.port.name;
        return KERN_SUCCESS;
    }
    return msg.u.err.ret;
}

static void
seed_root_keybag(void)
{
    io_service_t svc = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching("AppleKeyStore"));
    if (svc == IO_OBJECT_NULL) {
        return;
    }
    io_connect_t conn = IO_OBJECT_NULL;
    if (IOServiceOpen(svc, mach_task_self(), 0, &conn) == KERN_SUCCESS && conn != IO_OBJECT_NULL) {
        uint64_t uid0 = 0;
        uint64_t out_h = 0;
        uint32_t out_cnt = 1;
        if (IOConnectCallScalarMethod(conn, 14, &uid0, 1, &out_h, &out_cnt) != KERN_SUCCESS) {
            out_cnt = 1;
            if (IOConnectCallScalarMethod(conn, 2, NULL, 0, &out_h, &out_cnt) == KERN_SUCCESS) {
                uint64_t bind_in[2] = { out_h, 0ULL };
                (void)IOConnectCallScalarMethod(conn, 5, bind_in, 2, NULL, NULL);
            }
        }
        IOServiceClose(conn);
    }
    IOObjectRelease(svc);
}

static int
run_unpriv_tests(void)
{
    const char *sec = "unpriv";

    record(getuid() == TEST_UID_NOBODY && geteuid() == TEST_UID_NOBODY,
        sec, "running as nobody", "uid=%u euid=%u", (unsigned)getuid(), (unsigned)geteuid());

    {
        mach_port_name_t port = MACH_PORT_NULL;
        kern_return_t kr = task_name_for_pid(mach_task_self(), 1, &port);
        record(kr != KERN_SUCCESS, sec, "task_name_for_pid(pid=1) refused", "kr=0x%x", kr);
        if (kr == KERN_SUCCESS && port != MACH_PORT_NULL && port != mach_task_self()) {
            mach_port_deallocate(mach_task_self(), port);
        }
    }

    {
        struct test_work_interval_create_params params;
        memset(&params, 0, sizeof(params));
        errno = 0;
        int r = syscall(TEST_SYS_WORK_INTERVAL_CTL, (uint32_t)TEST_WORK_INTERVAL_OP_CREATE2,
            (uint64_t)0, &params, sizeof(params));
        record(r == -1 && errno == EPERM, sec, "work_interval_create refused without platform trust",
            "r=%d errno=%d", r, errno);
    }

    {
        errno = 0;
        int r = syscall(TEST_SYS_MEMORYSTATUS_CONTROL,
            (uint32_t)TEST_MEMORYSTATUS_CMD_SET_JETSAM_TASK_LIMIT, (int32_t)1,
            (uint32_t)256, (uintptr_t)0, (size_t)0);
        record(r == -1 && errno == EPERM, sec, "memorystatus_control(pid=1) EPERM",
            "r=%d errno=%d", r, errno);
    }

    {
        errno = 0;
        int r = syscall(TEST_SYS_PID_SUSPEND, 1);
        record(r == -1 && errno == EPERM, sec, "pid_suspend(1) EPERM",
            "r=%d errno=%d", r, errno);
    }

    {
        errno = 0;
        int r = syscall(TEST_SYS_PID_SHUTDOWN_SOCKETS, 1, TEST_SHUTDOWN_SOCKET_LEVEL_DISCONNECT_ALL);
        record(r == -1 && errno == EPERM, sec, "pid_shutdown_sockets(1) EPERM",
            "r=%d errno=%d", r, errno);
    }

    {
        struct timeval tv;
        memset(&tv, 0, sizeof(tv));
        gettimeofday(&tv, NULL);
        errno = 0;
        int r = settimeofday(&tv, NULL);
        record(r == -1 && errno == EPERM, sec, "settimeofday EPERM",
            "r=%d errno=%d", r, errno);
    }

    {
        struct test_kpersona_info info;
        uid_t persona_id = (uid_t)-1;
        memset(&info, 0, sizeof(info));
        info.persona_info_version = TEST_PERSONA_INFO_V1;
        info.persona_id = (uid_t)-1;
        info.persona_type = TEST_PERSONA_GUEST;
        snprintf(info.persona_name, sizeof(info.persona_name), "kido-test");
        errno = 0;
        int r = syscall(TEST_SYS_PERSONA, (uint32_t)TEST_PERSONA_OP_ALLOC, (uint32_t)0,
            &info, &persona_id, (size_t)0, (void *)NULL);
        record(r == -1 && errno == EPERM, sec, "persona(ALLOC) EPERM",
            "r=%d errno=%d", r, errno);
    }

    {
        struct test_posix_spawnattr sa;
        struct test_posix_spawn_args_desc adesc;
        pid_t child = -1;
        char *argv[] = { "/bin/sh", "-c", "true", NULL };
        char *envp[] = { NULL };

        memset(&sa, 0, sizeof(sa));
        memset(&adesc, 0, sizeof(adesc));
        memset(sa.psa_subcpuprefs, 0xff, sizeof(sa.psa_subcpuprefs));
        sa.psa_reserved = 1;
        sa.psa_jetsam_flags = (short)TEST_POSIX_SPAWN_JETSAM_SET;
        sa.psa_priority = -1;
        sa.psa_memlimit_active = -1;
        sa.psa_memlimit_inactive = -1;
        adesc.attr_size = sizeof(sa);
        adesc.attrp = &sa;

        errno = 0;
        int r1 = syscall(TEST_SYS_POSIX_SPAWN, &child, "/bin/sh", &adesc, argv, envp);
        record(r1 == -1 && errno == EPERM, sec, "posix_spawn(POSIX_SPAWN_JETSAM_SET) EPERM",
            "r=%d errno=%d", r1, errno);
        if (r1 == 0 && child > 0) {
            waitpid(child, NULL, 0);
        }

        child = -1;
        sa.psa_jetsam_flags = (short)TEST_POSIX_SPAWN_JETSAM_RELAUNCH_MED;
        errno = 0;
        int r2 = syscall(TEST_SYS_POSIX_SPAWN, &child, "/bin/sh", &adesc, argv, envp);
        record(r2 == -1 && errno == EPERM, sec, "posix_spawn(JETSAM_RELAUNCH_BEHAVIOR_MED) EPERM",
            "r=%d errno=%d", r2, errno);
        if (r2 == 0 && child > 0) {
            waitpid(child, NULL, 0);
        }
    }

    {
        errno = 0;
        int r1 = syscall(TEST_SYS_TELEMETRY, (uint64_t)TEST_TELEMETRY_CMD_PMI_SETUP,
            (uint64_t)0, (uint64_t)0, (uint64_t)0, (uint64_t)0, (uint64_t)0);
        record(r1 == -1 && errno == EPERM, sec, "telemetry(PMI_SETUP) EPERM",
            "r=%d errno=%d", r1, errno);

        errno = 0;
        int r2 = syscall(TEST_SYS_TELEMETRY, (uint64_t)TEST_TELEMETRY_CMD_PAGEIN_SETUP,
            (uint64_t)0, (uint64_t)0, (uint64_t)0, (uint64_t)0, (uint64_t)0);
        record(r2 == -1 && errno == EPERM, sec, "telemetry(PAGEIN_SETUP) EPERM",
            "r=%d errno=%d", r2, errno);
    }

    {
        struct test_ledger_info li;
        memset(&li, 0, sizeof(li));
        errno = 0;
        int r = syscall(TEST_SYS_LEDGER, TEST_LEDGER_INFO, (intptr_t)1, (intptr_t)&li, (intptr_t)0);
        record(r == -1 && errno == EPERM, sec, "ledger(LEDGER_INFO, pid=1) EPERM",
            "r=%d errno=%d", r, errno);

        errno = 0;
        int rself = syscall(TEST_SYS_LEDGER, TEST_LEDGER_INFO, (intptr_t)getpid(), (intptr_t)&li, (intptr_t)0);
        record(rself == 0, sec, "ledger(LEDGER_INFO, self) succeeds",
            "r=%d errno=%d", rself, errno);
    }

    {
        uint64_t cid = 1;
        char cname[32] = "kido-test";
        size_t sz = sizeof(cname);
        errno = 0;
        int r1 = syscall(TEST_SYS_COALITION_INFO, (uint32_t)TEST_COALITION_INFO_SET_NAME,
            &cid, cname, &sz);
        record(r1 == -1 && errno == EPERM, sec, "coalition_info(SET_NAME, cid=1) EPERM",
            "r=%d errno=%d", r1, errno);

        uint64_t flags = 0;
        size_t fsz = sizeof(flags);
        errno = 0;
        int r2 = syscall(TEST_SYS_COALITION_INFO, (uint32_t)TEST_COALITION_INFO_SET_EFFICIENCY,
            &cid, &flags, &fsz);
        record(r2 == -1 && errno == EPERM, sec, "coalition_info(SET_EFFICIENCY, cid=1) EPERM",
            "r=%d errno=%d", r2, errno);

        pid_t pids[16];
        size_t psz = sizeof(pids);
        errno = 0;
        int r3 = syscall(TEST_SYS_COALITION_INFO, (uint32_t)TEST_COALITION_INFO_PID_LIST,
            &cid, pids, &psz);
        record(r3 == -1 && errno == EPERM, sec, "coalition_info(PID_LIST, cid=1) EPERM",
            "r=%d errno=%d", r3, errno);

        uint64_t bogus_cid = 999999;
        uint8_t rbuf[512];
        size_t rsz = sizeof(rbuf);
        errno = 0;
        int r4 = syscall(TEST_SYS_COALITION_INFO, (uint32_t)TEST_COALITION_INFO_RESOURCE_USAGE,
            &bogus_cid, rbuf, &rsz);
        record(r4 == -1 && errno == EPERM, sec, "coalition_info(bogus cid) EPERM before lookup",
            "r=%d errno=%d", r4, errno);
    }

    {
        int aot = 0;
        errno = 0;
        int r1 = sysctlbyname("kern.aotmode", NULL, NULL, &aot, sizeof(aot));
        record(r1 == -1 && errno == EPERM, sec, "sysctl write kern.aotmode EPERM",
            "r=%d errno=%d", r1, errno);

        uint8_t prog[64];
        size_t proglen = sizeof(prog);
        memset(prog, 0, sizeof(prog));
        if (sysctlbyname("kern.progressoptions", prog, &proglen, NULL, 0) != 0 || proglen == 0) {
            proglen = sizeof(prog);
        }
        errno = 0;
        int r2 = sysctlbyname("kern.progressoptions", NULL, NULL, prog, proglen);
        record(r2 == -1 && errno == EPERM, sec, "sysctl write kern.progressoptions EPERM",
            "r=%d errno=%d", r2, errno);

        unsigned int hib = 0;
        errno = 0;
        int r3 = sysctlbyname("kern.hibernategraphicsready", NULL, NULL, &hib, sizeof(hib));
        if (r3 == -1 && errno == ENOENT) {
            printf("skip %s: sysctl kern.hibernategraphicsready (not built)\n", sec);
        } else {
            record(r3 == -1 && errno == EPERM, sec, "sysctl write kern.hibernategraphicsready EPERM",
                "r=%d errno=%d", r3, errno);
        }
    }

    {
        uint32_t dummy = 0;
        errno = 0;
        int r = fsctl("/", TEST_FSIOC_KERNEL_ROOTAUTH, &dummy, 0);
        record(r == -1 && (errno == EPERM || errno == EINVAL), sec,
            "fsctl(FSIOC_KERNEL_ROOTAUTH) refused", "r=%d errno=%d", r, errno);
    }

    {
        uint64_t out_flags = ~0ULL;
        uint64_t args[2] = { TEST_AMFI_DYLD_INPUT_RESTRICT_SEG, (uint64_t)(uintptr_t)&out_flags };
        errno = 0;
        int r1 = __mac_syscall("AMFI", 90, args);
        record(r1 == 0 && out_flags == 0, sec, "AMFI syscall 90 denies restricted segment flags",
            "r=%d errno=%d out=0x%llx", r1, errno, (unsigned long long)out_flags);

        errno = 0;
        int r2 = __mac_syscall("AMFI", 9999, NULL);
        record(r2 == -1 && errno == ENOSYS, sec, "AMFI unknown syscall returns ENOSYS",
            "r=%d errno=%d", r2, errno);
    }

    {
        io_registry_entry_t nvram = find_nvram();
        if (nvram != IO_OBJECT_NULL) {
            kern_return_t kr1 = IORegistryEntrySetCFProperty(nvram, CFSTR("boot-args"), CFSTR("debug=0x1"));
            record(kr1 != KERN_SUCCESS, sec, "IONVRAM write boot-args refused", "kr=0x%x", kr1);

            kern_return_t kr2 = IORegistryEntrySetCFProperty(nvram, CFSTR("kido-test-var"), CFSTR("1"));
            record(kr2 != KERN_SUCCESS, sec, "IONVRAM write custom var refused", "kr=0x%x", kr2);

            IOObjectRelease(nvram);
        } else {
            printf("skip %s: IONVRAM (IODTNVRAM not present)\n", sec);
        }
    }

    {
        io_service_t svc = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching("IOResources"));
        record(svc != IO_OBJECT_NULL, sec, "IOResources service present", "");
        if (svc != IO_OBJECT_NULL) {
            kern_return_t kr = IORegistryEntrySetCFProperty(svc, CFSTR("kido-test-resource"), kCFBooleanTrue);
            record(kr != KERN_SUCCESS, sec, "IOResources setProperty refused", "kr=0x%x", kr);
            IOObjectRelease(svc);
        }
    }

    {
        io_service_t svc = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching("IONetworkStack"));
        record(svc != IO_OBJECT_NULL, sec, "IONetworkStack service present", "");
        if (svc != IO_OBJECT_NULL) {
            int32_t cmd = 2;
            CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &cmd);
            const void *keys[1] = { CFSTR("IONetworkStackUserCommand") };
            const void *vals[1] = { num };
            CFDictionaryRef dict = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, 1,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            kern_return_t kr = IORegistryEntrySetCFProperties(svc, dict);
            record(kr == kIOReturnNotPrivileged, sec, "IONetworkStack setProperties NOT_PRIVILEGED",
                "kr=0x%x", kr);
            if (dict != NULL) {
                CFRelease(dict);
            }
            if (num != NULL) {
                CFRelease(num);
            }
            IOObjectRelease(svc);
        }
    }

    {
        kern_return_t kr1 = IOCatalogueReset(MACH_PORT_NULL, TEST_IOCATALOG_RESET_DEFAULT);
        record(kr1 == kIOReturnNotPrivileged, sec, "IOCatalogueReset NOT_PRIVILEGED", "kr=0x%x", kr1);

        const char dummy_plist[] = "<array/>";
        kern_return_t kr2 = IOCatalogueSendData(MACH_PORT_NULL, TEST_IOCATALOG_ADD_DRIVERS,
            dummy_plist, (uint32_t)sizeof(dummy_plist));
        record(kr2 != KERN_SUCCESS, sec, "IOCatalogueSendData refused", "kr=0x%x", kr2);

        kern_return_t kr3 = IOCatalogueModuleLoaded(MACH_PORT_NULL, "sh.kido.test.fake");
        record(kr3 == kIOReturnNotPrivileged, sec, "IOCatalogueModuleLoaded NOT_PRIVILEGED",
            "kr=0x%x", kr3);
    }

    {
        io_service_t svc = IOServiceGetMatchingService(MACH_PORT_NULL,
            IOServiceMatching("AppleDiskImagesController"));
        record(svc != IO_OBJECT_NULL, sec, "PDDiskImages2 service present", "");
        if (svc != IO_OBJECT_NULL) {
            io_connect_t conn = IO_OBJECT_NULL;
            kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
            record(kr != KERN_SUCCESS, sec, "PDDiskImages2 IOServiceOpen refused", "kr=0x%x", kr);
            if (kr == KERN_SUCCESS && conn != IO_OBJECT_NULL) {
                IOServiceClose(conn);
            }
            IOObjectRelease(svc);
        }
    }

    {
        io_service_t svc = IOServiceGetMatchingService(MACH_PORT_NULL,
            IOServiceMatching("AppleKeyStore"));
        record(svc != IO_OBJECT_NULL, sec, "PDKeyStore service present", "");
        if (svc != IO_OBJECT_NULL) {
            io_connect_t conn = IO_OBJECT_NULL;
            kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
            record(kr == KERN_SUCCESS && conn != IO_OBJECT_NULL, sec,
                "PDKeyStore user client opens for session queries", "kr=0x%x", kr);
            if (kr == KERN_SUCCESS && conn != IO_OBJECT_NULL) {
                uint64_t out_scalar = 0;
                uint32_t out_cnt = 1;
                kern_return_t kr_create = IOConnectCallScalarMethod(conn, 2, NULL, 0, &out_scalar, &out_cnt);
                record(kr_create == kIOReturnNotPrivileged, sec, "PDKeyStore sel 2 bag create NOT_PRIVILEGED",
                    "kr=0x%x", kr_create);

                uint8_t key32[32];
                uint8_t blob40[40];
                uint8_t outbuf[64];
                memset(key32, 0x41, sizeof(key32));
                memset(blob40, 0x42, sizeof(blob40));

                uint64_t handles[] = {
                    0ULL,
                    1ULL,
                    TEST_AKS_HANDLE_DEVICE,
                    TEST_AKS_HANDLE_DEVICE_ALT,
                    TEST_AKS_HANDLE_SESSION,
                    TEST_AKS_HANDLE_FOREIGN_UID,
                    TEST_AKS_HANDLE_INT64_MIN,
                };
                const char *hnames[] = {
                    "handle 0",
                    "handle 1",
                    "handle -6",
                    "handle -1",
                    "handle -3",
                    "handle -501",
                    "handle INT64_MIN",
                };
                for (size_t i = 0; i < sizeof(handles) / sizeof(handles[0]); i++) {
                    uint64_t in[2] = { handles[i], 1ULL };
                    uint64_t sout = 0;
                    uint32_t scnt = 1;
                    size_t osz = sizeof(outbuf);
                    char label[80];

                    kern_return_t kr10 = IOConnectCallMethod(conn, 10, in, 2, key32, sizeof(key32),
                        &sout, &scnt, outbuf, &osz);
                    snprintf(label, sizeof(label), "PDKeyStore sel 10 on %s refused", hnames[i]);
                    record(kr10 == TEST_AKS_ERR_NO_SESSION, sec, label, "kr=0x%x", kr10);

                    osz = sizeof(outbuf);
                    kern_return_t kr11 = IOConnectCallMethod(conn, 11, in, 2, blob40, sizeof(blob40),
                        NULL, NULL, outbuf, &osz);
                    snprintf(label, sizeof(label), "PDKeyStore sel 11 on %s refused", hnames[i]);
                    record(kr11 == TEST_AKS_ERR_NO_SESSION, sec, label, "kr=0x%x", kr11);
                }

                uint64_t uid0 = 0;
                out_cnt = 1;
                kern_return_t kr14 = IOConnectCallScalarMethod(conn, 14, &uid0, 1, &out_scalar, &out_cnt);
                record(kr14 == kIOReturnNotPrivileged, sec, "PDKeyStore sel 14 foreign UID 0 NOT_PRIVILEGED",
                    "kr=0x%x", kr14);

                uint8_t fv_der[16];
                memset(fv_der, 0, sizeof(fv_der));
                uint64_t fv_in[3] = { 0ULL, (uint64_t)(uintptr_t)fv_der, sizeof(fv_der) };
                size_t fv_osz = sizeof(outbuf);
                kern_return_t kr76 = IOConnectCallMethod(conn, 76, fv_in, 3, NULL, 0,
                    NULL, NULL, outbuf, &fv_osz);
                record(kr76 == kIOReturnNotPrivileged, sec, "PDKeyStore sel 76 FileVault NOT_PRIVILEGED",
                    "kr=0x%x", kr76);

                IOServiceClose(conn);
            }
            IOObjectRelease(svc);
        }
    }

    {
        io_service_t svc = IOServiceGetMatchingService(MACH_PORT_NULL,
            IOServiceMatching("com_apple_filesystems_lifs"));
        record(svc != IO_OBJECT_NULL, sec, "PDLifs service present", "");
        if (svc != IO_OBJECT_NULL) {
            io_connect_t conn = IO_OBJECT_NULL;
            kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
            record(kr != KERN_SUCCESS, sec, "PDLifs IOServiceOpen refused", "kr=0x%x", kr);
            if (kr == KERN_SUCCESS && conn != IO_OBJECT_NULL) {
                IOServiceClose(conn);
            }
            IOObjectRelease(svc);
        }
    }

    {
        io_service_t svc = IOServiceGetMatchingService(MACH_PORT_NULL,
            IOServiceMatching("IOWatchdog"));
        record(svc != IO_OBJECT_NULL, sec, "PDWatchdog service present", "");
        if (svc != IO_OBJECT_NULL) {
            io_connect_t conn = IO_OBJECT_NULL;
            kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
            record(kr != KERN_SUCCESS, sec, "PDWatchdog IOServiceOpen refused", "kr=0x%x", kr);
            if (kr == KERN_SUCCESS && conn != IO_OBJECT_NULL) {
                IOServiceClose(conn);
            }
            IOObjectRelease(svc);
        }
    }

    {
        mach_port_t reg_port = make_send_receive_port();
        kern_return_t kr1 = bootstrap_register2(bootstrap_port, "com.apple.kido.test.squat",
            reg_port, 0);
        record(kr1 != KERN_SUCCESS, sec, "launchd bootstrap_register2 undeclared service refused",
            "kr=0x%x", kr1);
        destroy_send_receive_port(reg_port);

        mach_port_t chk_port = MACH_PORT_NULL;
        kern_return_t kr2 = bootstrap_check_in(bootstrap_port, "com.apple.kido.test.squat",
            &chk_port);
        record(kr2 != KERN_SUCCESS, sec, "launchd bootstrap_check_in undeclared service refused",
            "kr=0x%x", kr2);
        if (kr2 == KERN_SUCCESS && chk_port != MACH_PORT_NULL) {
            mach_port_destruct(mach_task_self(), chk_port, 0, 0);
        }

        mach_port_t parent_port = MACH_PORT_NULL;
        kern_return_t kr3 = launchd_mig_noargs(bootstrap_port, TEST_LAUNCHD_MIG_PARENT, &parent_port);
        record(kr3 == TEST_BOOTSTRAP_NOT_PRIVILEGED, sec, "launchd bootstrap_parent NOT_PRIVILEGED",
            "kr=%d", kr3);
        if (parent_port != MACH_PORT_NULL) {
            mach_port_deallocate(mach_task_self(), parent_port);
        }

        mach_port_t root_port = MACH_PORT_NULL;
        kern_return_t kr4 = launchd_mig_noargs(bootstrap_port, TEST_LAUNCHD_MIG_GET_ROOT_BOOTSTRAP,
            &root_port);
        record(kr4 == TEST_BOOTSTRAP_NOT_PRIVILEGED, sec, "launchd bootstrap_get_root NOT_PRIVILEGED",
            "kr=%d", kr4);
        if (root_port != MACH_PORT_NULL) {
            mach_port_deallocate(mach_task_self(), root_port);
        }

        int64_t new_umask = 077;
        void *verr = vproc_swap_integer(NULL, TEST_VPROC_GSK_GLOBAL_UMASK, &new_umask, NULL);
        record(verr != NULL, sec, "launchd swap_integer(GLOBAL_UMASK) refused", "err=%p", verr);
    }

    {
        int tok = 0;
        uint32_t st_reg = notify_register_plain("com.apple.system.kido-test", &tok);
        record(st_reg == NOTIFY_STATUS_OK, sec,
            "notify_register_plain(com.apple.system.kido-test) allowed", "status=%u", st_reg);
        uint32_t st1 = notify_post("com.apple.system.kido-test");
        uint32_t st2 = notify_post("com.apple.system.kido-test");
        record(st2 == NOTIFY_STATUS_NOT_AUTHORIZED || st2 == NOTIFY_STATUS_FAILED, sec,
            "notify_post(com.apple.system.kido-test) sync post refused",
            "st1=%u st2=%u", st1, st2);
        if (st_reg == NOTIFY_STATUS_OK) {
            notify_cancel(tok);
        }

        int tok2 = 0;
        uint32_t st_chk = notify_register_check("com.apple.system.kido-test-chk", &tok2);
        if (st_chk == NOTIFY_STATUS_OK) {
            int chk = 0;
            uint64_t state = 0;
            (void)notify_check(tok2, &chk);
            (void)notify_check(tok2, &chk);
            (void)notify_post("com.apple.system.kido-test-chk");
            (void)notify_set_state(tok2, 0x42ULL);
            (void)notify_get_state(tok2, &state);
            (void)notify_check(tok2, &chk);
            record(state == 0 && chk == 0, sec,
                "notifyd drops unprivileged post and set_state on com.apple.system.*",
                "state=%llu chk=%d", (unsigned long long)state, chk);
            notify_cancel(tok2);
        } else {
            record(false, sec, "notify_register_check(com.apple.system.kido-test-chk)",
                "status=%u", st_chk);
        }
    }

    {
        errno = 0;
        int fd = open("/etc/master.passwd", O_RDONLY);
        record(fd == -1 && errno == EACCES, sec, "/etc/master.passwd unreadable as nobody",
            "fd=%d errno=%d", fd, errno);
        if (fd >= 0) {
            close(fd);
        }
    }

    printf("restrictions (%s): %d/%d passed\n", sec, g_run - g_fail, g_run);
    return g_fail != 0;
}

static int
run_root_tests(const char *restricted_bin)
{
    const char *sec = "root";

    record(geteuid() == 0, sec, "running as root", "euid=%u", (unsigned)geteuid());

    {
        uint64_t slide = 0;
        size_t sz = sizeof(slide);
        errno = 0;
        int r = syscall(TEST_SYS_KAS_INFO, TEST_KAS_INFO_KERNEL_TEXT_SLIDE, &slide, &sz);
        if (r == -1 && errno == ENOTSUP) {
            printf("skip %s: kas_info (CONFIG_KAS_INFO disabled)\n", sec);
        } else {
            record(r == -1 && errno == EPERM, sec, "kas_info(KERNEL_TEXT_SLIDE) EPERM",
                "r=%d errno=%d", r, errno);
        }
    }

    {
        char tmpl[] = "/tmp/kido-chflags.XXXXXX";
        int fd = mkstemp(tmpl);
        record(fd >= 0, sec, "create chflags probe file", "errno=%d", errno);
        if (fd >= 0) {
            close(fd);
            errno = 0;
            int r1 = chflags(tmpl, TEST_SF_RESTRICTED);
            record(r1 == -1 && errno == EPERM, sec, "chflags(SF_RESTRICTED) EPERM",
                "r=%d errno=%d", r1, errno);

            errno = 0;
            int r2 = chflags(tmpl, TEST_UF_DATAVAULT);
            record(r2 == -1 && errno == EPERM, sec, "chflags(UF_DATAVAULT) EPERM",
                "r=%d errno=%d", r2, errno);

            unlink(tmpl);
        }
    }

    {
        io_registry_entry_t nvram = find_nvram();
        if (nvram != IO_OBJECT_NULL) {
            kern_return_t kr1 = IORegistryEntrySetCFProperty(nvram, CFSTR("boot-args"), CFSTR("debug=0x1"));
            record(kr1 != KERN_SUCCESS, sec, "IONVRAM write boot-args refused under SIP", "kr=0x%x", kr1);

            kern_return_t kr2 = IORegistryEntrySetCFProperty(nvram, CFSTR("csr-active-config"), CFSTR("w%00%00%00"));
            record(kr2 != KERN_SUCCESS, sec, "IONVRAM write csr-active-config refused under SIP", "kr=0x%x", kr2);

            kern_return_t kr3 = IORegistryEntrySetCFProperty(nvram, CFSTR("boot-device"), CFSTR("test"));
            record(kr3 != KERN_SUCCESS, sec, "IONVRAM write boot-device refused under SIP", "kr=0x%x", kr3);

            IOObjectRelease(nvram);
        } else {
            printf("skip %s: IONVRAM (IODTNVRAM not present)\n", sec);
        }
    }

    {
        mach_port_name_t port = MACH_PORT_NULL;
        kern_return_t kr = task_for_pid(mach_task_self(), 1, &port);
        record(kr != KERN_SUCCESS, sec, "task_for_pid(pid=1) refused under SIP", "kr=0x%x", kr);
        if (kr == KERN_SUCCESS && port != MACH_PORT_NULL && port != mach_task_self()) {
            mach_port_deallocate(mach_task_self(), port);
        }
    }

    {
        mach_port_name_t port = MACH_PORT_NULL;
        kern_return_t kr = task_for_pid(mach_task_self(), getpid(), &port);
        record(kr == KERN_SUCCESS && port != MACH_PORT_NULL, sec,
            "task_for_pid(self) succeeds", "kr=0x%x port=0x%x", kr, port);
        if (kr == KERN_SUCCESS && port != MACH_PORT_NULL && port != mach_task_self()) {
            mach_port_deallocate(mach_task_self(), port);
        }
    }

    {
        errno = 0;
        int r = ptrace(PT_ATTACH, 1, 0, 0);
        record(r == -1 && errno == EPERM, sec, "ptrace(PT_ATTACH, pid=1) EPERM under SIP",
            "r=%d errno=%d", r, errno);
        if (r == 0) {
            ptrace(PT_DETACH, 1, 0, 0);
        }
    }

    if (restricted_bin != NULL) {
        int pfd[2];
        if (pipe(pfd) == 0) {
            fcntl(pfd[1], F_SETFD, FD_CLOEXEC);
            pid_t child = fork();
            if (child == 0) {
                close(pfd[0]);
                char *cargv[] = { (char *)restricted_bin, "--wait", NULL };
                execv(restricted_bin, cargv);
                _exit(127);
            }
            close(pfd[1]);
            char ch = 0;
            (void)read(pfd[0], &ch, 1);
            close(pfd[0]);
            if (child > 0) {
                errno = 0;
                int r = ptrace(PT_ATTACH, child, 0, 0);
                record(r == -1 && errno == EPERM, sec,
                    "ptrace(PT_ATTACH, CS_RESTRICT child) EPERM under SIP",
                    "r=%d errno=%d", r, errno);
                if (r == 0) {
                    ptrace(PT_DETACH, child, 0, 0);
                }
                kill(child, SIGKILL);
                waitpid(child, NULL, 0);
            } else {
                record(false, sec, "fork CS_RESTRICT child for ptrace test", "errno=%d", errno);
            }
        } else {
            record(false, sec, "pipe for ptrace test", "errno=%d", errno);
        }
    }

    {
        pid_t child = fork();
        if (child == 0) {
            mach_port_t host = mach_host_self();
            mach_port_t exc_port = make_send_receive_port();
            kern_return_t kr = host_set_exception_ports(host, EXC_MASK_BAD_ACCESS, exc_port,
                EXCEPTION_STATE, THREAD_STATE_NONE);
            destroy_send_receive_port(exc_port);
            mach_port_deallocate(mach_task_self(), host);
            _exit(kr == KERN_NO_ACCESS ? 0 : (kr != KERN_SUCCESS ? 1 : 2));
        }
        int st = 0;
        if (child > 0 && waitpid(child, &st, 0) == child) {
            bool refused = (WIFEXITED(st) && (WEXITSTATUS(st) == 0 || WEXITSTATUS(st) == 1)) ||
                WIFSIGNALED(st);
            record(refused, sec, "host_set_exception_ports refused under SIP",
                "exited=%d code=%d sig=%d", WIFEXITED(st),
                WIFEXITED(st) ? WEXITSTATUS(st) : 0,
                WIFSIGNALED(st) ? WTERMSIG(st) : 0);
        } else {
            record(false, sec, "host_set_exception_ports child wait", "errno=%d", errno);
        }
    }

    {
        mach_port_t host = mach_host_self();
        mach_port_t sp_port = make_send_receive_port();
        kern_return_t kr1 = host_set_special_port(host, TEST_HOST_ATM_NOTIFICATION_PORT, sp_port);
        record(kr1 != KERN_SUCCESS, sec, "host_set_special_port(ATM) refused under SIP", "kr=0x%x", kr1);

        kern_return_t kr2 = host_set_special_port(host, TEST_HOST_TELEMETRY_PORT, sp_port);
        record(kr2 != KERN_SUCCESS, sec, "host_set_special_port(TELEMETRY) refused under SIP", "kr=0x%x", kr2);

        destroy_send_receive_port(sp_port);
        mach_port_deallocate(mach_task_self(), host);
    }

    {
        io_service_t svc = IOServiceGetMatchingService(MACH_PORT_NULL,
            IOServiceMatching("AppleKeyStore"));
        if (svc != IO_OBJECT_NULL) {
            io_connect_t conn = IO_OBJECT_NULL;
            if (IOServiceOpen(svc, mach_task_self(), 0, &conn) == KERN_SUCCESS && conn != IO_OBJECT_NULL) {
                uint8_t key32[32];
                uint8_t outbuf[64];
                uint64_t in[2] = { 0ULL, 1ULL };
                uint64_t sout = 0;
                uint32_t scnt = 1;
                size_t osz = sizeof(outbuf);
                memset(key32, 0x41, sizeof(key32));
                kern_return_t kr0 = IOConnectCallMethod(conn, 10, in, 2, key32, sizeof(key32),
                    &sout, &scnt, outbuf, &osz);
                record(kr0 == TEST_AKS_ERR_NO_SESSION, sec,
                    "PDKeyStore rejects handle 0 for root", "kr=0x%x", kr0);
                IOServiceClose(conn);
            }
            IOObjectRelease(svc);
        }
    }

    {
        io_service_t svc = IOServiceGetMatchingService(MACH_PORT_NULL,
            IOServiceMatching("AppleDiskImagesController"));
        if (svc != IO_OBJECT_NULL) {
            io_connect_t conn = IO_OBJECT_NULL;
            kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
            record(kr == KERN_SUCCESS && conn != IO_OBJECT_NULL, sec,
                "PDDiskImages2 opens for root", "kr=0x%x", kr);
            if (kr == KERN_SUCCESS && conn != IO_OBJECT_NULL) {
                uint8_t req[TEST_DI_CREATE_SIZE];
                uint8_t resp[16];
                size_t rsz = sizeof(resp);
                uint64_t blocks = 0;
                uint32_t bsize = 512;

                memset(req, 0, sizeof(req));
                req[TEST_DI_CREATE_LOCATION] = 2;
                memcpy(req + TEST_DI_CREATE_BLOCKS, &blocks, sizeof(blocks));
                memcpy(req + TEST_DI_CREATE_BLOCK_SIZE, &bsize, sizeof(bsize));
                kern_return_t kr_zero = IOConnectCallStructMethod(conn, 0, req, sizeof(req), resp, &rsz);
                record(kr_zero == kIOReturnBadArgument, sec, "PDDiskImages2 rejects zero blocks",
                    "kr=0x%x", kr_zero);

                blocks = 1ULL << 30;
                bsize = 4096;
                rsz = sizeof(resp);
                memcpy(req + TEST_DI_CREATE_BLOCKS, &blocks, sizeof(blocks));
                memcpy(req + TEST_DI_CREATE_BLOCK_SIZE, &bsize, sizeof(bsize));
                kern_return_t kr_big = IOConnectCallStructMethod(conn, 0, req, sizeof(req), resp, &rsz);
                record(kr_big == kIOReturnBadArgument, sec, "PDDiskImages2 rejects >1 GiB backing",
                    "kr=0x%x", kr_big);

                blocks = (1ULL << 52) + 1ULL;
                bsize = 4096;
                rsz = sizeof(resp);
                memcpy(req + TEST_DI_CREATE_BLOCKS, &blocks, sizeof(blocks));
                memcpy(req + TEST_DI_CREATE_BLOCK_SIZE, &bsize, sizeof(bsize));
                kern_return_t kr_ovf = IOConnectCallStructMethod(conn, 0, req, sizeof(req), resp, &rsz);
                record(kr_ovf == kIOReturnBadArgument, sec, "PDDiskImages2 rejects blocks*blockSize overflow",
                    "kr=0x%x", kr_ovf);

                blocks = 16;
                bsize = 1000;
                rsz = sizeof(resp);
                memcpy(req + TEST_DI_CREATE_BLOCKS, &blocks, sizeof(blocks));
                memcpy(req + TEST_DI_CREATE_BLOCK_SIZE, &bsize, sizeof(bsize));
                kern_return_t kr_bs = IOConnectCallStructMethod(conn, 0, req, sizeof(req), resp, &rsz);
                record(kr_bs == kIOReturnBadArgument, sec, "PDDiskImages2 rejects non-power-of-two blockSize",
                    "kr=0x%x", kr_bs);

                IOServiceClose(conn);
            }
            IOObjectRelease(svc);
        }
    }

    printf("restrictions (%s): %d/%d passed\n", sec, g_run - g_fail, g_run);
    return g_fail != 0;
}

static int
stat_path(const char *path, struct stat *st)
{
    int fd = open(path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) {
        return -1;
    }
    int r = fstat(fd, st);
    int saved = errno;
    close(fd);
    errno = saved;
    return r;
}

static int
run_files_tests(void)
{
    const char *sec = "files";
    struct stat st;

    {
        errno = 0;
        int r = stat_path("/etc/master.passwd", &st);
        record(r == 0 && (st.st_mode & 0777) == 0600, sec,
            "/etc/master.passwd exists with mode 0600",
            "r=%d errno=%d mode=0%o", r, errno, r == 0 ? (unsigned)(st.st_mode & 0777) : 0U);
    }

    {
        if (stat_path("/var/log/dmesg-boot.0.txt", &st) == 0) {
            record((st.st_mode & 0777) == 0600, sec,
                "/var/log/dmesg-boot.0.txt mode 0600", "mode=0%o", (unsigned)(st.st_mode & 0777));
        } else {
            printf("skip %s: /var/log/dmesg-boot.0.txt not present\n", sec);
        }
    }

    {
        if (stat_path("/var/log/system.log", &st) == 0) {
            record((st.st_mode & 0777) == 0640, sec,
                "/var/log/system.log mode 0640", "mode=0%o", (unsigned)(st.st_mode & 0777));
        } else {
            printf("skip %s: /var/log/system.log not present\n", sec);
        }
    }

    {
        int n = proc_listallpids(NULL, 0);
        pid_t *pids = calloc((size_t)(n > 0 ? n : 64) + 64, sizeof(pid_t));
        pid_t found = -1;
        if (pids != NULL) {
            int got = proc_listallpids(pids, (int)(((size_t)(n > 0 ? n : 64) + 64) * sizeof(pid_t)));
            for (int i = 0; i < got && found < 0; i++) {
                char pname[2 * MAXCOMLEN + 1];
                memset(pname, 0, sizeof(pname));
                if (proc_name(pids[i], pname, (uint32_t)sizeof(pname)) > 0 &&
                    strcmp(pname, "doubleagentd") == 0) {
                    found = pids[i];
                }
            }
            free(pids);
        }
        record(found > 0, sec, "doubleagentd process running", "pid=%d", (int)found);
        if (found > 0) {
            struct proc_bsdinfo bsd;
            memset(&bsd, 0, sizeof(bsd));
            int sz = proc_pidinfo(found, PROC_PIDTBSDINFO, 0, &bsd, sizeof(bsd));
            record(sz == (int)sizeof(bsd) && bsd.pbi_uid == TEST_UID_DOUBLEAGENTD &&
                bsd.pbi_ruid == TEST_UID_DOUBLEAGENTD, sec,
                "doubleagentd runs as UID 283 (_doubleagentd)",
                "sz=%d uid=%u ruid=%u", sz, (unsigned)bsd.pbi_uid, (unsigned)bsd.pbi_ruid);
        }
    }

    printf("restrictions (%s): %d/%d passed\n", sec, g_run - g_fail, g_run);
    return g_fail != 0;
}

int
main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc < 2) {
        fprintf(stderr, "usage: %s unpriv|root|files\n", argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "unpriv") == 0) {
        if (geteuid() == 0) {
            seed_root_keybag();
            if (setgroups(0, NULL) != 0 || setgid(TEST_UID_NOBODY) != 0 || setuid(TEST_UID_NOBODY) != 0) {
                perror("drop privileges");
                return 1;
            }
            char *next_argv[] = { argv[0], "unpriv-exec", NULL };
            execv(argv[0], next_argv);
            perror("execv");
            return 1;
        }
        return run_unpriv_tests();
    }
    if (strcmp(argv[1], "unpriv-exec") == 0) {
        return run_unpriv_tests();
    }
    if (strcmp(argv[1], "root") == 0) {
        return run_root_tests(argc >= 3 ? argv[2] : NULL);
    }
    if (strcmp(argv[1], "files") == 0) {
        return run_files_tests();
    }

    fprintf(stderr, "unknown mode: %s\n", argv[1]);
    return 2;
}
