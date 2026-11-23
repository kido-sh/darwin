/*
 * PureDarwin recreation of `logd` - just enough to not shit the bed
*/
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef _PATH_LOG
#define _PATH_LOG "/var/run/syslog"
#endif

#define PD_LOGD_LOGFILE "/var/log/system.log"
#define PD_LOGD_OLDFILE "/var/log/system.log.1"
#define PD_LOGD_MAXSIZE (2 * 1024 * 1024)

static int logfile_fd = -1;
static off_t logfile_size;

static void
logd_prepare_path(const char *path)
{
	char tmp[256];
	char *slash;

	if (strlen(path) >= sizeof(tmp)) {
		return;
	}

	strcpy(tmp, path);
	slash = strrchr(tmp, '/');
	if (slash == NULL || slash == tmp) {
		return;
	}
	*slash = '\0';
	(void)mkdir(tmp, 0755);
}

static void
logd_open_logfile(void)
{
	struct stat st;

	logfile_fd = open(PD_LOGD_LOGFILE, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
	logfile_size = logfile_fd >= 0 && fstat(logfile_fd, &st) == 0 ? st.st_size : 0;
}

/* one old file is kept, so the log never takes more than twice PD_LOGD_MAXSIZE */
static void
logd_write(const char *msg, size_t len)
{
	if (len == 0) {
		return;
	}
	if (logfile_fd >= 0 && logfile_size + (off_t)len + 1 > PD_LOGD_MAXSIZE) {
		(void)close(logfile_fd);
		(void)rename(PD_LOGD_LOGFILE, PD_LOGD_OLDFILE);
		logd_open_logfile();
	}
	if (logfile_fd < 0) {
		return;
	}
	if (write(logfile_fd, msg, len) == (ssize_t)len) {
		logfile_size += (off_t)len;
	}
	if (msg[len - 1] != '\n' && write(logfile_fd, "\n", 1) == 1) {
		logfile_size += 1;
	}
}

static int
logd_bind_syslog_socket(void)
{
	struct sockaddr_un sun;
	int fd, rcvbuf = 256 * 1024;

	logd_prepare_path(_PATH_LOG);
	(void)unlink(_PATH_LOG);

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("logd: socket");
		return -1;
	}
	(void)setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, _PATH_LOG, sizeof(sun.sun_path) - 1);

	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
		perror("logd: bind " _PATH_LOG);
		(void)close(fd);
		return -1;
	}

	(void)chmod(_PATH_LOG, 0666);
	return fd;
}

int
main(void)
{
	char buf[8192];
	int syslog_fd;

	(void)signal(SIGPIPE, SIG_IGN);

	logd_prepare_path(PD_LOGD_LOGFILE);
	logd_open_logfile();

	syslog_fd = logd_bind_syslog_socket();
	if (syslog_fd < 0) {
		return 1;
	}

	for (;;) {
		ssize_t len = recv(syslog_fd, buf, sizeof(buf) - 1, 0);
		char *msg = buf;

		if (len < 0) {
			if (errno != EINTR) {
				sleep(1);
			}
			continue;
		}
		buf[len] = '\0';

		/* the syslog priority, "<13>" */
		if (buf[0] == '<') {
			char *end = buf + 1;

			while (*end >= '0' && *end <= '9') {
				end++;
			}
			if (*end == '>' && end > buf + 1) {
				msg = end + 1;
			}
		}
		logd_write(msg, (size_t)(buf + len - msg));
	}
}
