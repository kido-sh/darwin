/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "xcselect_private.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <unistd.h>

static int g_failures = 0;
static bool g_diff_mode = false;
static bool g_kido_extra = false;

#define CHECK(cond, fmt, ...)                                                  \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "FAIL [%s:%d]: " fmt "\n", __FILE__,   \
			        __LINE__, ##__VA_ARGS__);                      \
			g_failures++;                                          \
		}                                                              \
	} while (0)

static void
mkdir_p(const char *path)
{
	char tmp[PATH_MAX];
	size_t len;

	assert(strlcpy(tmp, path, sizeof(tmp)) < sizeof(tmp));
	len = strlen(tmp);
	for (size_t i = 1; i < len; i++) {
		if (tmp[i] == '/') {
			tmp[i] = '\0';
			mkdir(tmp, 0755);
			tmp[i] = '/';
		}
	}
	mkdir(tmp, 0755);
}

static void
write_text_file(const char *path, const char *content, mode_t mode)
{
	int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, mode);
	assert(fd >= 0);
	if (content != NULL) {
		size_t n = strlen(content);
		assert((size_t)write(fd, content, n) == n);
	}
	assert(fchmod(fd, mode) == 0);
	close(fd);
}

static void
write_synthetic_shim_macho(const char *path)
{
	struct {
		struct mach_header_64 hdr;
		struct segment_command_64 seg;
		struct section_64 sect;
	} bin;
	int fd;

	memset(&bin, 0, sizeof(bin));
	bin.hdr.magic = MH_MAGIC_64;
	bin.hdr.cputype = CPU_TYPE_ARM64;
	bin.hdr.filetype = MH_EXECUTE;
	bin.hdr.ncmds = 1;
	bin.hdr.sizeofcmds = sizeof(bin.seg) + sizeof(bin.sect);

	bin.seg.cmd = LC_SEGMENT_64;
	bin.seg.cmdsize = sizeof(bin.seg) + sizeof(bin.sect);
	memcpy(bin.seg.segname, "__DATA", 6);
	bin.seg.nsects = 1;

	memcpy(bin.sect.sectname, "__xcrun_shim", 12);
	memcpy(bin.sect.segname, "__DATA", 6);

	fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0755);
	assert(fd >= 0);
	assert(write(fd, &bin, sizeof(bin)) == (ssize_t)sizeof(bin));
	close(fd);
}

static void
write_be32_buf(uint8_t *dst, uint32_t v)
{
	dst[0] = (uint8_t)((v >> 24) & 0xffU);
	dst[1] = (uint8_t)((v >> 16) & 0xffU);
	dst[2] = (uint8_t)((v >> 8) & 0xffU);
	dst[3] = (uint8_t)(v & 0xffU);
}

static void
write_synthetic_fat_shim_macho(const char *path)
{
	struct {
		struct mach_header_64 hdr;
		struct segment_command_64 seg;
		struct section_64 sect;
	} slice;
	uint8_t fat_hdr[28];
	int fd;

	memset(&slice, 0, sizeof(slice));
	slice.hdr.magic = MH_MAGIC_64;
	slice.hdr.cputype = CPU_TYPE_ARM64;
	slice.hdr.filetype = MH_EXECUTE;
	slice.hdr.ncmds = 1;
	slice.hdr.sizeofcmds = sizeof(slice.seg) + sizeof(slice.sect);
	slice.seg.cmd = LC_SEGMENT_64;
	slice.seg.cmdsize = sizeof(slice.seg) + sizeof(slice.sect);
	memcpy(slice.seg.segname, "__DATA", 6);
	slice.seg.nsects = 1;
	memcpy(slice.sect.sectname, "__xcrun_shim", 12);
	memcpy(slice.sect.segname, "__DATA", 6);

	memset(fat_hdr, 0, sizeof(fat_hdr));
	write_be32_buf(fat_hdr + 0, FAT_MAGIC);
	write_be32_buf(fat_hdr + 4, 1);
	write_be32_buf(fat_hdr + 8, (uint32_t)CPU_TYPE_ARM64);
	write_be32_buf(fat_hdr + 12, 0);
	write_be32_buf(fat_hdr + 16, (uint32_t)sizeof(fat_hdr));
	write_be32_buf(fat_hdr + 20, (uint32_t)sizeof(slice));
	write_be32_buf(fat_hdr + 24, 14);

	fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0755);
	assert(fd >= 0);
	assert(write(fd, fat_hdr, sizeof(fat_hdr)) == (ssize_t)sizeof(fat_hdr));
	assert(write(fd, &slice, sizeof(slice)) == (ssize_t)sizeof(slice));
	close(fd);
}

static void
write_malformed_macho(const char *path)
{
	struct {
		struct mach_header_64 hdr;
		struct load_command lc;
	} bin;
	int fd;

	memset(&bin, 0, sizeof(bin));
	bin.hdr.magic = MH_MAGIC_64;
	bin.hdr.ncmds = 100;
	bin.hdr.sizeofcmds = sizeof(bin.lc);
	bin.lc.cmd = LC_SEGMENT_64;
	bin.lc.cmdsize = 0; /* Malformed zero cmdsize must not infinite-loop. */

	fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0755);
	assert(fd >= 0);
	assert(write(fd, &bin, sizeof(bin)) == (ssize_t)sizeof(bin));
	close(fd);
}

static void
write_huge_nsects_macho(const char *path)
{
	struct {
		struct mach_header_64 hdr;
		struct segment_command_64 seg;
	} bin;
	int fd;

	memset(&bin, 0, sizeof(bin));
	bin.hdr.magic = MH_MAGIC_64;
	bin.hdr.ncmds = 1;
	bin.hdr.sizeofcmds = sizeof(bin.seg);
	bin.seg.cmd = LC_SEGMENT_64;
	bin.seg.cmdsize = sizeof(bin.seg);
	memcpy(bin.seg.segname, "__DATA", 6);
	bin.seg.nsects = UINT32_MAX;

	fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0755);
	assert(fd >= 0);
	assert(write(fd, &bin, sizeof(bin)) == (ssize_t)sizeof(bin));
	close(fd);
}

static void
run_child_invoke(const char *devdir_env, const char *xcrun_log_env,
                 const char *tool, int argc, char **argv, bool req_xcode,
                 int *out_exit, char *stdout_buf, size_t stdout_sz,
                 char *stderr_buf, size_t stderr_sz)
{
	int out_pipe[2];
	int err_pipe[2];
	pid_t pid;
	int status = 0;

	assert(pipe(out_pipe) == 0);
	assert(pipe(err_pipe) == 0);
	fflush(stdout);
	fflush(stderr);

	pid = fork();
	assert(pid >= 0);
	if (pid == 0) {
		close(out_pipe[0]);
		close(err_pipe[0]);
		dup2(out_pipe[1], STDOUT_FILENO);
		dup2(err_pipe[1], STDERR_FILENO);
		close(out_pipe[1]);
		close(err_pipe[1]);

		if (devdir_env != NULL) {
			setenv("DEVELOPER_DIR", devdir_env, 1);
		} else {
			unsetenv("DEVELOPER_DIR");
		}
		if (xcrun_log_env != NULL) {
			setenv("xcrun_log", xcrun_log_env, 1);
		} else {
			unsetenv("xcrun_log");
		}
		xcselect_invoke_xcrun(tool, argc, argv, req_xcode);
		_exit(99);
	}

	close(out_pipe[1]);
	close(err_pipe[1]);

	size_t off = 0;
	ssize_t n;
	while (off + 1 < stdout_sz &&
	       (n = read(out_pipe[0], stdout_buf + off, stdout_sz - 1 - off)) > 0) {
		off += (size_t)n;
	}
	stdout_buf[off] = '\0';
	close(out_pipe[0]);

	off = 0;
	while (off + 1 < stderr_sz &&
	       (n = read(err_pipe[0], stderr_buf + off, stderr_sz - 1 - off)) > 0) {
		off += (size_t)n;
	}
	stderr_buf[off] = '\0';
	close(err_pipe[0]);

	assert(waitpid(pid, &status, 0) == pid);
	*out_exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static void
test_version(void)
{
	const char *v = xcselect_get_version();
	CHECK(v != NULL && strcmp(v, "2418") == 0, "version=%s", v ? v : "NULL");
	if (g_diff_mode) {
		printf("version=%s\n", v ? v : "NULL");
	}
}

static void
test_bundle_is_developer_tool(void)
{
	static const struct {
		const char *id;
		bool expect;
	} cases[] = {
		{ NULL, false },
		{ "", false },
		{ "com.apple.", false },
		{ "com.apple.dt.Xcode", true },
		{ "com.apple.dt.SourceEdit", true },
		{ "com.apple.iphonesimulator", true },
		{ "com.apple.dt.Instruments", true },
		{ "com.apple.FileMerge", true },
		{ "com.apple.itunes.connect.ApplicationLoader", true },
		{ "com.apple.AccessibilityInspector", true },
		{ "com.apple.RealityComposer", true },
		{ "com.apple.RealityComposerPro", true },
		{ "com.apple.Xcode", false },
		{ "com.apple.Instruments", false },
		{ "com.apple.dt.Xcode.extra", false },
		{ "com.apple.Safari", false },
		{ "com.other.dt.Xcode", false },
		{ "COM.APPLE.dt.Xcode", false },
		{ "com.apple.dt.xcode", false },
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		bool got = xcselect_bundle_is_developer_tool(cases[i].id);
		CHECK(got == cases[i].expect, "bundle(%s)=%d want=%d",
		      cases[i].id ? cases[i].id : "NULL", got, cases[i].expect);
		if (g_diff_mode) {
			printf("bundle[%zu]=%d\n", i, got);
		}
	}
}

static void
test_find_developer_contents(const char *root)
{
	char p[PATH_MAX];
	char p2[PATH_MAX];
	char out[PATH_MAX];
	char expect[PATH_MAX];
	bool clt = false;
	bool ok;

	/* 1. .app bundle with Contents/Developer */
	snprintf(p, sizeof(p), "%s/MyXcode.app/Contents/Developer", root);
	mkdir_p(p);
	snprintf(p2, sizeof(p2), "%s/MyXcode.app", root);
	clt = true;
	ok = xcselect_find_developer_contents_from_path(p2, out, sizeof(out), &clt);
	CHECK(ok && strcmp(out, p) == 0 && !clt, "app bundle: ok=%d out=%s clt=%d",
	      ok, out, clt);
	if (g_diff_mode) {
		printf("find.app=%d:%d:%d\n", ok, strcmp(out, p) == 0, clt);
	}

	/* 2. .app with trailing slashes */
	snprintf(p2, sizeof(p2), "%s/MyXcode.app///", root);
	clt = true;
	ok = xcselect_find_developer_contents_from_path(p2, out, sizeof(out), &clt);
	CHECK(ok && strcmp(out, p) == 0 && !clt, "app trailing slash");
	if (g_diff_mode) {
		printf("find.app_slash=%d:%d:%d\n", ok, strcmp(out, p) == 0, clt);
	}

	/* 3. Relative path */
	char saved_cwd[PATH_MAX];
	assert(getcwd(saved_cwd, sizeof(saved_cwd)) != NULL);
	assert(chdir(root) == 0);
	clt = true;
	ok = xcselect_find_developer_contents_from_path("MyXcode.app", out,
	                                                sizeof(out), &clt);
	assert(chdir(saved_cwd) == 0);
	/* Compare suffix since macOS /tmp may resolve to /private/tmp via getcwd */
	CHECK(ok && strstr(out, "/MyXcode.app/Contents/Developer") != NULL && !clt,
	      "relative path: ok=%d out=%s clt=%d", ok, out, clt);
	if (g_diff_mode) {
		printf("find.rel=%d:%d:%d\n", ok,
		       strstr(out, "/MyXcode.app/Contents/Developer") != NULL, clt);
	}

	/* 4. Directory containing Xcode.app/Contents/Developer */
	snprintf(p, sizeof(p), "%s/AppsDir/Xcode.app/Contents/Developer", root);
	mkdir_p(p);
	snprintf(p2, sizeof(p2), "%s/AppsDir", root);
	clt = true;
	ok = xcselect_find_developer_contents_from_path(p2, out, sizeof(out), &clt);
	CHECK(ok && strcmp(out, p) == 0 && !clt, "AppsDir/Xcode.app");
	if (g_diff_mode) {
		printf("find.xcode_in_dir=%d:%d:%d\n", ok, strcmp(out, p) == 0, clt);
	}

	/* 5. Direct developer dir with usr/lib/libxcrun.dylib */
	snprintf(p, sizeof(p), "%s/DirectLib/usr/lib", root);
	mkdir_p(p);
	snprintf(p2, sizeof(p2), "%s/DirectLib/usr/lib/libxcrun.dylib", root);
	write_text_file(p2, "stub", 0644);
	snprintf(p, sizeof(p), "%s/DirectLib", root);
	clt = true;
	ok = xcselect_find_developer_contents_from_path(p, out, sizeof(out), &clt);
	CHECK(ok && strcmp(out, p) == 0 && !clt, "direct libxcrun");
	if (g_diff_mode) {
		printf("find.direct_lib=%d:%d:%d\n", ok, strcmp(out, p) == 0, clt);
	}

	/* 6. Direct developer dir with executable usr/bin/xcrun */
	snprintf(p, sizeof(p), "%s/DirectBin/usr/bin", root);
	mkdir_p(p);
	snprintf(p2, sizeof(p2), "%s/DirectBin/usr/bin/xcrun", root);
	write_text_file(p2, "#!/bin/sh\n", 0755);
	snprintf(p, sizeof(p), "%s/DirectBin", root);
	clt = true;
	ok = xcselect_find_developer_contents_from_path(p, out, sizeof(out), &clt);
	CHECK(ok && strcmp(out, p) == 0 && !clt, "direct bin xcrun");
	if (g_diff_mode) {
		printf("find.direct_bin=%d:%d:%d\n", ok, strcmp(out, p) == 0, clt);
	}

	/* 7. Non-executable usr/bin/xcrun must fail */
	snprintf(p, sizeof(p), "%s/NonExecBin/usr/bin", root);
	mkdir_p(p);
	snprintf(p2, sizeof(p2), "%s/NonExecBin/usr/bin/xcrun", root);
	write_text_file(p2, "#!/bin/sh\n", 0644);
	snprintf(p, sizeof(p), "%s/NonExecBin", root);
	ok = xcselect_find_developer_contents_from_path(p, out, sizeof(out), &clt);
	CHECK(!ok, "non-exec xcrun should fail");
	if (g_diff_mode) {
		printf("find.nonexec=%d\n", ok);
	}

	/* 8. Basename CommandLineTools with libxcrun -> is_clt = true */
	snprintf(p, sizeof(p), "%s/CLTWrap/CommandLineTools/usr/lib", root);
	mkdir_p(p);
	snprintf(p2, sizeof(p2),
	         "%s/CLTWrap/CommandLineTools/usr/lib/libxcrun.dylib", root);
	write_text_file(p2, "stub", 0644);
	snprintf(p, sizeof(p), "%s/CLTWrap/CommandLineTools", root);
	clt = false;
	ok = xcselect_find_developer_contents_from_path(p, out, sizeof(out), &clt);
	CHECK(ok && strcmp(out, p) == 0 && clt, "CLT basename");
	if (g_diff_mode) {
		printf("find.clt=%d:%d:%d\n", ok, strcmp(out, p) == 0, clt);
	}

	/* 9. Basename CommandLineTools without xcrun -> false */
	snprintf(p, sizeof(p), "%s/EmptyCLT/CommandLineTools", root);
	mkdir_p(p);
	ok = xcselect_find_developer_contents_from_path(p, out, sizeof(out), &clt);
	CHECK(!ok, "empty CLT should fail");
	if (g_diff_mode) {
		printf("find.clt_empty=%d\n", ok);
	}

	/* 10. Volume with mach_kernel and Library/Developer/CommandLineTools */
	snprintf(p, sizeof(p), "%s/Vol/Library/Developer/CommandLineTools/usr/lib",
	         root);
	mkdir_p(p);
	snprintf(p2, sizeof(p2), "%s/Vol/mach_kernel", root);
	write_text_file(p2, "k", 0644);
	snprintf(p2, sizeof(p2),
	         "%s/Vol/Library/Developer/CommandLineTools/usr/lib/libxcrun.dylib",
	         root);
	write_text_file(p2, "stub", 0644);
	snprintf(p, sizeof(p), "%s/Vol", root);
	snprintf(expect, sizeof(expect),
	         "%s/Vol/Library/Developer/CommandLineTools", root);
	clt = false;
	ok = xcselect_find_developer_contents_from_path(p, out, sizeof(out), &clt);
	CHECK(ok && strcmp(out, expect) == 0 && clt, "volume mach_kernel");
	if (g_diff_mode) {
		printf("find.vol=%d:%d:%d\n", ok, strcmp(out, expect) == 0, clt);
	}

	/* 11. Symlink to valid .app and dangling symlink */
	snprintf(p, sizeof(p), "%s/MyXcode.app", root);
	snprintf(p2, sizeof(p2), "%s/LinkXcode.app", root);
	assert(symlink(p, p2) == 0);
	snprintf(expect, sizeof(expect), "%s/LinkXcode.app/Contents/Developer",
	         root);
	clt = true;
	ok = xcselect_find_developer_contents_from_path(p2, out, sizeof(out), &clt);
	CHECK(ok && strcmp(out, expect) == 0 && !clt, "symlink .app");
	if (g_diff_mode) {
		printf("find.symlink=%d:%d:%d\n", ok, strcmp(out, expect) == 0, clt);
	}

	snprintf(p2, sizeof(p2), "%s/DanglingLink", root);
	assert(symlink("/nonexistent/target/xcselect", p2) == 0);
	ok = xcselect_find_developer_contents_from_path(p2, out, sizeof(out), &clt);
	CHECK(!ok, "dangling symlink should fail");
	if (g_diff_mode) {
		printf("find.dangling=%d\n", ok);
	}

	/* 12. Extra defensive checks on our implementation */
	if (g_kido_extra) {
		char tiny[4] = "xxx";
		CHECK(!xcselect_find_developer_contents_from_path(NULL, out,
		                                                  sizeof(out), &clt),
		      "NULL path");
		CHECK(!xcselect_find_developer_contents_from_path(p, NULL,
		                                                  sizeof(out), &clt),
		      "NULL buf");
		CHECK(!xcselect_find_developer_contents_from_path(p, tiny, 0, &clt),
		      "zero bufsize");
		CHECK(!xcselect_find_developer_contents_from_path(p, tiny, -5, &clt),
		      "neg bufsize");
		CHECK(!xcselect_find_developer_contents_from_path(p, tiny,
		                                                  sizeof(tiny), NULL),
		      "tiny buf truncation");
		snprintf(p2, sizeof(p2), "%s/MyXcode.app", root);
		CHECK(xcselect_find_developer_contents_from_path(p2, out,
		                                                 sizeof(out), NULL),
		      "NULL is_clt allowed");
	}
}

static void
test_get_developer_dir_path(const char *root)
{
	char app[PATH_MAX];
	char expect[PATH_MAX];
	char clt_dir[PATH_MAX];
	char buf[PATH_MAX];
	bool we = false, clt = false, def = false;
	bool ok;

	snprintf(app, sizeof(app), "%s/MyXcode.app", root);
	snprintf(expect, sizeof(expect), "%s/MyXcode.app/Contents/Developer", root);
	snprintf(clt_dir, sizeof(clt_dir), "%s/CLTWrap/CommandLineTools", root);

	/* 1. DEVELOPER_DIR pointing to .app normalizes env and returns Contents/Developer */
	setenv("DEVELOPER_DIR", app, 1);
	we = false; clt = true; def = true;
	ok = xcselect_get_developer_dir_path(buf, sizeof(buf), &we, &clt, &def);
	const char *after_env = getenv("DEVELOPER_DIR");
	CHECK(ok && strcmp(buf, expect) == 0 && we && !clt && !def,
	      "env .app: ok=%d buf=%s we=%d clt=%d def=%d", ok, buf, we, clt, def);
	CHECK(after_env != NULL && strcmp(after_env, expect) == 0,
	      "env normalized to %s", after_env ? after_env : "NULL");
	if (g_diff_mode) {
		printf("devdir.env_app=%d:%d:%d:%d:%d:%d\n", ok,
		       strcmp(buf, expect) == 0, we, clt, def,
		       after_env != NULL && strcmp(after_env, expect) == 0);
	}

	/* 2. DEVELOPER_DIR pointing to CommandLineTools sets is_clt */
	setenv("DEVELOPER_DIR", clt_dir, 1);
	we = false; clt = false; def = true;
	ok = xcselect_get_developer_dir_path(buf, sizeof(buf), &we, &clt, &def);
	CHECK(ok && strcmp(buf, clt_dir) == 0 && we && clt && !def,
	      "env CLT: ok=%d buf=%s we=%d clt=%d def=%d", ok, buf, we, clt, def);
	if (g_diff_mode) {
		printf("devdir.env_clt=%d:%d:%d:%d:%d\n", ok,
		       strcmp(buf, clt_dir) == 0, we, clt, def);
	}

	/* 3. DEVELOPER_DIR pointing to non-existent dir still succeeds raw */
	setenv("DEVELOPER_DIR", "/nonexistent/xcselect/custom", 1);
	we = false; clt = true; def = true;
	ok = xcselect_get_developer_dir_path(buf, sizeof(buf), &we, &clt, &def);
	CHECK(ok && strcmp(buf, "/nonexistent/xcselect/custom") == 0 &&
	      we && !clt && !def, "env raw nonexistent");
	if (g_diff_mode) {
		printf("devdir.env_raw=%d:%d:%d:%d:%d\n", ok,
		       strcmp(buf, "/nonexistent/xcselect/custom") == 0, we, clt, def);
	}

	/* 4. Empty DEVELOPER_DIR treated as unset */
	setenv("DEVELOPER_DIR", "", 1);
	we = true; clt = false; def = false;
	ok = xcselect_get_developer_dir_path(buf, sizeof(buf), &we, &clt, &def);
	CHECK(!we, "empty env must have was_env=false");
	if (g_diff_mode) {
		printf("devdir.empty_env=%d:%d:%d:%d\n", ok, we, clt, def);
	}

	/* 5. Unset DEVELOPER_DIR */
	unsetenv("DEVELOPER_DIR");
	we = true; clt = false; def = false;
	ok = xcselect_get_developer_dir_path(buf, sizeof(buf), &we, &clt, &def);
	CHECK(!we, "unset env must have was_env=false");
	if (g_diff_mode) {
		printf("devdir.unset=%d:%s:%d:%d:%d\n", ok, ok ? buf : "", we, clt,
		       def);
	}

	/* 6. Extra defensive checks */
	if (g_kido_extra) {
		char tiny[6];
		char one[1];
		char direct_dev[PATH_MAX];
		snprintf(direct_dev, sizeof(direct_dev), "%s/DirectLib", root);

		setenv("DEVELOPER_DIR", "/a/b/c/d/e/f", 1);
		CHECK(!xcselect_get_developer_dir_path(NULL, 10, &we, &clt, &def),
		      "NULL buf");
		CHECK(!xcselect_get_developer_dir_path(tiny, 0, &we, &clt, &def),
		      "zero buf");
		CHECK(!xcselect_get_developer_dir_path(tiny, -1, &we, &clt, &def),
		      "neg buf");
		ok = xcselect_get_developer_dir_path(tiny, sizeof(tiny), NULL, NULL,
		                                     NULL);
		CHECK(ok && strcmp(tiny, "/a/b/") == 0, "tiny trunc ok=%d '%s'", ok,
		      tiny);

		/* Tiny buffer with valid direct devdir must NOT corrupt DEVELOPER_DIR */
		setenv("DEVELOPER_DIR", direct_dev, 1);
		ok = xcselect_get_developer_dir_path(tiny, sizeof(tiny), &we, &clt,
		                                     &def);
		const char *env_now = getenv("DEVELOPER_DIR");
		CHECK(ok && we && env_now != NULL && strcmp(env_now, direct_dev) == 0,
		      "tiny buf corrupted DEVELOPER_DIR: '%s'",
		      env_now ? env_now : "NULL");

		ok = xcselect_get_developer_dir_path(one, sizeof(one), &we, &clt,
		                                     &def);
		env_now = getenv("DEVELOPER_DIR");
		CHECK(ok && one[0] == '\0' && we && env_now != NULL &&
		      strcmp(env_now, direct_dev) == 0,
		      "1-byte buf corrupted DEVELOPER_DIR");

		/* 1-byte buffer with unset DEVELOPER_DIR must not falsely set default */
		unsetenv("DEVELOPER_DIR");
		struct stat link_st;
		if (lstat("/var/db/xcode_select_link", &link_st) == 0) {
			we = true; clt = false; def = true;
			ok = xcselect_get_developer_dir_path(one, sizeof(one), &we,
			                                     &clt, &def);
			CHECK(ok && one[0] == '\0' && !we && !def,
			      "1-byte unset buf: ok=%d we=%d def=%d", ok, we, def);
		}
	}
}

static void
test_developer_dir_matches_path(const char *root)
{
	char app[PATH_MAX];
	char probe[PATH_MAX];

	struct {
		const char *dev;
		const char *path;
		bool expect;
	} cases[] = {
		{ "/Applications/Xcode.app/Contents/Developer",
		  "/Applications/Xcode.app", true },
		{ "/Applications/Xcode.app/Contents/Developer",
		  "/Applications/Xcode.app/Contents/MacOS/Xcode", true },
		{ "/Applications/Xcode.app/Contents/Developer///",
		  "/Applications/Xcode.app/Contents/Developer/usr/bin/clang", true },
		{ "/Applications/Xcode.app/Contents/Developer",
		  "/Applications/Xcode-beta.app", false },
		{ "/Applications/Xcode.app/Contents/Developer",
		  "/Applications", false },
		{ "/Applications/Xcode.app/Contents/Developer", NULL, false },
		{ "/Library/Developer/CommandLineTools",
		  "/Library/Developer/CommandLineTools/usr/bin/git", true },
		{ "/Library/Developer/CommandLineTools",
		  "/Library/Developer", false },
		{ "", "/anything", false },
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		bool got = xcselect_developer_dir_matches_path(cases[i].dev,
		                                               cases[i].path);
		CHECK(got == cases[i].expect, "matches[%zu]=%d want=%d", i, got,
		      cases[i].expect);
		if (g_diff_mode) {
			printf("matches[%zu]=%d\n", i, got);
		}
	}

	/* NULL developer_dir falls back to active DEVELOPER_DIR */
	snprintf(app, sizeof(app), "%s/MyXcode.app", root);
	snprintf(probe, sizeof(probe), "%s/MyXcode.app/Contents/Info.plist", root);
	setenv("DEVELOPER_DIR", app, 1);
	bool m1 = xcselect_developer_dir_matches_path(NULL, probe);
	bool m2 = xcselect_developer_dir_matches_path(NULL, "/other/path");
	CHECK(m1 && !m2, "matches(NULL): m1=%d m2=%d", m1, m2);
	if (g_diff_mode) {
		printf("matches.null=%d:%d\n", m1, m2);
	}
	if (g_kido_extra) {
		char huge[PATH_MAX + 64];
		memset(huge, 'a', sizeof(huge) - 1);
		huge[0] = '/';
		huge[sizeof(huge) - 1] = '\0';
		CHECK(!xcselect_developer_dir_matches_path(huge, huge),
		      "oversized developer_dir must not falsely match");
	}
	unsetenv("DEVELOPER_DIR");
}

static void
create_fake_sdk(const char *sdks_dir, const char *sdk_name, bool valid)
{
	char p[PATH_MAX];
	if (valid) {
		snprintf(p, sizeof(p), "%s/%s/usr/include/sys", sdks_dir, sdk_name);
		mkdir_p(p);
		snprintf(p, sizeof(p), "%s/%s/usr/include/sys/cdefs.h", sdks_dir,
		         sdk_name);
		write_text_file(p, "/* stub */\n", 0644);
	} else {
		snprintf(p, sizeof(p), "%s/%s", sdks_dir, sdk_name);
		mkdir_p(p);
	}
}

static void
test_host_sdk_path(const char *root)
{
	char *sentinel = (char *)(uintptr_t)0xdeadbeefULL;
	char *sdk = sentinel;
	errno_t rc;

	/* Invalid inputs must return EINVAL and leave *path unchanged */
	rc = xcselect_host_sdk_path(XCSELECT_HOST_SDK_POLICY_LATEST, NULL);
	CHECK(rc == EINVAL, "NULL path rc=%d", rc);
	if (g_diff_mode) printf("sdk.null=%d\n", rc);

	uint32_t bad_policies[] = { 0, 4, 999 };
	for (size_t i = 0; i < sizeof(bad_policies) / sizeof(bad_policies[0]); i++) {
		sdk = sentinel;
		rc = xcselect_host_sdk_path(bad_policies[i], &sdk);
		CHECK(rc == EINVAL && sdk == sentinel, "bad policy %u rc=%d",
		      bad_policies[i], rc);
		if (g_diff_mode) {
			printf("sdk.bad[%zu]=%d:%d\n", i, rc, sdk == sentinel);
		}
	}

	/*
	 * Verify LATEST and MATCHING_PREFERRED against real CLT / Xcode SDKs when
	 * present on the host (compare canonical realpath since MacOSX27.sdk and
	 * MacOSX27.2.sdk are symlinks to the same SDK directory).
	 */
	unsetenv("DEVELOPER_DIR");
	struct stat st;
	if (stat("/Library/Developer/CommandLineTools/SDKs", &st) == 0) {
		char rp[PATH_MAX];
		sdk = NULL;
		rc = xcselect_host_sdk_path(XCSELECT_HOST_SDK_POLICY_LATEST, &sdk);
		CHECK(rc == 0 && sdk != NULL && realpath(sdk, rp) != NULL,
		      "host CLT latest: rc=%d sdk=%s", rc, sdk ? sdk : "NULL");
		if (g_diff_mode) {
			printf("sdk.clt_latest=%d:%s\n", rc,
			       (rc == 0 && sdk) ? rp : "");
		}
		free(sdk);

		sdk = NULL;
		rc = xcselect_host_sdk_path(XCSELECT_HOST_SDK_POLICY_MATCHING_PREFERRED,
		                            &sdk);
		CHECK(rc == 0 && sdk != NULL && realpath(sdk, rp) != NULL,
		      "host CLT matching_preferred: rc=%d sdk=%s", rc,
		      sdk ? sdk : "NULL");
		if (g_diff_mode) {
			printf("sdk.clt_matchpref=%d:%s\n", rc,
			       (rc == 0 && sdk) ? rp : "");
		}
		free(sdk);
	}
	if (stat("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs",
	         &st) == 0) {
		char rp[PATH_MAX];
		setenv("DEVELOPER_DIR", "/Applications/Xcode.app", 1);
		sdk = NULL;
		rc = xcselect_host_sdk_path(XCSELECT_HOST_SDK_POLICY_LATEST, &sdk);
		CHECK(rc == 0 && sdk != NULL && realpath(sdk, rp) != NULL,
		      "host Xcode latest: rc=%d sdk=%s", rc, sdk ? sdk : "NULL");
		if (g_diff_mode) {
			printf("sdk.xcode_latest=%d:%s\n", rc,
			       (rc == 0 && sdk) ? rp : "");
		}
		free(sdk);
		unsetenv("DEVELOPER_DIR");
	}

	/*
	 * Modern macOS SDK version selection (verified on our clean-room library;
	 * Apple's macOS 27.2 binary still checks strncmp("MacOSX10.", 9)).
	 */
	if (g_kido_extra) {
		char fake_modern[PATH_MAX];
		char modern_sdks[PATH_MAX];
		snprintf(fake_modern, sizeof(fake_modern), "%s/SdkModern.app", root);
		snprintf(modern_sdks, sizeof(modern_sdks),
		         "%s/SdkModern.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs",
		         root);
		mkdir_p(modern_sdks);
		create_fake_sdk(modern_sdks, "MacOSX10.14.sdk", true);
		create_fake_sdk(modern_sdks, "MacOSX10.15.sdk", true);
		create_fake_sdk(modern_sdks, "MacOSX26.5.sdk", true);
		create_fake_sdk(modern_sdks, "MacOSX27.2.sdk", true);
		create_fake_sdk(modern_sdks, "MacOSX.sdk", true);
		create_fake_sdk(modern_sdks, "MacOSX99.9.sdk", false);
		create_fake_sdk(modern_sdks, "OtherPlatform.sdk", true);

		/* Also synthesize an SDK matching the live host version if different */
		char host_ver[64] = { 0 };
		size_t hv_len = sizeof(host_ver) - 1;
		if (sysctlbyname("kern.osproductversion", host_ver, &hv_len, NULL, 0) == 0 &&
		    host_ver[0] != '\0') {
			char dyn_sdk[96];
			snprintf(dyn_sdk, sizeof(dyn_sdk), "MacOSX%s.sdk", host_ver);
			create_fake_sdk(modern_sdks, dyn_sdk, true);
		}

		setenv("DEVELOPER_DIR", fake_modern, 1);

		sdk = NULL;
		rc = xcselect_host_sdk_path(XCSELECT_HOST_SDK_POLICY_LATEST, &sdk);
		CHECK(rc == 0 && sdk != NULL &&
		      strstr(sdk, "SdkModern.app") != NULL &&
		      strstr(sdk, "OtherPlatform.sdk") == NULL,
		      "modern latest: rc=%d sdk=%s", rc, sdk ? sdk : "NULL");
		free(sdk);

		sdk = NULL;
		rc = xcselect_host_sdk_path(XCSELECT_HOST_SDK_POLICY_MATCHING_ONLY,
		                            &sdk);
		CHECK(rc == 0 && sdk != NULL &&
		      strstr(sdk, "SdkModern.app") != NULL,
		      "modern matching_only: rc=%d sdk=%s", rc, sdk ? sdk : "NULL");
		free(sdk);

		/* Relocated CommandLineTools/SDKs works via DEVELOPER_DIR */
		char reloc_clt[PATH_MAX];
		char reloc_sdks[PATH_MAX];
		snprintf(reloc_clt, sizeof(reloc_clt), "%s/CLTWrap/CommandLineTools",
		         root);
		snprintf(reloc_sdks, sizeof(reloc_sdks), "%s/SDKs", reloc_clt);
		mkdir_p(reloc_sdks);
		create_fake_sdk(reloc_sdks, "MacOSX98.0.sdk", true);
		setenv("DEVELOPER_DIR", reloc_clt, 1);
		sdk = NULL;
		rc = xcselect_host_sdk_path(XCSELECT_HOST_SDK_POLICY_LATEST, &sdk);
		CHECK(rc == 0 && sdk != NULL &&
		      strstr(sdk, "MacOSX98.0.sdk") != NULL,
		      "relocated CLT SDK: rc=%d sdk=%s", rc, sdk ? sdk : "NULL");
		free(sdk);

		/* Only unversioned MacOSX.sdk in isolated dir */
		char fake_unver[PATH_MAX];
		char unver_sdks[PATH_MAX];
		snprintf(fake_unver, sizeof(fake_unver), "%s/SdkUnver.app", root);
		snprintf(unver_sdks, sizeof(unver_sdks),
		         "%s/SdkUnver.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs",
		         root);
		mkdir_p(unver_sdks);
		create_fake_sdk(unver_sdks, "MacOSX.sdk", true);
		setenv("DEVELOPER_DIR", fake_unver, 1);
		sdk = NULL;
		rc = xcselect_host_sdk_path(XCSELECT_HOST_SDK_POLICY_LATEST, &sdk);
		CHECK(rc == 0 && sdk != NULL, "unversioned latest: rc=%d", rc);
		free(sdk);

		/* Higher synthetic SDK beats host CLT SDK */
		char fake_future[PATH_MAX];
		char future_sdks[PATH_MAX];
		snprintf(fake_future, sizeof(fake_future), "%s/SdkFuture.app", root);
		snprintf(future_sdks, sizeof(future_sdks),
		         "%s/SdkFuture.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs",
		         root);
		mkdir_p(future_sdks);
		create_fake_sdk(future_sdks, "MacOSX99.1.sdk", true);
		setenv("DEVELOPER_DIR", fake_future, 1);
		sdk = NULL;
		rc = xcselect_host_sdk_path(XCSELECT_HOST_SDK_POLICY_LATEST, &sdk);
		CHECK(rc == 0 && sdk != NULL &&
		      strstr(sdk, "MacOSX99.1.sdk") != NULL,
		      "future latest: rc=%d sdk=%s", rc, sdk ? sdk : "NULL");
		free(sdk);
	}

	unsetenv("DEVELOPER_DIR");
}

static void
test_manpaths(const char *root)
{
	char dev[PATH_MAX];
	snprintf(dev, sizeof(dev), "%s/MyXcode.app/Contents/Developer", root);
	setenv("DEVELOPER_DIR", dev, 1);

	xcselect_manpaths *mp = xcselect_get_manpaths(NULL);
	CHECK(mp != NULL, "manpaths fallback returned NULL");
	if (mp != NULL) {
		uint32_t n = xcselect_manpaths_get_num_paths(mp);
		CHECK(n == 3, "fallback manpaths count=%u want 3", n);
		if (g_diff_mode) printf("man.fallback.n=%u\n", n);

		const char *suffixes[3] = {
			"/MyXcode.app/Contents/Developer/usr/share/man",
			"/MyXcode.app/Contents/Developer/usr/llvm-gcc-4.2/share/man",
			"/MyXcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/share/man",
		};
		for (uint32_t i = 0; i < n && i < 3; i++) {
			const char *p = xcselect_manpaths_get_path(mp, i);
			CHECK(p != NULL && strstr(p, suffixes[i]) != NULL,
			      "man[%u]=%s want suffix %s", i, p ? p : "NULL",
			      suffixes[i]);
			if (g_diff_mode) {
				printf("man.fallback[%u]=%d\n", i,
				       p != NULL && strstr(p, suffixes[i]) != NULL);
			}
		}
		CHECK(xcselect_manpaths_get_path(mp, 100) == NULL,
		      "man OOB 100 must be NULL");
		if (g_diff_mode) {
			printf("man.oob100=%d\n",
			       xcselect_manpaths_get_path(mp, 100) == NULL);
		}
		if (g_kido_extra) {
			/* Apple has off-by-one <= bug at idx==n; verify ours returns NULL */
			CHECK(xcselect_manpaths_get_path(mp, n) == NULL,
			      "man idx==n must be NULL");
			CHECK(xcselect_manpaths_get_num_paths(NULL) == 0,
			      "man num(NULL)==0");
			CHECK(xcselect_manpaths_get_path(NULL, 0) == NULL,
			      "man path(NULL,0)==NULL");
			xcselect_manpaths_free(NULL);
		}
		xcselect_manpaths_free(mp);
	}

	/* If real /Applications/Xcode.app exists on host, test libxcrun.dylib path */
	struct stat st;
	if (stat("/Applications/Xcode.app/Contents/Developer/usr/lib/libxcrun.dylib",
	         &st) == 0) {
		setenv("DEVELOPER_DIR", "/Applications/Xcode.app", 1);
		mp = xcselect_get_manpaths(NULL);
		CHECK(mp != NULL, "real Xcode manpaths NULL");
		if (mp != NULL) {
			uint32_t n = xcselect_manpaths_get_num_paths(mp);
			CHECK(n >= 3, "real Xcode manpaths count=%u", n);
			if (g_diff_mode) {
				printf("man.xcode.n=%u\n", n);
				for (uint32_t i = 0; i < n; i++) {
					printf("man.xcode[%u]=%s\n", i,
					       xcselect_manpaths_get_path(mp, i));
				}
			}
			xcselect_manpaths_free(mp);
		}
	}

	unsetenv("DEVELOPER_DIR");
}

static void
test_invoke_xcrun(const char *root)
{
	char dev[PATH_MAX];
	char bin[PATH_MAX];
	char out[2048];
	char err[2048];
	int rc = -1;

	/* 1. Fake developer dir with executable usr/bin/xcrun script */
	snprintf(dev, sizeof(dev), "%s/FakeExecDev", root);
	snprintf(bin, sizeof(bin), "%s/usr/bin", dev);
	mkdir_p(bin);
	snprintf(bin, sizeof(bin), "%s/usr/bin/xcrun", dev);
	write_text_file(bin,
	                "#!/bin/sh\n"
	                "printf 'args:%s|log:%s\\n' \"$*\" \"${xcrun_log:-UNSET}\"\n",
	                0755);

	char *args1[] = { "-alpha", "-beta" };
	run_child_invoke(dev, "1", "mytool", 2, args1, false, &rc, out,
	                 sizeof(out), err, sizeof(err));
	CHECK(rc == 0 && strcmp(out, "args:mytool -alpha -beta|log:UNSET\n") == 0,
	      "invoke tool: rc=%d out='%s' err='%s'", rc, out, err);
	if (g_diff_mode) {
		printf("invoke.tool=%d:%s", rc, out);
	}

	char *args2[] = { "--version" };
	run_child_invoke(dev, "1", NULL, 1, args2, false, &rc, out, sizeof(out),
	                 err, sizeof(err));
	CHECK(rc == 0 && strcmp(out, "args:--version|log:1\n") == 0,
	      "invoke NULL tool: rc=%d out='%s' err='%s'", rc, out, err);
	if (g_diff_mode) {
		printf("invoke.nulltool=%d:%s", rc, out);
	}

	/* 2. requires_xcode=true with CommandLineTools must fail with exit 1 */
	char clt_dev[PATH_MAX];
	snprintf(clt_dev, sizeof(clt_dev), "%s/ExecCLT/CommandLineTools", root);
	snprintf(bin, sizeof(bin), "%s/usr/bin", clt_dev);
	mkdir_p(bin);
	snprintf(bin, sizeof(bin), "%s/usr/bin/xcrun", clt_dev);
	write_text_file(bin, "#!/bin/sh\necho SHOULD_NOT_RUN\n", 0755);

	run_child_invoke(clt_dev, NULL, "xcodebuild", 0, NULL, true, &rc, out,
	                 sizeof(out), err, sizeof(err));
	CHECK(rc == 1 && out[0] == '\0' && err[0] != '\0',
	      "invoke requires_xcode on CLT: rc=%d out='%s' err='%s'", rc, out, err);
	if (g_diff_mode) {
		printf("invoke.req_xcode_clt=%d:%d:%d\n", rc, out[0] == '\0',
		       err[0] != '\0');
	}

	/* 3. Missing usr/bin/xcrun must fail with exit 1 and stderr message */
	char empty_dev[PATH_MAX];
	snprintf(empty_dev, sizeof(empty_dev), "%s/EmptyDev", root);
	mkdir_p(empty_dev);
	run_child_invoke(empty_dev, NULL, "clang", 0, NULL, false, &rc, out,
	                 sizeof(out), err, sizeof(err));
	CHECK(rc == 1 && err[0] != '\0', "invoke missing xcrun: rc=%d err='%s'",
	      rc, err);
	if (g_diff_mode) {
		printf("invoke.missing=%d:%d\n", rc, err[0] != '\0');
	}

	/* 4. Synthetic Mach-O64 __DATA,__xcrun_shim must be rejected (no loop) */
	char shim_dev[PATH_MAX];
	snprintf(shim_dev, sizeof(shim_dev), "%s/ShimDev", root);
	snprintf(bin, sizeof(bin), "%s/usr/bin", shim_dev);
	mkdir_p(bin);
	snprintf(bin, sizeof(bin), "%s/usr/bin/xcrun", shim_dev);
	write_synthetic_shim_macho(bin);
	run_child_invoke(shim_dev, NULL, "clang", 0, NULL, false, &rc, out,
	                 sizeof(out), err, sizeof(err));
	CHECK(rc == 1 && err[0] != '\0', "invoke shim loop: rc=%d err='%s'",
	      rc, err);
	if (g_diff_mode) {
		printf("invoke.shim=%d:%d\n", rc, err[0] != '\0');
	}

	/* 5. Fat shim + malformed Mach-O64 (cmdsize=0, huge nsects) + traversal */
	if (g_kido_extra) {
		char fat_dev[PATH_MAX];
		snprintf(fat_dev, sizeof(fat_dev), "%s/FatShimDev", root);
		snprintf(bin, sizeof(bin), "%s/usr/bin", fat_dev);
		mkdir_p(bin);
		snprintf(bin, sizeof(bin), "%s/usr/bin/xcrun", fat_dev);
		write_synthetic_fat_shim_macho(bin);
		run_child_invoke(fat_dev, NULL, "clang", 0, NULL, false, &rc, out,
		                 sizeof(out), err, sizeof(err));
		CHECK(rc == 1 && strstr(err, "shim") != NULL,
		      "fat shim rejected: rc=%d err='%s'", rc, err);

		char bad_dev[PATH_MAX];
		snprintf(bad_dev, sizeof(bad_dev), "%s/BadMachoDev", root);
		snprintf(bin, sizeof(bin), "%s/usr/bin", bad_dev);
		mkdir_p(bin);
		snprintf(bin, sizeof(bin), "%s/usr/bin/xcrun", bad_dev);
		write_malformed_macho(bin);
		run_child_invoke(bad_dev, NULL, "clang", 0, NULL, false, &rc, out,
		                 sizeof(out), err, sizeof(err));
		CHECK(rc == 1 && err[0] != '\0', "malformed macho: rc=%d", rc);

		char nsects_dev[PATH_MAX];
		snprintf(nsects_dev, sizeof(nsects_dev), "%s/HugeNsectsDev", root);
		snprintf(bin, sizeof(bin), "%s/usr/bin", nsects_dev);
		mkdir_p(bin);
		snprintf(bin, sizeof(bin), "%s/usr/bin/xcrun", nsects_dev);
		write_huge_nsects_macho(bin);
		run_child_invoke(nsects_dev, NULL, "clang", 0, NULL, false, &rc, out,
		                 sizeof(out), err, sizeof(err));
		CHECK(rc == 1 && err[0] != '\0', "huge nsects macho: rc=%d", rc);

		/* Path traversal tool_name when no devdir exists must not exec /bin/sh */
		struct stat def_st;
		if (stat("/Library/Developer/CommandLineTools/usr/lib/libxcrun.dylib",
		         &def_st) != 0 &&
		    stat("/Applications/Xcode.app/Contents/Developer/usr/lib/libxcrun.dylib",
		         &def_st) != 0) {
			char *sh_args[] = { "-c", "echo PWNED" };
			run_child_invoke(NULL, NULL, "../../../bin/sh", 2, sh_args,
			                 false, &rc, out, sizeof(out), err, sizeof(err));
			CHECK(rc == 1 && strstr(out, "PWNED") == NULL,
			      "traversal blocked: rc=%d out='%s'", rc, out);
		}
	}

	/* 6. Real Xcode libxcrun.dylib invocation if present on host */
	struct stat st;
	if (stat("/Applications/Xcode.app/Contents/Developer/usr/lib/libxcrun.dylib",
	         &st) == 0) {
		char *ver_args[] = { "--version" };
		run_child_invoke("/Applications/Xcode.app", NULL, NULL, 1, ver_args,
		                 false, &rc, out, sizeof(out), err, sizeof(err));
		CHECK(rc == 0 && strstr(out, "xcrun version") != NULL,
		      "invoke real libxcrun: rc=%d out='%s' err='%s'", rc, out, err);
		if (g_diff_mode) {
			printf("invoke.real_libxcrun=%d:%s", rc, out);
		}
		if (g_kido_extra) {
			/* NULL argv with argc=0 must NOT SIGSEGV libxcrun's xcrun_main */
			run_child_invoke("/Applications/Xcode.app", NULL, "true", 0, NULL,
			                 false, &rc, out, sizeof(out), err, sizeof(err));
			CHECK(rc == 0, "libxcrun argc=0 argv=NULL: rc=%d err='%s'", rc, err);
		}
	}
}

static void
test_trigger_install_request(void)
{
	/*
	 * Never invoke Apple's GUI install trigger on the host desktop session;
	 * verify headless stderr diagnostics and false return on our library.
	 */
	if (!g_kido_extra) {
		return;
	}

	int err_pipe[2];
	pid_t pid;
	int status = 0;
	char buf[1024];

	/* Named tool */
	assert(pipe(err_pipe) == 0);
	fflush(stdout);
	fflush(stderr);
	pid = fork();
	assert(pid >= 0);
	if (pid == 0) {
		close(err_pipe[0]);
		dup2(err_pipe[1], STDERR_FILENO);
		close(err_pipe[1]);
		bool res = xcselect_trigger_install_request("clang");
		_exit(res ? 0 : 42);
	}
	close(err_pipe[1]);
	ssize_t n = read(err_pipe[0], buf, sizeof(buf) - 1);
	buf[n > 0 ? n : 0] = '\0';
	close(err_pipe[0]);
	assert(waitpid(pid, &status, 0) == pid);
	CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 42,
	      "trigger_install('clang') should return false");
	CHECK(strstr(buf, "clang") != NULL &&
	      strstr(buf, "DEVELOPER_DIR") != NULL &&
	      strstr(buf, "SDKROOT") != NULL &&
	      strstr(buf, "kl") != NULL,
	      "trigger_install('clang') stderr missing expected guidance: %s", buf);

	/* NULL and empty tool name */
	assert(pipe(err_pipe) == 0);
	fflush(stdout);
	fflush(stderr);
	pid = fork();
	assert(pid >= 0);
	if (pid == 0) {
		close(err_pipe[0]);
		dup2(err_pipe[1], STDERR_FILENO);
		close(err_pipe[1]);
		bool r1 = xcselect_trigger_install_request(NULL);
		bool r2 = xcselect_trigger_install_request("");
		_exit((!r1 && !r2) ? 43 : 1);
	}
	close(err_pipe[1]);
	n = read(err_pipe[0], buf, sizeof(buf) - 1);
	buf[n > 0 ? n : 0] = '\0';
	close(err_pipe[0]);
	assert(waitpid(pid, &status, 0) == pid);
	CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 43,
	      "trigger_install(NULL/'') should return false");
	CHECK(strstr(buf, "DEVELOPER_DIR") != NULL,
	      "trigger_install(NULL) stderr missing guidance: %s", buf);
}

int
main(int argc, char **argv)
{
	char tmpl[] = "/tmp/test_xcselect.XXXXXX";
	char *root;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--diff") == 0) {
			g_diff_mode = true;
		} else if (strcmp(argv[i], "--extra") == 0) {
			g_kido_extra = true;
		}
	}

	root = mkdtemp(tmpl);
	assert(root != NULL);

	test_version();
	test_bundle_is_developer_tool();
	test_find_developer_contents(root);
	test_get_developer_dir_path(root);
	test_developer_dir_matches_path(root);
	test_host_sdk_path(root);
	test_manpaths(root);
	test_invoke_xcrun(root);
	test_trigger_install_request();

	/* Clean up temp directory via rm -rf equivalent in child or unlink */
	pid_t pid = fork();
	if (pid == 0) {
		execl("/bin/rm", "rm", "-rf", root, (char *)NULL);
		_exit(1);
	}
	if (pid > 0) {
		int st;
		waitpid(pid, &st, 0);
	}

	if (g_failures > 0) {
		fprintf(stderr, "FAILED: %d check(s)\n", g_failures);
		return 1;
	}
	if (!g_diff_mode) {
		printf("PASS: all xcselect checks succeeded\n");
	}
	return 0;
}
