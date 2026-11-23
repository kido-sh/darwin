/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The launch_data interface between launchd, launchctl and liblaunch: a
 * property-list value tree, launch_msg() to send one to launchd, and the
 * property names of a job.
 */

#ifndef KIDO_LAUNCH_H
#define KIDO_LAUNCH_H

#include <xpc/base.h>
#include <mach/mach.h>
#include <stdbool.h>
#include <stddef.h>

__BEGIN_DECLS

/* Requests understood by launch_msg() */
#define LAUNCH_KEY_CHECKIN "CheckIn"
#define LAUNCH_KEY_GETJOB "GetJob"
#define LAUNCH_KEY_GETJOBS "GetJobs"
#define LAUNCH_KEY_REMOVEJOB "RemoveJob"
#define LAUNCH_KEY_STARTJOB "StartJob"
#define LAUNCH_KEY_STOPJOB "StopJob"
#define LAUNCH_KEY_SUBMITJOB "SubmitJob"

/* Top-level properties of a job */
#define LAUNCH_JOBKEY_ABANDONPROCESSGROUP "AbandonProcessGroup"
#define LAUNCH_JOBKEY_CFBUNDLEIDENTIFIER "CFBundleIdentifier"
#define LAUNCH_JOBKEY_DEBUG "Debug"
#define LAUNCH_JOBKEY_DISABLED "Disabled"
#define LAUNCH_JOBKEY_ENABLEGLOBBING "EnableGlobbing"
#define LAUNCH_JOBKEY_ENABLETRANSACTIONS "EnableTransactions"
#define LAUNCH_JOBKEY_ENVIRONMENTVARIABLES "EnvironmentVariables"
#define LAUNCH_JOBKEY_EXITTIMEOUT "ExitTimeOut"
#define LAUNCH_JOBKEY_GROUPNAME "GroupName"
#define LAUNCH_JOBKEY_HARDRESOURCELIMITS "HardResourceLimits"
#define LAUNCH_JOBKEY_HOPEFULLYEXITSLAST "HopefullyExitsLast"
#define LAUNCH_JOBKEY_IGNOREPROCESSGROUPATSHUTDOWN "IgnoreProcessGroupAtShutdown"
#define LAUNCH_JOBKEY_INETDCOMPATIBILITY "inetdCompatibility"
#define LAUNCH_JOBKEY_INITGROUPS "InitGroups"
#define LAUNCH_JOBKEY_KEEPALIVE "KeepAlive"
#define LAUNCH_JOBKEY_LABEL "Label"
#define LAUNCH_JOBKEY_LASTEXITSTATUS "LastExitStatus"
#define LAUNCH_JOBKEY_LAUNCHEVENTS "LaunchEvents"
#define LAUNCH_JOBKEY_LAUNCHONLYONCE "LaunchOnlyOnce"
#define LAUNCH_JOBKEY_LEGACYTIMERS "LegacyTimers"
#define LAUNCH_JOBKEY_LIMITLOADFROMHARDWARE "LimitLoadFromHardware"
#define LAUNCH_JOBKEY_LIMITLOADFROMHOSTS "LimitLoadFromHosts"
#define LAUNCH_JOBKEY_LIMITLOADTOHARDWARE "LimitLoadToHardware"
#define LAUNCH_JOBKEY_LIMITLOADTOHOSTS "LimitLoadToHosts"
#define LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE "LimitLoadToSessionType"
#define LAUNCH_JOBKEY_LOWPRIORITYBACKGROUNDIO "LowPriorityBackgroundIO"
#define LAUNCH_JOBKEY_LOWPRIORITYIO "LowPriorityIO"
#define LAUNCH_JOBKEY_MACHSERVICES "MachServices"
#define LAUNCH_JOBKEY_NICE "Nice"
#define LAUNCH_JOBKEY_ONDEMAND "OnDemand"
#define LAUNCH_JOBKEY_PID "PID"
#define LAUNCH_JOBKEY_POLICIES "Policies"
#define LAUNCH_JOBKEY_PROCESSTYPE "ProcessType"
#define LAUNCH_JOBKEY_PROGRAM "Program"
#define LAUNCH_JOBKEY_PROGRAMARGUMENTS "ProgramArguments"
#define LAUNCH_JOBKEY_QUEUEDIRECTORIES "QueueDirectories"
#define LAUNCH_JOBKEY_ROOTDIRECTORY "RootDirectory"
#define LAUNCH_JOBKEY_RUNATLOAD "RunAtLoad"
#define LAUNCH_JOBKEY_SESSIONCREATE "SessionCreate"
#define LAUNCH_JOBKEY_SOCKETS "Sockets"
#define LAUNCH_JOBKEY_SOFTRESOURCELIMITS "SoftResourceLimits"
#define LAUNCH_JOBKEY_STANDARDERRORPATH "StandardErrorPath"
#define LAUNCH_JOBKEY_STANDARDINPATH "StandardInPath"
#define LAUNCH_JOBKEY_STANDARDOUTPATH "StandardOutPath"
#define LAUNCH_JOBKEY_STARTCALENDARINTERVAL "StartCalendarInterval"
#define LAUNCH_JOBKEY_STARTINTERVAL "StartInterval"
#define LAUNCH_JOBKEY_STARTONMOUNT "StartOnMount"
#define LAUNCH_JOBKEY_THROTTLEINTERVAL "ThrottleInterval"
#define LAUNCH_JOBKEY_TIMEOUT "TimeOut"
#define LAUNCH_JOBKEY_UMASK "Umask"
#define LAUNCH_JOBKEY_USERENVIRONMENTVARIABLES "UserEnvironmentVariables"
#define LAUNCH_JOBKEY_USERNAME "UserName"
#define LAUNCH_JOBKEY_WAITFORDEBUGGER "WaitForDebugger"
#define LAUNCH_JOBKEY_WATCHPATHS "WatchPaths"
#define LAUNCH_JOBKEY_WORKINGDIRECTORY "WorkingDirectory"

/* Inside KeepAlive */
#define LAUNCH_JOBKEY_KEEPALIVE_AFTERINITIALDEMAND "AfterInitialDemand"
#define LAUNCH_JOBKEY_KEEPALIVE_CRASHED "Crashed"
#define LAUNCH_JOBKEY_KEEPALIVE_NETWORKSTATE "NetworkState"
#define LAUNCH_JOBKEY_KEEPALIVE_OTHERJOBACTIVE "OtherJobActive"
#define LAUNCH_JOBKEY_KEEPALIVE_OTHERJOBENABLED "OtherJobEnabled"
#define LAUNCH_JOBKEY_KEEPALIVE_PATHSTATE "PathState"
#define LAUNCH_JOBKEY_KEEPALIVE_SUCCESSFULEXIT "SuccessfulExit"

/* Inside StartCalendarInterval */
#define LAUNCH_JOBKEY_CAL_DAY "Day"
#define LAUNCH_JOBKEY_CAL_HOUR "Hour"
#define LAUNCH_JOBKEY_CAL_MINUTE "Minute"
#define LAUNCH_JOBKEY_CAL_MONTH "Month"
#define LAUNCH_JOBKEY_CAL_WEEKDAY "Weekday"

/* Inside SoftResourceLimits and HardResourceLimits */
#define LAUNCH_JOBKEY_RESOURCELIMIT_CORE "Core"
#define LAUNCH_JOBKEY_RESOURCELIMIT_CPU "CPU"
#define LAUNCH_JOBKEY_RESOURCELIMIT_DATA "Data"
#define LAUNCH_JOBKEY_RESOURCELIMIT_FSIZE "FileSize"
#define LAUNCH_JOBKEY_RESOURCELIMIT_MEMLOCK "MemoryLock"
#define LAUNCH_JOBKEY_RESOURCELIMIT_NOFILE "NumberOfFiles"
#define LAUNCH_JOBKEY_RESOURCELIMIT_NPROC "NumberOfProcesses"
#define LAUNCH_JOBKEY_RESOURCELIMIT_RSS "ResidentSetSize"
#define LAUNCH_JOBKEY_RESOURCELIMIT_STACK "Stack"

/* Inside a MachServices entry */
#define LAUNCH_JOBKEY_MACH_DRAINMESSAGESONCRASH "DrainMessagesOnCrash"
#define LAUNCH_JOBKEY_MACH_HIDEUNTILCHECKIN "HideUntilCheckIn"
#define LAUNCH_JOBKEY_MACH_RESETATCLOSE "ResetAtClose"

/* Inside a Disabled dictionary */
#define LAUNCH_JOBKEY_DISABLED_MACHINETYPE "MachineType"
#define LAUNCH_JOBKEY_DISABLED_MODELNAME "ModelName"

/* Inside a Sockets entry */
#define LAUNCH_JOBSOCKETKEY_BONJOUR "Bonjour"
#define LAUNCH_JOBSOCKETKEY_FAMILY "SockFamily"
#define LAUNCH_JOBSOCKETKEY_MULTICASTGROUP "MulticastGroup"
#define LAUNCH_JOBSOCKETKEY_NODENAME "SockNodeName"
#define LAUNCH_JOBSOCKETKEY_PASSIVE "SockPassive"
#define LAUNCH_JOBSOCKETKEY_PATHMODE "SockPathMode"
#define LAUNCH_JOBSOCKETKEY_PATHNAME "SockPathName"
#define LAUNCH_JOBSOCKETKEY_PROTOCOL "SockProtocol"
#define LAUNCH_JOBSOCKETKEY_SECUREWITHKEY "SecureSocketWithKey"
#define LAUNCH_JOBSOCKETKEY_SERVICENAME "SockServiceName"
#define LAUNCH_JOBSOCKETKEY_TYPE "SockType"

/* Values of ProcessType */
#define LAUNCH_KEY_PROCESSTYPE_ADAPTIVE "Adaptive"
#define LAUNCH_KEY_PROCESSTYPE_APP "App"
#define LAUNCH_KEY_PROCESSTYPE_BACKGROUND "Background"
#define LAUNCH_KEY_PROCESSTYPE_INTERACTIVE "Interactive"
#define LAUNCH_KEY_PROCESSTYPE_STANDARD "Standard"

/* Inside inetdCompatibility */
#define LAUNCH_JOBINETDCOMPATIBILITY_WAIT "Wait"

/* Inside Policies */
#define LAUNCH_JOBPOLICY_DENYCREATINGOTHERJOBS "DenyCreatingOtherJobs"

typedef struct _launch_data *launch_data_t;

typedef enum {
	LAUNCH_DATA_DICTIONARY = 1,
	LAUNCH_DATA_ARRAY = 2,
	LAUNCH_DATA_FD = 3,
	LAUNCH_DATA_INTEGER = 4,
	LAUNCH_DATA_REAL = 5,
	LAUNCH_DATA_BOOL = 6,
	LAUNCH_DATA_STRING = 7,
	LAUNCH_DATA_OPAQUE = 8,
	LAUNCH_DATA_ERRNO = 9,
	LAUNCH_DATA_MACHPORT = 10
} launch_data_type_t;

typedef void (*launch_data_dict_iterator_t)(const launch_data_t value, const char *key, void *context);

/* Sends a request to launchd and returns its reply, or NULL with errno set. */
__XPC_API launch_data_t launch_msg(const launch_data_t request);
__XPC_API int launch_get_fd(void);

__XPC_API launch_data_t launch_data_alloc(launch_data_type_t type);
__XPC_API launch_data_t launch_data_copy(launch_data_t data);
__XPC_API void launch_data_free(launch_data_t data);
__XPC_API launch_data_type_t launch_data_get_type(const launch_data_t data);

/* Dictionaries */
__XPC_API bool launch_data_dict_insert(launch_data_t dict, const launch_data_t value, const char *key);
__XPC_API launch_data_t launch_data_dict_lookup(const launch_data_t dict, const char *key);
__XPC_API bool launch_data_dict_remove(launch_data_t dict, const char *key);
__XPC_API void launch_data_dict_iterate(const launch_data_t dict, launch_data_dict_iterator_t iterator,
    void *context);
__XPC_API size_t launch_data_dict_get_count(const launch_data_t dict);

/* Arrays */
__XPC_API bool launch_data_array_set_index(launch_data_t array, const launch_data_t value, size_t index);
__XPC_API launch_data_t launch_data_array_get_index(const launch_data_t array, size_t index);
__XPC_API size_t launch_data_array_get_count(const launch_data_t array);

/* Leaf values */
__XPC_API launch_data_t launch_data_new_fd(int fd);
__XPC_API launch_data_t launch_data_new_machport(mach_port_t port);
__XPC_API launch_data_t launch_data_new_integer(long long value);
__XPC_API launch_data_t launch_data_new_bool(bool value);
__XPC_API launch_data_t launch_data_new_real(double value);
__XPC_API launch_data_t launch_data_new_string(const char *string);
__XPC_API launch_data_t launch_data_new_opaque(const void *bytes, size_t length);

__XPC_API bool launch_data_set_fd(launch_data_t data, int fd);
__XPC_API bool launch_data_set_machport(launch_data_t data, mach_port_t port);
__XPC_API bool launch_data_set_integer(launch_data_t data, long long value);
__XPC_API bool launch_data_set_bool(launch_data_t data, bool value);
__XPC_API bool launch_data_set_real(launch_data_t data, double value);
__XPC_API bool launch_data_set_string(launch_data_t data, const char *string);
__XPC_API bool launch_data_set_opaque(launch_data_t data, const void *bytes, size_t length);

__XPC_API int launch_data_get_fd(const launch_data_t data);
__XPC_API mach_port_t launch_data_get_machport(const launch_data_t data);
__XPC_API long long launch_data_get_integer(const launch_data_t data);
__XPC_API bool launch_data_get_bool(const launch_data_t data);
__XPC_API double launch_data_get_real(const launch_data_t data);
__XPC_API const char *launch_data_get_string(const launch_data_t data);
__XPC_API void *launch_data_get_opaque(const launch_data_t data);
__XPC_API size_t launch_data_get_opaque_size(const launch_data_t data);
__XPC_API int launch_data_get_errno(const launch_data_t data);

__END_DECLS

#endif
