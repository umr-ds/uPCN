/*
 * hal_defines.h
 *
 * Description: contains platform-specific defines
 *
 */

#ifndef HAL_DEFINES_H_INCLUDED
#define HAL_DEFINES_H_INCLUDED

#include <stdint.h>
#include "hal_debug.h"
#include "upcn/buildFlags.h"

/* ASSERT */

#ifdef INCLUDE_ASSERT

#if !defined(UPCN_LOCAL) && !defined(UPCN_POSIX_TEST_BUILD)

#include <hal_platform.h>

#define ASSERT(value) do { \
	if (!(value)) { \
		hal_platform_led_set(5); \
		for (int i = 0; i < 1000000; i++) \
			; \
		hal_platform_restart_upcn(); \
		for (;;) \
			; \
	} \
} while (0)

#endif /* UPCN_LOCAL */

#else /* INCLUDE_ASSERT */

#define ASSERT(value) ((void)(value))

#endif /* INCLUDE_ASSERT */

/* Memory debugging */

#ifdef MEMDEBUG

#define malloc(size) upcn_dbg_malloc(size, __FILE__, __LINE__)
#define calloc(num, size) upcn_dbg_calloc(num, size, __FILE__, __LINE__)
#define realloc(ptr, size) upcn_dbg_realloc(ptr, size, __FILE__, __LINE__)
#define free(ptr) do { upcn_dbg_free(ptr, __FILE__, __LINE__); \
	ptr = NULL; } while (0)

void *upcn_dbg_malloc(size_t size, char *file, int line);
void *upcn_dbg_calloc(size_t num, size_t size, char *file, int line);
void *upcn_dbg_realloc(void *ptr, size_t size, char *file, int line);
void upcn_dbg_free(void *ptr, char *file, int line);
void upcn_dbg_memprint(void);

void upcn_dbg_mem_lock(void *ptr);
void upcn_dbg_mem_unlock(void *ptr);

uint32_t upcn_dbg_memstat_get_cur(void);
uint32_t upcn_dbg_memstat_get_max(void);
void upcn_dbg_memstat_reset(void);
void upcn_dbg_memstat_print(void);

#else /* MEMDEBUG */

#define upcn_dbg_memprint(a) ((void)0)

#define upcn_dbg_mem_lock(a) ((void)0)
#define upcn_dbg_mem_unlock(a) ((void)0)

#define upcn_dbg_memstat_get_cur() (0)
#define upcn_dbg_memstat_get_max() (0)
#define upcn_dbg_memstat_reset() ((void)0)
#define upcn_dbg_memstat_print() ((void)0)

#endif /* MEMDEBUG */

/* Logging */

#define LOG_NO_ITEM 0xFFFFFFFF
#define LOGI(message, itemid) LOGA(message, 0xFF, itemid)
#define LOG(message) LOGI(message, LOG_NO_ITEM)

#define LOGF(f_, ...) hal_debug_printf_log(__FILE__, \
	(int)(__LINE__), \
	(f_), \
	__VA_ARGS__)

#ifdef LOGGING

#define LOGA(message, actionid, itemid) \
	upcn_dbg_log(message, actionid, (uint32_t)itemid, __FILE__, __LINE__)

void upcn_dbg_log(char *msg, uint8_t actid, uint32_t itemid,
	char *file, int line);
void upcn_dbg_printlogs(void);
void upcn_dbg_clearlogs(void);
void upcn_dbg_log_disable_output(void);
void upcn_dbg_log_enable_output(void);

#else /* LOGGING */

#ifdef LOG_PRINT


#include <hal_debug.h>

extern uint8_t _upcn_dbg_log_disabled;

#define LOGA(message, actionid, itemid) do { \
if (_upcn_dbg_log_disabled) \
	break; \
uint32_t ii = (intptr_t)(itemid); \
hal_debug_printf( \
	"%d: %s (%d) [%s:%d]\n", \
	(int)(hal_time_get_timestamp_ms()), \
	message, \
	((uint8_t)(actionid) == 0xFF) \
		? (ii == LOG_NO_ITEM ? -1 : (intptr_t)itemid) \
		: (int)(actionid), \
	__FILE__, \
	(int)(__LINE__) \
); \
} while (0)

void upcn_dbg_log_disable_output(void);
void upcn_dbg_log_enable_output(void);

#else /* LOG_PRINT */

#define LOGA(message, actionid, itemid) ((void)(itemid))

#define upcn_dbg_log_disable_output() ((void)0)
#define upcn_dbg_log_enable_output() ((void)0)

#endif /* LOG_PRINT */

#define upcn_dbg_printlogs() ((void)0)
#define upcn_dbg_clearlogs() ((void)0)

#endif /* LOGGING */

#ifdef DEBUG_FREERTOS_TRACE

void upcn_dbg_printtrace(void);
void upcn_dbg_resettrace(void);
void upcn_dbg_storetrace(void);
void upcn_dbg_cleartraces(void);
void upcn_dbg_printtraces(void);

#else /* DEBUG_FREERTOS_TRACE */

#define upcn_dbg_printtrace() ((void)0)
#define upcn_dbg_resettrace() ((void)0)
#define upcn_dbg_storetrace() ((void)0)
#define upcn_dbg_cleartraces() ((void)0)
#define upcn_dbg_printtraces() ((void)0)

#endif /* DEBUG_FREERTOS_TRACE */


#endif /* HAL_DEFINES_H_INCLUDED */
