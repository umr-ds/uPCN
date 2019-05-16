/*
 * hal_debug.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for debugging output functionality
 *
 */

#include "hal_debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <hal_time.h>
#include <hal_config.h>
#include <hal_defines.h>
#include <hal_semaphore.h>

static char log_entry_buffer[MAX_LOG_ENTRY_SIZE];
static Semaphore_t printf_sem;

#if DEBUG_LOG_FILE || DEBUG_STD_OUT
	FILE *log_fp;
#endif

int hal_debug_init(void)
{
#if DEBUG_LOG_FILE
	log_fp = fopen(DEBUG_LOG_FILE_PATH, "a");
	/* disable buffering of the log messages */
	setbuf(log_fp, NULL);
#endif
#if DEBUG_STD_OUT
	log_fp = stdout;
#endif
	printf_sem = hal_semaphore_init_mutex();
	hal_semaphore_release(printf_sem);
	return RETURN_SUCCESS;
}

void hal_debug_write_string(const char *string)
{
	hal_debug_printf("%s", string);
}

int hal_debug_printf(const char *format, ...)
{
	int rc;
	va_list args;

	va_start(args, format);
	rc = hal_debug_vprintf(format, args);
	va_end(args);
	return rc;
}

int hal_debug_vprintf(const char *format, va_list args)
{
	int rc;

#if DEBUG_STD_OUT || DEBUG_LOG_FILE
	rc = vfprintf(log_fp, format, args);
#endif
	return rc;
}

int hal_debug_printf_log(const char *file,
			 const int line,
			 const char *format, ...)
{
#if DEBUG_STD_OUT || DEBUG_LOG_FILE
	va_list args;

	va_start(args, format);
	hal_semaphore_take_blocking(printf_sem);
	vsnprintf(log_entry_buffer, MAX_LOG_ENTRY_SIZE, format, args);
	va_end(args);

	hal_debug_printf("[%s]: %s [%s:%d]\n", \
			 (hal_time_get_log_time_string()), \
			 log_entry_buffer, \
			 file, \
			 line);

	log_entry_buffer[0] = '\0';
	hal_semaphore_release(printf_sem);
#endif
	return RETURN_SUCCESS;
}

