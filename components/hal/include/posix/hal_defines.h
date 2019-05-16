/*
 * hal_defines.h
 *
 * Description: contains platform-specific defines
 *
 */

#ifndef HAL_DEFINES_H_INCLUDED
#define HAL_DEFINES_H_INCLUDED

#define LOG_NO_ITEM 0xFFFFFFFF

#define LOGI(message, itemid) LOGA(message, 0xFF, itemid)
#define LOG(message) LOGI(message, LOG_NO_ITEM)

#include <hal_debug.h>
#include <hal_time.h>

#define LOGA(message, actionid, itemid) do { \
intptr_t ii = (intptr_t)(itemid); \
hal_debug_printf( \
	"[%s]: %s (%d) [%s:%d]\n", \
	(hal_time_get_log_time_string()), \
	message, \
	((uint8_t)(actionid) == 0xFF) \
		? (ii == (intptr_t)LOG_NO_ITEM ? -1 : (intptr_t)itemid) \
		: (intptr_t)(actionid), \
	__FILE__, \
	(int)(__LINE__) \
); \
} while (0)

#define LOGF(f_, ...) hal_debug_printf_log(__FILE__, \
	(int)(__LINE__), \
	(f_), \
	__VA_ARGS__)

/* #define LOGI(message, itemid) hal_debug_printf(message);
 * #define LOGA(message, actionid, itemid)
 *	(void)(message);(void)(actionid);(void)(itemid);
 * #define LOG(message) hal_debug_printf(message);
 */

#define upcn_dbg_printlogs() ((void)0)
#define upcn_dbg_clearlogs() ((void)0)

#define upcn_dbg_log_disable_output() ((void)0)
#define upcn_dbg_log_enable_output() ((void)0)

#define upcn_dbg_memprint(a) ((void)0)

#define upcn_dbg_mem_lock(a) ((void)0)
#define upcn_dbg_mem_unlock(a) ((void)0)

#define upcn_dbg_memstat_get_cur() (0)
#define upcn_dbg_memstat_get_max() (0)
#define upcn_dbg_memstat_reset() ((void)0)
#define upcn_dbg_memstat_print() ((void)0)

#define upcn_dbg_printtrace() \
	hal_io_send_packet("", 1, COMM_TYPE_PERF_DATA)
#define upcn_dbg_resettrace() ((void)0)
#define upcn_dbg_storetrace() ((void)0)
#define upcn_dbg_cleartraces() ((void)0)
#define upcn_dbg_printtraces() ((void)0)

/* ASSERT */

#ifdef INCLUDE_ASSERT

#if !defined(UPCN_LOCAL) && !defined(UPCN_POSIX_TEST_BUILD)

#include <hal_platform.h>

#include <signal.h>

#define ASSERT(value) do { \
	if (!(value)) { \
		LOG("ASSERTION FAILED!"); \
		abort(); \
		for (;;) \
			; \
	} \
} while (0)

#endif /* UPCN_LOCAL */

#else /* INCLUDE_ASSERT */

#define ASSERT(value) ((void)(value))

#endif /* INCLUDE_ASSERT */

#endif /* HAL_DEFINES_H_INCLUDED */
