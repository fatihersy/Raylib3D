#ifndef LOGGER_H
#define LOGGER_H

typedef enum logging_severity {
	LOG_SEV_UNDEFINED,
	LOG_SEV_TRACE,
	LOG_SEV_DEBUG,
	LOG_SEV_INFO,
	LOG_SEV_WARNING,
	LOG_SEV_ERROR,
	LOG_SEV_FATAL,
	LOG_SEV_MAX,
} logging_severity;

void inc_logging(logging_severity ls, const char* fmt, ...);

#define ITRACE(fmt, ...) do { inc_logging(LOG_SEV_TRACE, 		fmt __VA_OPT__(,) __VA_ARGS__); } while(0)
#define IDEBUG(fmt, ...) do { inc_logging(LOG_SEV_DEBUG, 		fmt __VA_OPT__(,) __VA_ARGS__); } while(0)
#define IINFO(fmt, ...) 	do { inc_logging(LOG_SEV_INFO, 			fmt __VA_OPT__(,) __VA_ARGS__); } while(0)
#define IWARN(fmt, ...) 	do { inc_logging(LOG_SEV_WARNING,	fmt __VA_OPT__(,) __VA_ARGS__); } while(0)
#define IERROR(fmt, ...) do { inc_logging(LOG_SEV_ERROR,			fmt __VA_OPT__(,) __VA_ARGS__); } while(0)
#define IFATAL(fmt, ...) do { inc_logging(LOG_SEV_FATAL,			fmt __VA_OPT__(,) __VA_ARGS__); } while(0)

[[__nodiscard__]] bool logging_system_initialize(void);
void logging_system_shutdown(void);

#endif