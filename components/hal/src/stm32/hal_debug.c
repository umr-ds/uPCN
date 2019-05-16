/*
 * hal_debug.c
 *
 * Description: contains the stm32-implementation of the hardware
 * abstraction layer interface for debugging output functionality
 *
 */

#include <FreeRTOS.h>
#include <stdio.h>
#include <stdlib.h>
#include "hal_debug.h"
#include "hal_io.h"
#include "hal_semaphore.h"
#include "hal_time.h"
#include "drv/mini-printf.h"


#define DEFAULT_PRINTF_BUFFER_SIZE 192
static char printf_buffer[DEFAULT_PRINTF_BUFFER_SIZE];
static Semaphore_t printf_semaphore;
static Semaphore_t printf_log_semaphore;


static char printf_log_buffer[DEFAULT_PRINTF_BUFFER_SIZE];



int hal_debug_init(void)
{
	printf_semaphore = hal_semaphore_init_binary();
	if (printf_semaphore == NULL)
		return EXIT_FAILURE;
	hal_semaphore_release(printf_semaphore);

	printf_log_semaphore = hal_semaphore_init_binary();
	if (printf_log_semaphore == NULL)
		return EXIT_FAILURE;
	hal_semaphore_release(printf_log_semaphore);

	return EXIT_SUCCESS;
}

void hal_debug_write_string(const char *string)
{
	hal_io_write_string(string);
}


int hal_debug_printf(const char *format, ...)
{
	va_list args;
	int length;

	va_start(args, format);
	length = hal_debug_vprintf(format, args);
	va_end(args);
	return length;
}

int hal_debug_vprintf(const char *format, va_list args)
{
	int length;

	hal_semaphore_take_blocking(printf_semaphore);
	length = mini_vsnprintf(
		printf_buffer, DEFAULT_PRINTF_BUFFER_SIZE, format, args);
	/* If the string to be printed is longer, we should allocate memory */
	/* and do the printing again... [ TODO: removed! ] */
	if (length >= DEFAULT_PRINTF_BUFFER_SIZE - 1) {
		printf_buffer[length - 2] = '~';
		printf_buffer[length - 1] = '\n';
	}
	if (length >= 0)
		hal_io_write_raw(printf_buffer, length);
	hal_semaphore_release(printf_semaphore);
	return length;
}

int hal_debug_printf_log(const char *file,
			 const int line,
			 const char *format, ...)
{

	va_list args;
	int length;

	hal_semaphore_take_blocking(printf_log_semaphore);
	va_start(args, format);
	length = mini_vsnprintf(
		printf_log_buffer, DEFAULT_PRINTF_BUFFER_SIZE, format, args);
	va_end(args);
	/* If the string to be printed is longer, we should allocate memory */
	/* and do the printing again... [ TODO: removed! ] */
	if (length >= DEFAULT_PRINTF_BUFFER_SIZE - 1) {
		printf_log_buffer[length - 2] = '~';
		printf_log_buffer[length - 1] = '\n';
	}
	if (length >= 0)
		hal_debug_printf("[%s]: %s [%s:%d]\n", \
				 (hal_time_get_log_time_string()), \
				 printf_log_buffer, \
				 file, \
				 line);
	hal_semaphore_release(printf_log_semaphore);

	return length;
}

