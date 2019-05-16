/*
 * hal_debug.h
 *
 * Description: contains the definitions of the hardware abstraction
 * layer interface for debugging output functionality
 *
 */

#ifndef HAL_DEBUG_H_INCLUDED
#define HAL_DEBUG_H_INCLUDED

#include <stddef.h>
#include <stdarg.h>

#define MAX_LOG_ENTRY_SIZE 200


/**
 * @brief hal_debug_init Initialization of underlying OS/HW for debugging
 * @return EXIT_SUCCESS or EXIT_FAILURE (macros are resolved to convention of
 *         the underlying OS infrastructure
 */
int hal_debug_init(void);

/**
 * @brief hal_debug_write_line Write a string with arbitrary lenth to the
 *			       debug interface
 * @param string
 */
void hal_debug_write_string(const char *string);

/**
 * @brief hal_debug_printf Write a string with arbitrary lenth to the
 *			   debug interface, provides functionality as
 *			   libc-printf()
 * @param format Parameters as standard libc-printf()
 */
int hal_debug_printf(const char *format, ...);

/**
 * @brief hal_debug_printf_log Write a string with arbitrary lenth to the
 *			       debug interface, provides functionality as
 *			       libc-printf()
 *			       Additionally, format as upcn standard output
 * @param format Parameters as standard libc-printf()
 */
int hal_debug_printf_log(const char *file,
			 const int line,
			 const char *format, ...);

/**
 * @brief hal_debug_vprintf Write a string with arbitrary lenth to the
 *			   debug interface, provides functionality as
 *			   libc-vprintf()
 * @param format Parameters as standard libc-vprintf()
 */
int hal_debug_vprintf(const char *format, va_list args);

#endif /* HAL_DEBUG_H_INCLUDED */
