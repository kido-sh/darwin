/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/types.h>
#include <sys/disk.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <crt_externs.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <mach-o/dyld.h>
#include <mach/machine.h>
#include <netdb.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HOSTNAME_FOUND 0
#define HOSTNAME_INCORRECTNAME 1
#define HOSTNAME_INVALIDADDR 2
#define HOSTNAME_INVALIDNAME 3

#define REEXEC_GUARD "__KIDO_LIBUTIL_REEXEC"
#define WIPEFS_SPAN ((off_t)2 * 1024 * 1024)

uint32_t notify_post(const char *name);

int
expand_number(const char *buf, uint64_t *num)
{
	static const char units[] = "kmgtpe";
	const char *p = buf, *unit;
	char *end;
	uint64_t n;
	unsigned shift = 0;

	while (isspace((unsigned char)*p))
		p++;
	if (*p == '-') {
		errno = EINVAL;
		return -1;
	}
	errno = 0;
	n = strtoumax(p, &end, 0);
	if (end == p) {
		errno = EINVAL;
		return -1;
	}
	if (errno == ERANGE)
		return -1;
	if (*end != '\0' && (unit = strchr(units, tolower((unsigned char)*end))) != NULL) {
		shift = 10 * (unsigned)(unit - units + 1);
		end++;
	}
	if (*end == 'b' || *end == 'B')
		end++;
	if (*end != '\0') {
		errno = EINVAL;
		return -1;
	}
	if (shift != 0 && n > (UINT64_MAX >> shift)) {
		errno = ERANGE;
		return -1;
	}
	*num = n << shift;
	return 0;
}

struct mntopt {
	const char *m_option;
	int m_inverse;
	int m_flag;
	int m_altloc;
};

struct mntoptparse {
	char *buf;
	const struct mntopt *table;
	char **values;
};

typedef struct mntoptparse *mntoptparse_t;

int getmnt_silent = 0;

static long
mntopt_index(const struct mntopt *table, const char *name)
{
	long i;

	for (i = 0; table[i].m_option != NULL; i++)
		if (strcasecmp(table[i].m_option, name) == 0)
			return i;
	return -1;
}

void
freemntopts(mntoptparse_t mp)
{
	if (mp == NULL)
		return;
	free(mp->buf);
	free(mp->values);
	free(mp);
}

mntoptparse_t
getmntopts(const char *options, const struct mntopt *table, int *flagp, int *altflagp)
{
	mntoptparse_t mp;
	char *rest, *opt, *value;
	size_t n = 0;
	long i;
	int negative, *flags;

	while (table[n].m_option != NULL)
		n++;
	if ((mp = calloc(1, sizeof(*mp))) == NULL)
		return NULL;
	mp->table = table;
	mp->buf = strdup(options);
	mp->values = calloc(n + 1, sizeof(*mp->values));
	if (mp->buf == NULL || mp->values == NULL) {
		freemntopts(mp);
		return NULL;
	}
	for (rest = mp->buf; (opt = strsep(&rest, ",")) != NULL;) {
		if (*opt == '\0')
			continue;
		if ((value = strchr(opt, '=')) != NULL)
			*value++ = '\0';
		negative = strncmp(opt, "no", 2) == 0;
		i = mntopt_index(table, negative ? opt + 2 : opt);
		if (i < 0 && negative) {
			negative = 0;
			i = mntopt_index(table, opt);
		}
		if (i < 0) {
			if (!getmnt_silent)
				errx(1, "-o %s: option not supported", opt);
			continue;
		}
		mp->values[i] = value;
		flags = table[i].m_altloc ? altflagp : flagp;
		if (flags == NULL)
			continue;
		if (negative == (table[i].m_inverse != 0))
			*flags |= table[i].m_flag;
		else
			*flags &= ~table[i].m_flag;
	}
	return mp;
}

const char *
getmntoptstr(mntoptparse_t mp, const char *opt)
{
	long i = mntopt_index(mp->table, opt);

	if (i < 0 || mp->values[i] == NULL) {
		if (!getmnt_silent)
			errx(1, "Missing %s argument", opt);
		return NULL;
	}
	return mp->values[i];
}

long
getmntoptnum(mntoptparse_t mp, const char *opt)
{
	const char *val = getmntoptstr(mp, opt);
	char *end;
	long v;

	if (val == NULL)
		return -1;
	errno = 0;
	v = strtol(val, &end, 0);
	if (end == val || *end != '\0' || errno == ERANGE) {
		if (!getmnt_silent)
			errx(1, "Invalid %s argument `%s'", opt, val);
		return -1;
	}
	return v;
}

struct pidfh {
	int pf_fd;
	char pf_path[MAXPATHLEN + 1];
	dev_t pf_dev;
	ino_t pf_ino;
};

static bool
pidfile_valid(const struct pidfh *pfh)
{
	struct stat st;

	return pfh->pf_fd >= 0 && fstat(pfh->pf_fd, &st) == 0 && st.st_dev == pfh->pf_dev &&
	    st.st_ino == pfh->pf_ino;
}

static pid_t
pidfile_holder(const char *path)
{
	char buf[16], *end;
	ssize_t n;
	long v;
	int fd, tries;

	for (tries = 0; tries < 20; tries++) {
		if ((fd = open(path, O_RDONLY | O_CLOEXEC)) >= 0) {
			n = read(fd, buf, sizeof(buf) - 1);
			close(fd);
			if (n > 0) {
				buf[n] = '\0';
				v = strtol(buf, &end, 10);
				if (end != buf && (*end == '\n' || *end == '\0') && v > 0)
					return (pid_t)v;
			}
		}
		usleep(5000);
	}
	return -1;
}

struct pidfh *
pidfile_open(const char *path, mode_t mode, pid_t *pidptr)
{
	struct pidfh *pfh;
	struct stat st;
	int len, saved;

	if ((pfh = calloc(1, sizeof(*pfh))) == NULL)
		return NULL;
	if (path == NULL)
		len = snprintf(pfh->pf_path, sizeof(pfh->pf_path), "/var/run/%s.pid", getprogname());
	else
		len = snprintf(pfh->pf_path, sizeof(pfh->pf_path), "%s", path);
	if (len < 0 || (size_t)len >= sizeof(pfh->pf_path)) {
		free(pfh);
		errno = ENAMETOOLONG;
		return NULL;
	}
	pfh->pf_fd = open(pfh->pf_path, O_WRONLY | O_CREAT | O_CLOEXEC, mode != 0 ? mode : 0600);
	if (pfh->pf_fd < 0) {
		saved = errno;
		free(pfh);
		errno = saved;
		return NULL;
	}
	if (flock(pfh->pf_fd, LOCK_EX | LOCK_NB) != 0 || fstat(pfh->pf_fd, &st) != 0) {
		saved = errno;
		close(pfh->pf_fd);
		if (saved == EWOULDBLOCK) {
			if (pidptr != NULL)
				*pidptr = pidfile_holder(pfh->pf_path);
			saved = EEXIST;
		}
		free(pfh);
		errno = saved;
		return NULL;
	}
	pfh->pf_dev = st.st_dev;
	pfh->pf_ino = st.st_ino;
	return pfh;
}

int
pidfile_write(struct pidfh *pfh)
{
	char buf[16];
	int len;

	if (pfh == NULL)
		return 0;
	if (!pidfile_valid(pfh)) {
		errno = EINVAL;
		return -1;
	}
	len = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
	if (ftruncate(pfh->pf_fd, 0) != 0 || pwrite(pfh->pf_fd, buf, (size_t)len, 0) != len)
		return -1;
	return 0;
}

static int
pidfile_finish(struct pidfh *pfh, bool remove)
{
	int rc = 0, saved = 0;

	if (pfh == NULL)
		return 0;
	if (!pidfile_valid(pfh)) {
		errno = EINVAL;
		return -1;
	}
	if (remove && unlink(pfh->pf_path) != 0) {
		saved = errno;
		rc = -1;
	}
	if (close(pfh->pf_fd) != 0 && rc == 0) {
		saved = errno;
		rc = -1;
	}
	free(pfh);
	if (rc != 0)
		errno = saved;
	return rc;
}

int
pidfile_close(struct pidfh *pfh)
{
	return pidfile_finish(pfh, false);
}

int
pidfile_remove(struct pidfh *pfh)
{
	return pidfile_finish(pfh, true);
}

static void
trim_local_domain(char *name)
{
	char local[MAXHOSTNAMELEN + 1];
	const char *domain;
	size_t n, d;

	if (gethostname(local, sizeof(local)) != 0)
		return;
	local[sizeof(local) - 1] = '\0';
	if ((domain = strchr(local, '.')) == NULL || domain[1] == '\0')
		return;
	n = strlen(name);
	d = strlen(domain);
	if (n > d && strcasecmp(name + n - d, domain) == 0)
		name[n - d] = '\0';
}

static bool
same_address(const struct sockaddr *a, const struct sockaddr *b)
{
	if (a->sa_family != b->sa_family)
		return false;
	if (a->sa_family == AF_INET)
		return memcmp(&((const struct sockaddr_in *)(const void *)a)->sin_addr,
		    &((const struct sockaddr_in *)(const void *)b)->sin_addr, sizeof(struct in_addr)) == 0;
	if (a->sa_family == AF_INET6)
		return memcmp(&((const struct sockaddr_in6 *)(const void *)a)->sin6_addr,
		    &((const struct sockaddr_in6 *)(const void *)b)->sin6_addr, sizeof(struct in6_addr)) == 0;
	return false;
}

int
realhostname_sa(char *host, size_t hsize, struct sockaddr *addr, int addrlen)
{
	struct sockaddr_in mapped;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)(void *)addr;
	struct addrinfo hints, *res, *ai;
	char name[NI_MAXHOST];
	int result = HOSTNAME_INVALIDADDR;

	if (addr->sa_family == AF_INET6 && addrlen >= (int)sizeof(*sin6) &&
	    IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
		memset(&mapped, 0, sizeof(mapped));
		mapped.sin_len = sizeof(mapped);
		mapped.sin_family = AF_INET;
		memcpy(&mapped.sin_addr, &sin6->sin6_addr.s6_addr[12], sizeof(mapped.sin_addr));
		addr = (struct sockaddr *)&mapped;
		addrlen = sizeof(mapped);
	}
	if (getnameinfo(addr, (socklen_t)addrlen, name, sizeof(name), NULL, 0, NI_NAMEREQD) == 0) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = addr->sa_family;
		hints.ai_socktype = SOCK_STREAM;
		if (getaddrinfo(name, NULL, &hints, &res) != 0) {
			result = HOSTNAME_INVALIDNAME;
		} else {
			result = HOSTNAME_INCORRECTNAME;
			for (ai = res; ai != NULL; ai = ai->ai_next) {
				if (ai->ai_addr != NULL && same_address(ai->ai_addr, addr)) {
					result = HOSTNAME_FOUND;
					break;
				}
			}
			freeaddrinfo(res);
		}
		if (result == HOSTNAME_FOUND) {
			trim_local_domain(name);
			if (strlen(name) < hsize) {
				strlcpy(host, name, hsize);
				return result;
			}
		}
	}
	if (getnameinfo(addr, (socklen_t)addrlen, name, sizeof(name), NULL, 0, NI_NUMERICHOST) != 0)
		name[0] = '\0';
	strlcpy(host, name, hsize);
	return result;
}

int
realhostname(char *host, size_t hsize, const struct in_addr *ip)
{
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr = *ip;
	return realhostname_sa(host, hsize, (struct sockaddr *)&sin, sizeof(sin));
}

static bool
reexeced(void)
{
	if (getenv(REEXEC_GUARD) == NULL)
		return false;
	unsetenv(REEXEC_GUARD);
	return true;
}

static cpu_type_t
own_cpu_type(void)
{
#if defined(__x86_64__)
	return CPU_TYPE_X86_64;
#else
	return CPU_TYPE_ARM64;
#endif
}

static int
reexec_as(cpu_type_t type)
{
	char path[MAXPATHLEN];
	uint32_t size = sizeof(path);
	posix_spawnattr_t attr;
	size_t copied;
	int err;

	if (_NSGetExecutablePath(path, &size) != 0) {
		errno = ENAMETOOLONG;
		return -1;
	}
	if ((err = posix_spawnattr_init(&attr)) != 0) {
		errno = err;
		return -1;
	}
	err = posix_spawnattr_setbinpref_np(&attr, 1, &type, &copied);
	if (err == 0)
		err = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETEXEC);
	if (err == 0 && setenv(REEXEC_GUARD, "1", 1) != 0)
		err = errno;
	if (err == 0) {
		err = posix_spawn(NULL, path, NULL, &attr, *_NSGetArgv(), *_NSGetEnviron());
		unsetenv(REEXEC_GUARD);
	}
	posix_spawnattr_destroy(&attr);
	errno = err;
	return -1;
}

int
reexec_to_match_kernel(void)
{
	cpu_type_t kernel = 0;
	size_t n = sizeof(kernel);
	int is64 = 0;

	if (reexeced())
		return 0;
	if (sysctlbyname("hw.cputype", &kernel, &n, NULL, 0) != 0)
		return -1;
	n = sizeof(is64);
	if (sysctlbyname("hw.cpu64bit_capable", &is64, &n, NULL, 0) == 0 && is64)
		kernel |= CPU_ARCH_ABI64;
	if (kernel == own_cpu_type())
		return 0;
	return reexec_as(kernel);
}

int
reexec_to_match_lp64ness(bool isLP64)
{
	cpu_type_t own = own_cpu_type();

	if (reexeced())
		return 0;
	if (isLP64 == ((own & CPU_ARCH_ABI64) != 0))
		return 0;
	return reexec_as(isLP64 ? (own | CPU_ARCH_ABI64) : (own & ~CPU_ARCH_ABI64));
}

static bool
has_dotdot(const char *path)
{
	const char *s;

	for (s = path; (s = strstr(s, "..")) != NULL; s += 2)
		if ((s == path || s[-1] == '/') && (s[2] == '/' || s[2] == '\0'))
			return true;
	return false;
}

int
tzlink(const char *tz)
{
	static const char *const roots[] = { "/usr/share/zoneinfo/", "/var/db/timezone/zoneinfo/" };
	char tmp[MAXPATHLEN], magic[4];
	struct stat st;
	size_t i;
	bool tzif;
	int fd, err;

	if (tz == NULL || has_dotdot(tz))
		return EINVAL;
	for (i = 0; i < sizeof(roots) / sizeof(roots[0]); i++)
		if (strncmp(tz, roots[i], strlen(roots[i])) == 0)
			break;
	if (i == sizeof(roots) / sizeof(roots[0]))
		return EINVAL;
	if ((fd = open(tz, O_RDONLY | O_CLOEXEC)) < 0)
		return errno;
	tzif = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
	    read(fd, magic, sizeof(magic)) == (ssize_t)sizeof(magic) && memcmp(magic, "TZif", sizeof(magic)) == 0;
	close(fd);
	if (!tzif)
		return EINVAL;
	snprintf(tmp, sizeof(tmp), "/etc/localtime.%d", (int)getpid());
	unlink(tmp);
	if (symlink(tz, tmp) != 0)
		return errno;
	if (rename(tmp, "/etc/localtime") != 0) {
		err = errno;
		unlink(tmp);
		return err;
	}
	notify_post("com.apple.system.timezone");
	return 0;
}

struct wipefs_range {
	off_t start;
	off_t end;
};

struct __wipefs_ctx {
	int fd;
	bool device;
	off_t block_size;
	off_t size;
	struct wipefs_range *wipe;
	struct wipefs_range *keep;
	size_t nwipe;
	size_t nkeep;
};

typedef struct __wipefs_ctx *wipefs_ctx;

void
wipefs_free(wipefs_ctx *handle)
{
	if (handle == NULL || *handle == NULL)
		return;
	free((*handle)->wipe);
	free((*handle)->keep);
	free(*handle);
	*handle = NULL;
}

static int
wipefs_add(struct wipefs_range **list, size_t *n, off_t start, off_t end)
{
	struct wipefs_range *r;

	if (start >= end)
		return 0;
	if ((r = realloc(*list, (*n + 1) * sizeof(**list))) == NULL)
		return ENOMEM;
	r[*n].start = start;
	r[*n].end = end;
	*list = r;
	(*n)++;
	return 0;
}

static int
wipefs_add_blocks(wipefs_ctx ctx, bool keep, off_t block, off_t count)
{
	off_t start, end;

	if (ctx == NULL || block < 0 || count < 0 || block > ctx->size / ctx->block_size)
		return EINVAL;
	start = block * ctx->block_size;
	end = count > (ctx->size - start) / ctx->block_size ? ctx->size : start + count * ctx->block_size;
	return keep ? wipefs_add(&ctx->keep, &ctx->nkeep, start, end) :
	    wipefs_add(&ctx->wipe, &ctx->nwipe, start, end);
}

int
wipefs_alloc(int fd, size_t block_size, wipefs_ctx *handle)
{
	struct stat st;
	wipefs_ctx ctx;
	uint32_t device_block;
	uint64_t count;
	off_t size;
	int err;

	if (handle == NULL)
		return EINVAL;
	*handle = NULL;
	if (fstat(fd, &st) != 0)
		return errno;
	if (S_ISREG(st.st_mode)) {
		size = st.st_size;
	} else if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) {
		if (ioctl(fd, DKIOCGETBLOCKSIZE, &device_block) != 0 || ioctl(fd, DKIOCGETBLOCKCOUNT, &count) != 0)
			return errno;
		if (block_size == 0)
			block_size = device_block;
		size = (off_t)(count * device_block);
	} else {
		return EINVAL;
	}
	if ((ctx = calloc(1, sizeof(*ctx))) == NULL)
		return ENOMEM;
	ctx->fd = fd;
	ctx->device = !S_ISREG(st.st_mode);
	ctx->block_size = block_size != 0 ? (off_t)block_size : 512;
	ctx->size = size;
	err = wipefs_add(&ctx->wipe, &ctx->nwipe, 0, MIN(size, WIPEFS_SPAN));
	if (err == 0)
		err = wipefs_add(&ctx->wipe, &ctx->nwipe, MAX(0, size - WIPEFS_SPAN), size);
	if (err != 0) {
		wipefs_free(&ctx);
		return err;
	}
	*handle = ctx;
	return 0;
}

int
wipefs_include_blocks(wipefs_ctx handle, off_t block_offset, off_t nblocks)
{
	return wipefs_add_blocks(handle, false, block_offset, nblocks);
}

int
wipefs_except_blocks(wipefs_ctx handle, off_t block_offset, off_t nblocks)
{
	return wipefs_add_blocks(handle, true, block_offset, nblocks);
}

static off_t
wipefs_skip_kept(wipefs_ctx ctx, off_t pos)
{
	size_t i;
	bool moved;

	do {
		moved = false;
		for (i = 0; i < ctx->nkeep; i++) {
			if (ctx->keep[i].start <= pos && pos < ctx->keep[i].end) {
				pos = ctx->keep[i].end;
				moved = true;
			}
		}
	} while (moved);
	return pos;
}

static off_t
wipefs_next_kept(wipefs_ctx ctx, off_t pos, off_t stop)
{
	size_t i;

	for (i = 0; i < ctx->nkeep; i++)
		if (ctx->keep[i].start > pos && ctx->keep[i].start < stop)
			stop = ctx->keep[i].start;
	return stop;
}

static int
wipefs_zero(int fd, const char *zeros, size_t chunk, off_t start, off_t end)
{
	ssize_t n;

	while (start < end) {
		n = pwrite(fd, zeros, (size_t)MIN((off_t)chunk, end - start), start);
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0)
			return errno;
		if (n == 0)
			return EIO;
		start += n;
	}
	return 0;
}

int
wipefs_wipe(wipefs_ctx handle)
{
	const size_t chunk = 256 * 1024;
	char *zeros;
	off_t pos, next;
	size_t i;
	int err = 0;

	if (handle == NULL)
		return EINVAL;
	if ((zeros = calloc(1, chunk)) == NULL)
		return ENOMEM;
	for (i = 0; i < handle->nwipe && err == 0; i++) {
		for (pos = handle->wipe[i].start; err == 0; pos = next) {
			pos = wipefs_skip_kept(handle, pos);
			if (pos >= handle->wipe[i].end)
				break;
			next = wipefs_next_kept(handle, pos, handle->wipe[i].end);
			err = wipefs_zero(handle->fd, zeros, chunk, pos, next);
		}
	}
	free(zeros);
	if (err != 0)
		return err;
	if (handle->device)
		(void)ioctl(handle->fd, DKIOCSYNCHRONIZECACHE);
	else if (fsync(handle->fd) != 0)
		return errno;
	return 0;
}
