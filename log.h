#ifndef __LOG__
#define __LOG__

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

/*
* Header file for logging-error reporting
*/

enum log_level { LOG, ERROR };
void log_print(enum log_level level, const char *logmsg, ...)
{
	va_list args;
	va_start(args, logmsg);

	pid_t curr_pid = getpid();

	char t[20];
	time_t st = time(0);
	struct tm *stm = gmtime(&st);
	strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S", stm);

	switch (level) {
	case LOG:
		fprintf(stderr, "%s PROCESS %d LOG: ", t, curr_pid);
		vfprintf(stderr, logmsg, args);
		fprintf(stderr, "\n");
		break;
	case ERROR:
		fprintf(stderr, "%s PROCESS %d ERROR: ", t, curr_pid);
		vfprintf(stderr, logmsg, args);
		if (errno)
			fprintf(stderr, "; %s", strerror(errno));
		fprintf(stderr, "\n");
		_exit(-1);
	}
}

#endif // !__LOG__
