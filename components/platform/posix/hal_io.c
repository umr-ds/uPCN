/*
 * hal_io.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for input/output functionality
 *
 */

#include "upcn/result.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

enum upcn_result hal_io_init(void)
{
	return UPCN_OK;
}

int hal_io_message_printf(const char *format, ...)
{
	int rc;
	va_list v;

	va_start(v, format);
	// A false positive error of clang-tidy follows...
	// NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
	rc = vprintf(format, v);
	va_end(v);
	return rc;
}
