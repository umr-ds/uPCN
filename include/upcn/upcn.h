#ifndef UPCN_H_INCLUDED
#define UPCN_H_INCLUDED

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "upcn/config.h"
#include "upcn/buildFlags.h"
#include "upcn/result.h"

#ifdef INCLUDE_BOARD_LIB
#include <hal_platform.h>
#include <hal_io.h>
#include <hal_debug.h>
#include <hal_time.h>
#include <hal_random.h>
#include <hal_queue.h>
#include <hal_task.h>
#include <hal_semaphore.h>
#include <hal_config.h>
#include <hal_defines.h>  // eventually overwrites memory allocators
#endif /* INCLUDE_BOARD_LIB */


enum task_tags {
	IDLE_TASK_TAG = 0,
	CONTACT_RX_TASK_TAG,
	ROUTER_TASK_TAG,
	BUNDLE_PROCESSOR_TASK_TAG,
	CONTACT_MANAGER_TASK_TAG,
	CONTACT_TX_TASK_TAG,
	ROUTER_OPTIMIZER_TASK_TAG,
	FS_TASK_TAG,
	CONFIG_AGENT_TASK_TAG,
};


enum convergence_layer {
	CLA_UNKNOWN,
	CLA_UDP,
	CLA_AX25
	/* ... */
};

/* ASSERT (for local builds, for plattform-builds see hal_defines.h)*/

#ifdef INCLUDE_ASSERT

#if defined(UPCN_LOCAL) || defined(UPCN_POSIX_TEST_BUILD)

#include <stdio.h>
#include <signal.h>

#define ASSERT(value) do { \
	if (!(value)) { \
		printf("Assertion failed in %s on line %d\n", \
			__FILE__, __LINE__); \
		abort(); \
		for (;;) \
			; \
	} \
} while (0)

#endif /* UPCN_LOCAL */

#else /* INCLUDE_ASSERT */

#define ASSERT(value) ((void)(value))

#endif /* INCLUDE_ASSERT */

#ifdef UPCN_LOCAL

#define upcn_dbg_mem_lock(a) ((void)0)
#define upcn_dbg_mem_unlock(a) ((void)0)

#endif /* UPCN_LOCAL */


/* COMMON FUNCTIONS */

#define count_list_next_length(list, cnt) do { \
	if (list != NULL) \
		cnt = 1; \
	iterate_list_next(list) { ++cnt; } \
} while (0)

#define iterate_list_next(list) \
	for (; list; list = (list)->next)

#define list_element_free(list) do { \
	void *e = list->next; \
	free(list); \
	list = e; \
} while (0)

#define list_free(list) { \
while (list != NULL) { \
	list_element_free(list); \
} }

#ifdef MIN
#undef MIN
#endif

#ifdef MAX
#undef MAX
#endif

#define MIN(a, b) ({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	_b < _a ? _b : _a; \
})
#define MAX(a, b) ({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	_b > _a ? _b : _a; \
})

#define HAS_FLAG(value, flag) ((value & flag) != 0)

#if defined(__GNUC__) && (__GNUC__ >= 7) && !defined(__clang__)
#define fallthrough __attribute__ ((fallthrough))
#else
#define fallthrough
#endif


#ifdef UPCN_TEST_BUILD
void upcntest_print(void);
#endif /* UPCN_TEST_BUILD */

#endif /* UPCN_H_INCLUDED */
