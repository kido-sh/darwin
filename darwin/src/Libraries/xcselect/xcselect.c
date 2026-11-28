/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "xcselect_private.h"

#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <unistd.h>

#define XCSELECT_VERSION_STRING "2418"

#define XCSELECT_MAX_SDKS 64
#define XCSELECT_MAX_FAT_ARCHES 64

#define PATH_LINK_PRIMARY   "/var/select/developer_dir"
#define PATH_LINK_LEGACY_DB "/var/db/xcode_select_link"
#define PATH_LINK_LEGACY_US "/usr/share/xcode-select/xcode_dir_link"
#define PATH_DATA_LEGACY    "/usr/share/xcode-select/xcode_dir_path"

#define PATH_DEFAULT_XCODE      "/Applications/Xcode.app/Contents/Developer"
#define PATH_DEFAULT_CLT        "/Library/Developer/CommandLineTools"
#define PATH_DEFAULT_XCODE_BETA "/Applications/Xcode-beta.app/Contents/Developer"

#define PATH_CLT_SDKS           "/Library/Developer/CommandLineTools/SDKs"
#define SUBPATH_LIBXCRUN        "usr/lib/libxcrun.dylib"
#define SUBPATH_BIN_XCRUN       "usr/bin/xcrun"
#define SUBPATH_MACOS_SDKS      "Platforms/MacOSX.platform/Developer/SDKs"

struct xcselect_manpaths {
	char **paths;
	uint32_t count;
};

struct xcselect_os_version {
	uint32_t major;
	uint32_t minor;
	bool has_minor;
	bool valid;
};

typedef int (*sandbox_check_fn)(void);
typedef void (*xcrun_iter_manpaths_fn)(const char *devdir,
                                       const char *sdkname,
                                       void (^callback)(const char *path));
typedef void (*xcrun_main_fn)(const char *tool_name, int argc, char **argv,
                              const char *devdir);
typedef void (*xcrun_set_unknown_handler_fn)(void (^handler)(const char *tool));

/* ------------------------------------------------------------------------- */
/* Small path, string, error, and environment helpers                        */
/* ------------------------------------------------------------------------- */

static const char *
format_errno(int err, char *buf, size_t buf_size)
{
	if (strerror_r(err, buf, buf_size) != 0) {
		snprintf(buf, buf_size, "errno %d", err);
	}
	return buf;
}

static bool
path_exists(const char *path)
{
	struct stat st;

	if (path == NULL || path[0] == '\0') {
		return false;
	}
	return stat(path, &st) == 0;
}

static bool
path_is_dir(const char *path)
{
	struct stat st;

	if (path == NULL || path[0] == '\0') {
		return false;
	}
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool
path_is_reg(const char *path)
{
	struct stat st;

	if (path == NULL || path[0] == '\0') {
		return false;
	}
	return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool
path_is_exec_file(const char *path)
{
	struct stat st;

	if (path == NULL || path[0] == '\0') {
		return false;
	}
	if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
		return false;
	}
	if ((st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0) {
		return false;
	}
	return access(path, X_OK) == 0;
}

static bool
str_ends_with(const char *str, const char *suffix)
{
	size_t slen;
	size_t suflen;

	if (str == NULL || suffix == NULL) {
		return false;
	}
	slen = strlen(str);
	suflen = strlen(suffix);
	if (slen < suflen) {
		return false;
	}
	return memcmp(str + (slen - suflen), suffix, suflen + 1) == 0;
}

static bool
path_append(char *dst, size_t dst_size, const char *component)
{
	size_t len;
	size_t total;

	if (dst == NULL || dst_size == 0 || component == NULL) {
		return false;
	}
	len = strlen(dst);
	if (len > 0 && dst[len - 1] != '/') {
		if (len + 1 >= dst_size) {
			return false;
		}
		dst[len] = '/';
		dst[len + 1] = '\0';
	}
	total = strlcat(dst, component, dst_size);
	return total < dst_size;
}

static bool
path_join(char *dst, size_t dst_size, const char *base, const char *component)
{
	if (dst == NULL || dst_size == 0 || base == NULL || component == NULL) {
		return false;
	}
	if (strlcpy(dst, base, dst_size) >= dst_size) {
		return false;
	}
	return path_append(dst, dst_size, component);
}

static void
strip_trailing_slashes(char *path)
{
	size_t len;

	if (path == NULL) {
		return;
	}
	len = strlen(path);
	while (len >= 2 && path[len - 1] == '/') {
		path[--len] = '\0';
	}
}

static bool
path_is_clt_basename(const char *path)
{
	static const char clt[] = "CommandLineTools";
	const size_t clt_len = sizeof(clt) - 1;
	size_t end;
	size_t start;

	if (path == NULL) {
		return false;
	}
	end = strlen(path);
	while (end > 1 && path[end - 1] == '/') {
		end--;
	}
	start = end;
	while (start > 0 && path[start - 1] != '/') {
		start--;
	}
	if (start == 0) {
		return false;
	}
	return (end - start) == clt_len && memcmp(path + start, clt, clt_len) == 0;
}

static bool
path_contains_xcrun(const char *dir)
{
	char candidate[PATH_MAX];

	if (dir == NULL || dir[0] == '\0') {
		return false;
	}
	if (path_join(candidate, sizeof(candidate), dir, SUBPATH_LIBXCRUN) &&
	    path_is_reg(candidate)) {
		return true;
	}
	if (path_join(candidate, sizeof(candidate), dir, SUBPATH_BIN_XCRUN) &&
	    path_is_exec_file(candidate)) {
		return true;
	}
	return false;
}

static sandbox_check_fn g_sandbox_check_fn = NULL;

static void
init_sandbox_check_fn(void)
{
	void *sym = dlsym(RTLD_DEFAULT, "_xpc_runtime_is_app_sandboxed");
	if (sym != NULL) {
		memcpy(&g_sandbox_check_fn, &sym, sizeof(g_sandbox_check_fn));
	}
}

static bool
is_app_sandboxed(void)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;

	/* Resolve dynamically so libxcselect links only public libSystem symbols. */
	pthread_once(&once, init_sandbox_check_fn);
	if (g_sandbox_check_fn != NULL) {
		return g_sandbox_check_fn() != 0;
	}
	return false;
}

static uint32_t
read_be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t
read_be64(const uint8_t *p)
{
	return ((uint64_t)read_be32(p) << 32) | (uint64_t)read_be32(p + 4);
}

static bool
pread_full(int fd, void *buf, size_t count, uint64_t offset)
{
	uint8_t *dst = (uint8_t *)buf;
	size_t done = 0;

	if (offset > (uint64_t)INT64_MAX ||
	    count > (uint64_t)INT64_MAX - offset) {
		return false;
	}
	while (done < count) {
		ssize_t n = pread(fd, dst + done, count - done,
		                  (off_t)(offset + done));
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			return false;
		}
		if (n == 0) {
			return false;
		}
		done += (size_t)n;
	}
	return true;
}

static bool
slice_is_xcrun_shim(int fd, uint64_t slice_offset, uint64_t slice_end)
{
	struct mach_header_64 hdr;
	uint64_t cursor;
	uint64_t cmds_end;

	if (slice_offset > slice_end ||
	    slice_end - slice_offset < sizeof(hdr)) {
		return false;
	}
	if (!pread_full(fd, &hdr, sizeof(hdr), slice_offset)) {
		return false;
	}
	if (hdr.magic != MH_MAGIC_64 || hdr.ncmds == 0) {
		return false;
	}

	cursor = slice_offset + sizeof(hdr);
	if (hdr.sizeofcmds > slice_end - cursor) {
		return false;
	}
	cmds_end = cursor + hdr.sizeofcmds;

	for (uint32_t i = 0; i < hdr.ncmds; i++) {
		struct load_command lc;

		if (cmds_end - cursor < sizeof(lc)) {
			return false;
		}
		if (!pread_full(fd, &lc, sizeof(lc), cursor)) {
			return false;
		}
		if (lc.cmdsize < sizeof(lc) || lc.cmdsize > cmds_end - cursor) {
			return false;
		}

		if (lc.cmd == LC_SEGMENT_64 &&
		    lc.cmdsize >= sizeof(struct segment_command_64)) {
			struct segment_command_64 seg;

			if (!pread_full(fd, &seg, sizeof(seg), cursor)) {
				return false;
			}
			if (strncmp(seg.segname, "__DATA", sizeof(seg.segname)) == 0) {
				uint64_t avail = lc.cmdsize - sizeof(seg);
				uint64_t max_sects = avail / sizeof(struct section_64);
				uint32_t nsects = seg.nsects;
				uint64_t sect_off = cursor + sizeof(seg);

				if ((uint64_t)nsects > max_sects) {
					return false;
				}
				for (uint32_t s = 0; s < nsects; s++) {
					struct section_64 sect;

					if (!pread_full(fd, &sect, sizeof(sect),
					                sect_off +
					                (uint64_t)s * sizeof(sect))) {
						return false;
					}
					if (strncmp(sect.sectname, "__xcrun_shim",
					            sizeof(sect.sectname)) == 0) {
						return true;
					}
				}
			}
		}

		cursor += lc.cmdsize;
	}

	return false;
}

static bool
is_path_xcrun_shim(const char *path)
{
	int fd;
	struct stat st;
	uint8_t magic_bytes[4];
	uint32_t magic_be;
	uint64_t file_size;
	bool result = false;

	if (path == NULL || path[0] == '\0') {
		return false;
	}
	fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		return false;
	}
	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 4) {
		close(fd);
		return false;
	}
	file_size = (uint64_t)st.st_size;

	if (!pread_full(fd, magic_bytes, sizeof(magic_bytes), 0)) {
		close(fd);
		return false;
	}
	magic_be = read_be32(magic_bytes);

	if (magic_be == FAT_MAGIC) {
		uint8_t count_bytes[4];
		uint32_t nfat;

		if (pread_full(fd, count_bytes, sizeof(count_bytes), 4)) {
			nfat = read_be32(count_bytes);
			if (nfat > XCSELECT_MAX_FAT_ARCHES) {
				nfat = XCSELECT_MAX_FAT_ARCHES;
			}
			for (uint32_t i = 0; i < nfat; i++) {
				uint8_t arch_buf[20];
				uint64_t entry_off = 8ULL + (uint64_t)i * sizeof(arch_buf);
				uint64_t slice_off;
				uint64_t slice_size;

				if (entry_off + sizeof(arch_buf) > file_size) {
					break;
				}
				if (!pread_full(fd, arch_buf, sizeof(arch_buf), entry_off)) {
					break;
				}
				slice_off = read_be32(arch_buf + 8);
				slice_size = read_be32(arch_buf + 12);
				if (slice_off > file_size ||
				    slice_size > file_size - slice_off) {
					continue;
				}
				if (slice_is_xcrun_shim(fd, slice_off,
				                        slice_off + slice_size)) {
					result = true;
					break;
				}
			}
		}
	} else if (magic_be == FAT_MAGIC_64) {
		uint8_t count_bytes[4];
		uint32_t nfat;

		if (pread_full(fd, count_bytes, sizeof(count_bytes), 4)) {
			nfat = read_be32(count_bytes);
			if (nfat > XCSELECT_MAX_FAT_ARCHES) {
				nfat = XCSELECT_MAX_FAT_ARCHES;
			}
			for (uint32_t i = 0; i < nfat; i++) {
				uint8_t arch_buf[32];
				uint64_t entry_off = 8ULL + (uint64_t)i * sizeof(arch_buf);
				uint64_t slice_off;
				uint64_t slice_size;

				if (entry_off + sizeof(arch_buf) > file_size) {
					break;
				}
				if (!pread_full(fd, arch_buf, sizeof(arch_buf), entry_off)) {
					break;
				}
				slice_off = read_be64(arch_buf + 8);
				slice_size = read_be64(arch_buf + 16);
				if (slice_off > file_size ||
				    slice_size > file_size - slice_off) {
					continue;
				}
				if (slice_is_xcrun_shim(fd, slice_off,
				                        slice_off + slice_size)) {
					result = true;
					break;
				}
			}
		}
	} else {
		result = slice_is_xcrun_shim(fd, 0, file_size);
	}

	close(fd);
	return result;
}

/* ------------------------------------------------------------------------- */
/* Symlink / legacy file readers for developer directory selection           */
/* ------------------------------------------------------------------------- */

/*
 * Resolve a developer directory symlink into a full-size PATH_MAX buffer.
 * Returns true when lookup produced a terminal result (either a valid
 * directory in resolved with *valid = true, or a non-ENOENT error with
 * *valid = false). Returns false when the symlink does not exist, points to a
 * non-directory, or exceeds PATH_MAX so callers continue checking fallbacks.
 */
static bool
read_developer_dir_symlink(const char *link_path, char resolved[PATH_MAX],
                           bool *valid)
{
	char tmp[PATH_MAX + 1];
	char errbuf[128];
	ssize_t n;

	n = readlink(link_path, tmp, sizeof(tmp) - 1);
	if (n >= 1) {
		if ((size_t)n >= PATH_MAX) {
			return false;
		}
		tmp[n] = '\0';
		if (path_is_dir(tmp)) {
			strlcpy(resolved, tmp, PATH_MAX);
			*valid = true;
			return true;
		}
		return false;
	}

	if (n < 0 && errno == ENOENT) {
		return false;
	}

	*valid = false;
	if (n < 0 && errno == EACCES) {
		fprintf(stderr,
		        "xcselect: permission denied reading developer link '%s'\n",
		        link_path);
	} else {
		int err = (n < 0) ? errno : EINVAL;
		fprintf(stderr,
		        "xcselect: failed to read developer symlink '%s' (%s)\n",
		        link_path, format_errno(err, errbuf, sizeof(errbuf)));
	}
	return true;
}

static bool
read_developer_dir_data_file(const char *file_path, char resolved[PATH_MAX],
                             bool *valid)
{
	char tmp[PATH_MAX + 1];
	char errbuf[128];
	struct stat st;
	int fd;
	size_t total = 0;

	fd = open(file_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		if (errno == ENOENT) {
			return false;
		}
		*valid = false;
		if (errno == EACCES) {
			fprintf(stderr,
			        "xcselect: permission denied opening developer path file '%s'\n",
			        file_path);
		} else {
			fprintf(stderr,
			        "xcselect: failed to open developer path file '%s' (%s)\n",
			        file_path,
			        format_errno(errno, errbuf, sizeof(errbuf)));
		}
		return true;
	}

	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
		close(fd);
		return false;
	}

	while (total < sizeof(tmp) - 1) {
		ssize_t n = read(fd, tmp + total, (sizeof(tmp) - 1) - total);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			int saved_errno = errno;
			close(fd);
			*valid = false;
			fprintf(stderr,
			        "xcselect: failed to read developer path file '%s' (%s)\n",
			        file_path,
			        format_errno(saved_errno, errbuf, sizeof(errbuf)));
			return true;
		}
		if (n == 0) {
			break;
		}
		total += (size_t)n;
	}
	close(fd);

	if (total == 0 || total >= PATH_MAX) {
		return false;
	}
	if (memchr(tmp, '\0', total) != NULL) {
		return false;
	}
	while (total > 0 && (tmp[total - 1] == '\n' || tmp[total - 1] == '\r')) {
		total--;
	}
	if (total == 0) {
		return false;
	}
	tmp[total] = '\0';
	if (path_is_dir(tmp)) {
		strlcpy(resolved, tmp, PATH_MAX);
		*valid = true;
		return true;
	}
	return false;
}

/* ------------------------------------------------------------------------- */
/* Host OS and SDK version parsing                                           */
/* ------------------------------------------------------------------------- */

static bool
parse_uint_component(const char *start, const char *end, uint32_t *out)
{
	uint64_t val = 0;

	if (start == NULL || end == NULL || start >= end) {
		return false;
	}
	for (const char *p = start; p < end; p++) {
		if (!isdigit((unsigned char)*p)) {
			return false;
		}
		val = val * 10ULL + (uint64_t)(*p - '0');
		if (val > UINT32_MAX) {
			return false;
		}
	}
	*out = (uint32_t)val;
	return true;
}

static struct xcselect_os_version
parse_os_version_string(const char *str)
{
	struct xcselect_os_version v = { 0, 0, false, false };
	const char *dot1;
	const char *dot2;

	if (str == NULL || str[0] == '\0') {
		return v;
	}

	dot1 = strchr(str, '.');
	if (dot1 == NULL) {
		if (parse_uint_component(str, str + strlen(str), &v.major)) {
			v.valid = true;
		}
		return v;
	}

	if (!parse_uint_component(str, dot1, &v.major)) {
		return v;
	}
	v.valid = true;

	dot2 = strchr(dot1 + 1, '.');
	if (dot2 == NULL) {
		dot2 = dot1 + 1 + strlen(dot1 + 1);
	}
	if (parse_uint_component(dot1 + 1, dot2, &v.minor)) {
		v.has_minor = true;
	}
	return v;
}

static struct xcselect_os_version g_host_os_version = { 0, 0, false, false };

static void
init_host_os_version(void)
{
	char ver[64];
	size_t len = sizeof(ver) - 1;

	memset(ver, 0, sizeof(ver));
	if (sysctlbyname("kern.osproductversion", ver, &len, NULL, 0) == 0) {
		ver[sizeof(ver) - 1] = '\0';
		g_host_os_version = parse_os_version_string(ver);
	}
}

static struct xcselect_os_version
get_host_os_version(void)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;

	pthread_once(&once, init_host_os_version);
	return g_host_os_version;
}

static struct xcselect_os_version
parse_sdk_basename_version(const char *sdk_path)
{
	struct xcselect_os_version v = { 0, 0, false, false };
	const char *slash;
	const char *base;
	size_t base_len;
	const char *ver_start;
	const char *ver_end;
	const char *dot;

	if (sdk_path == NULL) {
		return v;
	}
	slash = strrchr(sdk_path, '/');
	base = (slash != NULL) ? (slash + 1) : sdk_path;
	base_len = strlen(base);

	/* Expect "MacOSX" prefix and ".sdk" suffix. */
	if (base_len < 10 || strncmp(base, "MacOSX", 6) != 0 ||
	    strcmp(base + base_len - 4, ".sdk") != 0) {
		return v;
	}

	ver_start = base + 6;
	ver_end = base + base_len - 4;
	if (ver_start == ver_end) {
		/* Unversioned "MacOSX.sdk" */
		return v;
	}

	dot = memchr(ver_start, '.', (size_t)(ver_end - ver_start));
	if (dot == NULL) {
		if (parse_uint_component(ver_start, ver_end, &v.major)) {
			v.valid = true;
		}
		return v;
	}

	/* Require exactly major.minor (no second dot). */
	if (memchr(dot + 1, '.', (size_t)(ver_end - (dot + 1))) != NULL) {
		return v;
	}
	if (!parse_uint_component(ver_start, dot, &v.major) ||
	    !parse_uint_component(dot + 1, ver_end, &v.minor)) {
		return v;
	}
	v.has_minor = true;
	v.valid = true;
	return v;
}

static int
compare_sdk_versions(const struct xcselect_os_version *a,
                     const struct xcselect_os_version *b)
{
	if (a->valid != b->valid) {
		return a->valid ? 1 : -1;
	}
	if (!a->valid) {
		return 0;
	}
	if (a->major != b->major) {
		return (a->major > b->major) ? 1 : -1;
	}
	if (a->minor != b->minor) {
		return (a->minor > b->minor) ? 1 : -1;
	}
	if (a->has_minor != b->has_minor) {
		return a->has_minor ? 1 : -1;
	}
	return 0;
}

static bool
sdk_matches_host(const struct xcselect_os_version *sdk,
                 const struct xcselect_os_version *host)
{
	if (!sdk->valid || !host->valid) {
		return false;
	}
	if (host->major == 10) {
		return sdk->major == 10 && sdk->has_minor &&
		       host->has_minor && sdk->minor == host->minor;
	}
	return sdk->major == host->major;
}

static size_t
collect_sdks_at_path(const char *dir_path, char **out_list, size_t max_count)
{
	DIR *dir;
	struct dirent *ent;
	size_t found = 0;

	if (dir_path == NULL || out_list == NULL || max_count == 0) {
		return 0;
	}
	dir = opendir(dir_path);
	if (dir == NULL) {
		return 0;
	}

	while (found < max_count && (ent = readdir(dir)) != NULL) {
		char *probe = NULL;
		int len;
		struct stat st;

		if (strncmp(ent->d_name, "MacOSX", 6) != 0 ||
		    !str_ends_with(ent->d_name, ".sdk")) {
			continue;
		}
		len = asprintf(&probe, "%s/%s/usr/include/sys/cdefs.h",
		               dir_path, ent->d_name);
		if (len < 0 || probe == NULL) {
			continue;
		}
		if (stat(probe, &st) == 0) {
			/* Strip "/usr/include/sys/cdefs.h" (24 bytes) in place. */
			const size_t suffix_len = 24;
			if ((size_t)len > suffix_len) {
				probe[(size_t)len - suffix_len] = '\0';
				out_list[found++] = probe;
			} else {
				free(probe);
			}
		} else {
			free(probe);
		}
	}

	closedir(dir);
	return found;
}

/* ------------------------------------------------------------------------- */
/* Manpaths internal helper                                                  */
/* ------------------------------------------------------------------------- */

static bool
manpaths_append(xcselect_manpaths *mp, const char *path)
{
	char **grown;
	char *copy;
	size_t new_count;

	if (mp == NULL || path == NULL) {
		return false;
	}
	if (mp->count == UINT32_MAX) {
		return false;
	}
	new_count = (size_t)mp->count + 1;
	if (new_count > SIZE_MAX / sizeof(char *)) {
		return false;
	}
	copy = strdup(path);
	if (copy == NULL) {
		return false;
	}
	grown = realloc(mp->paths, new_count * sizeof(char *));
	if (grown == NULL) {
		free(copy);
		return false;
	}
	mp->paths = grown;
	mp->paths[mp->count] = copy;
	mp->count++;
	return true;
}

/* ------------------------------------------------------------------------- */
/* Exported API (12 symbols)                                                 */
/* ------------------------------------------------------------------------- */

const char *
xcselect_get_version(void)
{
	return XCSELECT_VERSION_STRING;
}

bool
xcselect_find_developer_contents_from_path(const char *path, char *buffer,
                                           int buffer_size,
                                           bool *is_command_line_tools)
{
	char work[PATH_MAX];
	char probe[PATH_MAX];

	if (path == NULL || buffer == NULL || buffer_size <= 0) {
		return false;
	}

	if (path[0] == '/') {
		if (strlcpy(work, path, sizeof(work)) >= sizeof(work)) {
			return false;
		}
	} else {
		if (getcwd(work, sizeof(work)) == NULL) {
			return false;
		}
		if (path[0] != '\0' && !path_append(work, sizeof(work), path)) {
			return false;
		}
	}

	strip_trailing_slashes(work);

	if (strcmp(work, "/") == 0 ||
	    (path_join(probe, sizeof(probe), work, "mach_kernel") &&
	     path_exists(probe))) {
		if (!path_join(buffer, (size_t)buffer_size, work,
		               "Library/Developer/CommandLineTools")) {
			return false;
		}
		if (path_contains_xcrun(buffer)) {
			if (is_command_line_tools != NULL) {
				*is_command_line_tools = true;
			}
			return true;
		}
		return false;
	}

	if (path_is_clt_basename(work)) {
		if (strlcpy(buffer, work, (size_t)buffer_size) >= (size_t)buffer_size) {
			return false;
		}
		if (path_contains_xcrun(buffer)) {
			if (is_command_line_tools != NULL) {
				*is_command_line_tools = true;
			}
			return true;
		}
		return false;
	}

	if (path_join(buffer, (size_t)buffer_size, work, "Contents/Developer") &&
	    path_is_dir(buffer)) {
		if (is_command_line_tools != NULL) {
			*is_command_line_tools = false;
		}
		return true;
	}

	if (path_contains_xcrun(work)) {
		if (strlcpy(buffer, work, (size_t)buffer_size) >= (size_t)buffer_size) {
			return false;
		}
		if (is_command_line_tools != NULL) {
			*is_command_line_tools = false;
		}
		return true;
	}

	if (path_join(buffer, (size_t)buffer_size, work,
	              "Xcode.app/Contents/Developer") &&
	    path_is_dir(buffer)) {
		if (is_command_line_tools != NULL) {
			*is_command_line_tools = false;
		}
		return true;
	}

	return false;
}

bool
xcselect_get_developer_dir_path(char *buffer, int buffer_size,
                                bool *was_environment,
                                bool *is_command_line_tools,
                                bool *was_default)
{
	char resolved[PATH_MAX];
	const char *env;
	bool valid = false;
	bool clt = false;

	if (is_command_line_tools != NULL) {
		*is_command_line_tools = false;
	}
	if (was_default != NULL) {
		*was_default = false;
	}
	if (was_environment != NULL) {
		*was_environment = false;
	}
	if (buffer == NULL || buffer_size <= 0) {
		return false;
	}

	env = getenv("DEVELOPER_DIR");
	if (env != NULL && env[0] != '\0') {
		if (xcselect_find_developer_contents_from_path(env, resolved,
		                                               (int)sizeof(resolved),
		                                               &clt)) {
			if (is_command_line_tools != NULL) {
				*is_command_line_tools = clt;
			}
			if (strcmp(resolved, env) != 0) {
				setenv("DEVELOPER_DIR", resolved, 1);
			}
			strlcpy(buffer, resolved, (size_t)buffer_size);
		} else {
			strlcpy(buffer, env, (size_t)buffer_size);
		}
		if (was_environment != NULL) {
			*was_environment = true;
		}
		return true;
	}

	if (read_developer_dir_symlink(PATH_LINK_PRIMARY, resolved, &valid) ||
	    read_developer_dir_symlink(PATH_LINK_LEGACY_DB, resolved, &valid) ||
	    read_developer_dir_symlink(PATH_LINK_LEGACY_US, resolved, &valid) ||
	    read_developer_dir_data_file(PATH_DATA_LEGACY, resolved, &valid)) {
		if (!valid) {
			return false;
		}
		if (is_command_line_tools != NULL) {
			*is_command_line_tools = path_is_clt_basename(resolved);
		}
		strlcpy(buffer, resolved, (size_t)buffer_size);
		return true;
	}

	if (path_contains_xcrun(PATH_DEFAULT_XCODE)) {
		strlcpy(buffer, PATH_DEFAULT_XCODE, (size_t)buffer_size);
		if (was_default != NULL) {
			*was_default = true;
		}
		return true;
	}
	if (path_contains_xcrun(PATH_DEFAULT_CLT)) {
		strlcpy(buffer, PATH_DEFAULT_CLT, (size_t)buffer_size);
		if (is_command_line_tools != NULL) {
			*is_command_line_tools = true;
		}
		if (was_default != NULL) {
			*was_default = true;
		}
		return true;
	}
	if (path_contains_xcrun(PATH_DEFAULT_XCODE_BETA)) {
		strlcpy(buffer, PATH_DEFAULT_XCODE_BETA, (size_t)buffer_size);
		if (was_default != NULL) {
			*was_default = true;
		}
		return true;
	}

	return false;
}

bool
xcselect_developer_dir_matches_path(const char *developer_dir, const char *path)
{
	char dev[PATH_MAX];
	size_t dev_len;

	if (developer_dir != NULL) {
		if (strlcpy(dev, developer_dir, sizeof(dev)) >= sizeof(dev)) {
			return false;
		}
	} else {
		bool we = false;
		bool clt = false;
		bool def = false;

		if (!xcselect_get_developer_dir_path(dev, (int)sizeof(dev),
		                                     &we, &clt, &def)) {
			return false;
		}
	}

	dev_len = strlen(dev);
	if (dev_len == 0) {
		return false;
	}

	strip_trailing_slashes(dev);
	if (str_ends_with(dev, ".app/Contents/Developer")) {
		const size_t strip_len = sizeof("/Contents/Developer") - 1;
		dev_len = strlen(dev);
		if (dev_len >= strip_len) {
			dev[dev_len - strip_len] = '\0';
		}
	}

	dev_len = strlen(dev);
	if (path == NULL || strlen(path) < dev_len) {
		return false;
	}

	return strncmp(dev, path, dev_len) == 0;
}

bool
xcselect_bundle_is_developer_tool(const char *bundle_id)
{
	static const char prefix[] = "com.apple.";
	static const char *const suffixes[] = {
		"dt.Xcode",
		"dt.SourceEdit",
		"iphonesimulator",
		"dt.Instruments",
		"FileMerge",
		"itunes.connect.ApplicationLoader",
		"AccessibilityInspector",
		"RealityComposer",
		"RealityComposerPro",
	};
	const size_t prefix_len = sizeof(prefix) - 1;
	const char *rest;

	if (bundle_id == NULL) {
		return false;
	}
	if (strncmp(bundle_id, prefix, prefix_len) != 0) {
		return false;
	}
	rest = bundle_id + prefix_len;
	for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
		if (strcmp(rest, suffixes[i]) == 0) {
			return true;
		}
	}
	return false;
}

errno_t
xcselect_host_sdk_path(xcselect_host_sdk_policy_t sdk_policy, char **path)
{
	char **sdks;
	size_t count = 0;
	char devdir[PATH_MAX];
	bool was_env = false;
	bool is_clt = false;
	bool was_default = false;
	bool have_devdir;
	struct xcselect_os_version host_ver;
	char *best_latest = NULL;
	struct xcselect_os_version best_latest_ver = { 0, 0, false, false };
	char *best_match = NULL;
	struct xcselect_os_version best_match_ver = { 0, 0, false, false };
	bool best_match_exact = false;
	char *chosen = NULL;

	if (path == NULL) {
		return EINVAL;
	}
	if (sdk_policy != XCSELECT_HOST_SDK_POLICY_MATCHING_PREFERRED &&
	    sdk_policy != XCSELECT_HOST_SDK_POLICY_MATCHING_ONLY &&
	    sdk_policy != XCSELECT_HOST_SDK_POLICY_LATEST) {
		return EINVAL;
	}

	sdks = calloc(XCSELECT_MAX_SDKS, sizeof(char *));
	if (sdks == NULL) {
		return ENOMEM;
	}

	host_ver = get_host_os_version();
	have_devdir = xcselect_get_developer_dir_path(devdir, (int)sizeof(devdir),
	                                              &was_env, &is_clt,
	                                              &was_default);

	if (have_devdir && !is_clt && !was_default) {
		char *platform_sdks = NULL;
		if (asprintf(&platform_sdks, "%s/%s", devdir, SUBPATH_MACOS_SDKS) >= 0 &&
		    platform_sdks != NULL) {
			count += collect_sdks_at_path(platform_sdks, sdks + count,
			                              XCSELECT_MAX_SDKS - count);
			free(platform_sdks);
		}
	} else if (have_devdir && is_clt && strcmp(devdir, PATH_DEFAULT_CLT) != 0) {
		char *reloc_clt_sdks = NULL;
		if (asprintf(&reloc_clt_sdks, "%s/SDKs", devdir) >= 0 &&
		    reloc_clt_sdks != NULL) {
			count += collect_sdks_at_path(reloc_clt_sdks, sdks + count,
			                              XCSELECT_MAX_SDKS - count);
			free(reloc_clt_sdks);
		}
	}

	if (count < XCSELECT_MAX_SDKS) {
		count += collect_sdks_at_path(PATH_CLT_SDKS, sdks + count,
		                              XCSELECT_MAX_SDKS - count);
	}

	/*
	 * Container fallback: when no SDK was found in the primary locations,
	 * also inspect <devdir>/SDKs and default Xcode platform SDKs.
	 */
	if (count == 0 && have_devdir) {
		char *alt_sdks = NULL;

		if (!is_clt && asprintf(&alt_sdks, "%s/SDKs", devdir) >= 0 &&
		    alt_sdks != NULL) {
			count += collect_sdks_at_path(alt_sdks, sdks + count,
			                              XCSELECT_MAX_SDKS - count);
			free(alt_sdks);
			alt_sdks = NULL;
		}
		if (count < XCSELECT_MAX_SDKS && was_default) {
			if (asprintf(&alt_sdks, "%s/%s", devdir,
			             SUBPATH_MACOS_SDKS) >= 0 && alt_sdks != NULL) {
				count += collect_sdks_at_path(alt_sdks, sdks + count,
				                              XCSELECT_MAX_SDKS - count);
				free(alt_sdks);
			}
		}
	}

	for (size_t i = 0; i < count; i++) {
		char *candidate = sdks[i];
		struct xcselect_os_version ver = parse_sdk_basename_version(candidate);

		if (best_latest == NULL ||
		    compare_sdk_versions(&ver, &best_latest_ver) > 0) {
			best_latest = candidate;
			best_latest_ver = ver;
		}

		if (sdk_matches_host(&ver, &host_ver)) {
			bool is_exact = host_ver.has_minor && ver.has_minor &&
			                ver.minor == host_ver.minor;
			bool take = false;

			if (best_match == NULL) {
				take = true;
			} else if (is_exact && !best_match_exact) {
				take = true;
			} else if (is_exact == best_match_exact &&
			           compare_sdk_versions(&ver, &best_match_ver) > 0) {
				take = true;
			}

			if (take) {
				best_match = candidate;
				best_match_ver = ver;
				best_match_exact = is_exact;
			}
		}
	}

	switch (sdk_policy) {
	case XCSELECT_HOST_SDK_POLICY_MATCHING_PREFERRED:
		chosen = (best_match != NULL) ? best_match : best_latest;
		break;
	case XCSELECT_HOST_SDK_POLICY_MATCHING_ONLY:
		chosen = best_match;
		break;
	case XCSELECT_HOST_SDK_POLICY_LATEST:
		chosen = best_latest;
		break;
	default:
		chosen = NULL;
		break;
	}

	for (size_t i = 0; i < count; i++) {
		if (sdks[i] != chosen) {
			free(sdks[i]);
		}
	}
	free(sdks);

	*path = chosen;
	return (chosen != NULL) ? 0 : ENOENT;
}

xcselect_manpaths *
xcselect_get_manpaths(const char *sdkname)
{
	char devdir[PATH_MAX];
	char libxcrun_path[PATH_MAX];
	char manpath[PATH_MAX];
	bool was_env = false;
	bool is_clt = false;
	bool was_default = false;
	xcselect_manpaths *mp;

	if (is_app_sandboxed()) {
		return NULL;
	}
	if (!xcselect_get_developer_dir_path(devdir, (int)sizeof(devdir),
	                                     &was_env, &is_clt, &was_default)) {
		return NULL;
	}

	if (path_join(libxcrun_path, sizeof(libxcrun_path), devdir,
	              SUBPATH_LIBXCRUN) && path_is_reg(libxcrun_path)) {
		void *handle = dlopen(libxcrun_path, RTLD_LAZY | RTLD_LOCAL);
		if (handle == NULL) {
			const char *prog = getprogname();
			const char *err = dlerror();
			fprintf(stderr, "%s: error: failed to load libxcrun (%s)\n",
			        prog != NULL ? prog : "xcselect",
			        err != NULL ? err : "unknown error");
			return NULL;
		}

		void *sym = dlsym(handle, "xcrun_iter_manpaths");
		if (sym != NULL) {
			xcrun_iter_manpaths_fn iter_fn = NULL;

			memcpy(&iter_fn, &sym, sizeof(iter_fn));
			mp = calloc(1, sizeof(*mp));
			if (mp == NULL) {
				dlclose(handle);
				return NULL;
			}
			iter_fn(devdir, sdkname, ^(const char *p) {
				if (p != NULL) {
					manpaths_append(mp, p);
				}
			});
			dlclose(handle);
			return mp;
		}
		dlclose(handle);
	}

	mp = calloc(1, sizeof(*mp));
	if (mp == NULL) {
		return NULL;
	}
	if (path_join(manpath, sizeof(manpath), devdir, "usr/share/man")) {
		manpaths_append(mp, manpath);
	}
	if (path_join(manpath, sizeof(manpath), devdir,
	              "usr/llvm-gcc-4.2/share/man")) {
		manpaths_append(mp, manpath);
	}
	if (path_join(manpath, sizeof(manpath), devdir,
	              "Toolchains/XcodeDefault.xctoolchain/usr/share/man")) {
		manpaths_append(mp, manpath);
	}
	return mp;
}

uint32_t
xcselect_manpaths_get_num_paths(xcselect_manpaths *manpaths)
{
	if (manpaths == NULL) {
		return 0;
	}
	return manpaths->count;
}

const char *
xcselect_manpaths_get_path(xcselect_manpaths *manpaths, uint32_t idx)
{
	if (manpaths == NULL || idx >= manpaths->count) {
		return NULL;
	}
	return manpaths->paths[idx];
}

void
xcselect_manpaths_free(xcselect_manpaths *manpaths)
{
	if (manpaths == NULL) {
		return;
	}
	for (uint32_t i = 0; i < manpaths->count; i++) {
		free(manpaths->paths[i]);
	}
	free(manpaths->paths);
	free(manpaths);
}

static void
invoke_xcrun_via_library(const char *lib_path, const char *tool_name,
                         int argc, char *argv[], const char *devdir)
{
	void *handle;
	void *main_sym;
	xcrun_main_fn main_fn = NULL;
	const char *prog;

	handle = dlopen(lib_path, RTLD_LAZY | RTLD_LOCAL);
	if (handle == NULL) {
		const char *err = dlerror();
		fprintf(stderr, "xcrun: error: failed to load libxcrun (%s)\n",
		        err != NULL ? err : "unknown error");
		exit(1);
	}

	main_sym = dlsym(handle, "xcrun_main");
	if (main_sym == NULL) {
		const char *err = dlerror();
		fprintf(stderr, "xcrun: error: failed to locate xcrun_main (%s)\n",
		        err != NULL ? err : "unknown error");
		dlclose(handle);
		exit(1);
	}
	memcpy(&main_fn, &main_sym, sizeof(main_fn));

	prog = getprogname();
	if (prog == NULL || strcmp(prog, "xcrun") != 0) {
		void *handler_sym = dlsym(handle, "xcrun_set_unknown_utility_handler");
		if (handler_sym != NULL) {
			xcrun_set_unknown_handler_fn set_handler = NULL;
			memcpy(&set_handler, &handler_sym, sizeof(set_handler));
			set_handler(^(const char *missing_tool) {
				xcselect_trigger_install_request(missing_tool);
			});
		}
	}

	main_fn(tool_name, argc, argv, devdir);
	fprintf(stderr, "xcrun: error: xcrun_main returned unexpectedly\n");
	exit(1);
}

static void
invoke_xcrun_via_binary(const char *xcrun_bin, char *const exec_argv[],
                        const char *devdir, bool was_env)
{
	char errbuf[128];
	int saved_errno;

	execv(xcrun_bin, exec_argv);
	saved_errno = errno;

	if (saved_errno == ENOENT) {
		if (path_is_dir(devdir)) {
			fprintf(stderr,
			        "xcrun: error: invalid %s path '%s': missing xcrun at '%s'\n",
			        was_env ? "DEVELOPER_DIR" : "active developer",
			        devdir, xcrun_bin);
		} else if (was_env) {
			fprintf(stderr,
			        "xcrun: error: DEVELOPER_DIR path does not exist: '%s'\n",
			        devdir);
		} else {
			fprintf(stderr,
			        "xcrun: error: active developer directory '%s' does not exist\n",
			        devdir);
		}
	} else {
		fprintf(stderr, "xcrun: error: unable to execute '%s' (%s)\n",
		        xcrun_bin, format_errno(saved_errno, errbuf, sizeof(errbuf)));
	}
	exit(1);
}

void
xcselect_invoke_xcrun(const char *tool_name, int argc, char *argv[],
                      bool requires_xcode)
{
	static const char *const fallback_dirs[] = {
		"/usr/libexec/DeveloperTools",
		"/usr/local/bin",
	};
	char devdir[PATH_MAX];
	char libxcrun_path[PATH_MAX];
	char xcrun_bin[PATH_MAX];
	char *empty_argv[1] = { NULL };
	bool was_env = false;
	bool is_clt = false;
	bool was_default = false;
	int safe_argc;
	char **safe_argv;

	if (is_app_sandboxed()) {
		fprintf(stderr, "xcrun: error: cannot run inside an App Sandbox\n");
		exit(1);
	}

	safe_argc = (argc > 0 && argv != NULL) ? argc : 0;
	safe_argv = (safe_argc > 0) ? argv : empty_argv;

	if (xcselect_get_developer_dir_path(devdir, (int)sizeof(devdir),
	                                    &was_env, &is_clt, &was_default)) {
		size_t slots;
		char **exec_argv;
		size_t pos = 0;

		if (requires_xcode && is_clt) {
			fprintf(stderr,
			        "xcselect: error: tool '%s' requires full Xcode, "
			        "but active developer directory '%s' is Command Line Tools\n",
			        tool_name != NULL ? tool_name : "xcrun", devdir);
			exit(1);
		}

		if (tool_name != NULL) {
			unsetenv("xcrun_log");
		}

		if (path_join(libxcrun_path, sizeof(libxcrun_path), devdir,
		              SUBPATH_LIBXCRUN) && path_is_reg(libxcrun_path)) {
			invoke_xcrun_via_library(libxcrun_path, tool_name, safe_argc,
			                         safe_argv, devdir);
		}

		if (!path_join(xcrun_bin, sizeof(xcrun_bin), devdir, SUBPATH_BIN_XCRUN)) {
			fprintf(stderr, "xcselect: error: developer path too long: '%s'\n",
			        devdir);
			exit(1);
		}
		if (is_path_xcrun_shim(xcrun_bin)) {
			fprintf(stderr,
			        "xcselect: error: developer path '%s' points to an xcrun shim\n",
			        devdir);
			exit(1);
		}

		slots = (size_t)safe_argc + (tool_name != NULL ? 3U : 2U);
		exec_argv = calloc(slots, sizeof(char *));
		if (exec_argv == NULL) {
			fprintf(stderr, "xcrun: error: out of memory\n");
			exit(1);
		}
		exec_argv[pos++] = xcrun_bin;
		if (tool_name != NULL) {
			exec_argv[pos++] = (char *)tool_name;
		}
		for (int i = 0; i < safe_argc; i++) {
			exec_argv[pos++] = safe_argv[i];
		}
		exec_argv[pos] = NULL;

		invoke_xcrun_via_binary(xcrun_bin, exec_argv, devdir, was_env);
	}

	if (tool_name != NULL && tool_name[0] != '\0') {
		bool safe_tool = (strchr(tool_name, '/') == NULL &&
		                  strcmp(tool_name, ".") != 0 &&
		                  strcmp(tool_name, "..") != 0);

		if (safe_tool) {
			for (size_t d = 0;
			     d < sizeof(fallback_dirs) / sizeof(fallback_dirs[0]); d++) {
				char candidate[PATH_MAX];

				if (path_join(candidate, sizeof(candidate), fallback_dirs[d],
				              tool_name) && path_is_exec_file(candidate)) {
					char errbuf[128];
					size_t pos = 0;
					char **exec_argv = calloc((size_t)safe_argc + 2U,
					                          sizeof(char *));
					if (exec_argv == NULL) {
						fprintf(stderr, "xcrun: error: out of memory\n");
						exit(1);
					}
					exec_argv[pos++] = candidate;
					for (int i = 0; i < safe_argc; i++) {
						exec_argv[pos++] = safe_argv[i];
					}
					exec_argv[pos] = NULL;
					execv(candidate, exec_argv);
					fprintf(stderr,
					        "xcrun: error: unable to execute '%s' (%s)\n",
					        candidate,
					        format_errno(errno, errbuf, sizeof(errbuf)));
					free(exec_argv);
					exit(1);
				}
			}
		}

		const char *prog = getprogname();
		xcselect_trigger_install_request(
		    (prog != NULL && prog[0] != '\0') ? prog : tool_name);
	} else {
		xcselect_trigger_install_request("xcrun");
	}
	exit(1);
}

bool
xcselect_trigger_install_request(const char *tool_name)
{
	if (tool_name != NULL && tool_name[0] != '\0') {
		fprintf(stderr,
		        "xcselect: unable to locate developer tool '%s' and no "
		        "graphical installer is available in this environment.\n"
		        "Set DEVELOPER_DIR or SDKROOT, mount a macOS toolchain or "
		        "SDK, or install packages with kl.\n",
		        tool_name);
	} else {
		fprintf(stderr,
		        "xcselect: no developer tools were found and no graphical "
		        "installer is available in this environment.\n"
		        "Set DEVELOPER_DIR or SDKROOT, mount a macOS toolchain or "
		        "SDK, or install packages with kl.\n");
	}
	return false;
}
