#ifndef FILE_LOG_H
#define FILE_LOG_H
#include <syslog.h>

#define BLOOM_LOG_ID "bloomd"

static enum log_level_t {
	DEBUG,
	INFO,
	NOTICE,
	WARNING,
	ERROR,
	CRIT,
	ALERT
} LOG_LEVEL = NOTICE;

void
log_message(char *message)
{
	int flags;
	const char *tag;
	switch (LOG_LEVEL) {
	case DEBUG:
		flags = LOG_MAKEPRI(LOG_USER, LOG_DEBUG);
		tag = "DEBUG";
		break;
	case INFO:
		flags = LOG_MAKEPRI(LOG_USER, LOG_INFO);
		tag = "INFO";
		break;
	case NOTICE:
		flags = LOG_MAKEPRI(LOG_USER, LOG_NOTICE);
		tag = "NOTICE";
		break;
	case WARNING:
		flags = LOG_MAKEPRI(LOG_USER, LOG_WARNING);
		tag = "WARNING";
		break;
	case ERROR:
		flags = LOG_MAKEPRI(LOG_USER, LOG_ERR);
		tag = "ERROR";
		break;
	case CRIT:
		flags = LOG_MAKEPRI(LOG_USER, LOG_CRIT);
		tag = "CRITICAL";
		break;
	case ALERT:
		flags = LOG_MAKEPRI(LOG_USER, LOG_ALERT);
		tag = "ALERT";
		break;
	default:
		flags = LOG_MAKEPRI(LOG_USER, LOG_EMERG);
		tag = "EMERGENCY";
	}
	#ifndef NDEBUG
	openlog(BLOOM_LOG_ID, LOG_PERROR | LOG_PID, LOG_DAEMON);
	#else
	openlog(BLOOM_LOG_ID, LOG_CONS, LOG_DAEMON);
	#endif
	syslog(flags, "%s: %s", tag, message);
	closelog();
}

#endif /* FILE_LOG_H */
