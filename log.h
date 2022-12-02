/**
 * @file log.h
 * @author Burak Köroğlu (e2448637@ceng.metu.edu.tr)
 * @brief Log interface.
 * 
 */

#ifndef __LOG__
#define __LOG__

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

/**
 * @enum log_level
 * 
 * @brief This enumeration is used to determine the log level
 * 
 */
enum log_level { LOG, ERROR };

/**
 * @brief Prints the log level, timestamp, process id and given formatted message for a given log level.
 * 
 * @details For `LOG` log level, the function just prints the given formatted message.
 * For `ERROR` log level, the function print the given message and if the errno is set, prints the error message, stops the program.
 * 
 * @param level 
 * @param logmsg 
 * @param ... 
 */
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
