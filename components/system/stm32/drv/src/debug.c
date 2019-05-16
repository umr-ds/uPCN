/* Attention: This file the single one that MUST NOT include upcn.h */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "upcn/buildFlags.h"

#include <hal_io.h>
#include <hal_time.h>
#include <hal_platform.h>
#include <hal_task.h>

#define MEMDEBUG_NOFILETRACK

#ifdef MEMDEBUG
#define W_P_NUM
#endif /* MEMDEBUG */

#ifdef DEBUG_FREERTOS_TRACE
#define W_P_NUM
#endif /* DEBUG_FREERTOS_TRACE */

#ifdef W_P_NUM

static char wbuf[16];

static void write_padded_num(uint32_t num, uint8_t digits)
{
	uint8_t len, i;

	hal_platform_sprintu32(wbuf, num);
	len = strlen(wbuf);
	if (digits > len) {
		for (i = digits; i > len; i--)
			hal_io_write_raw(" ", 1);
	}
	hal_io_write_raw(wbuf, len);
}

#endif /* W_P_NUM */

#ifdef INCLUDE_FILETRACK

/* 256 B */
#define MAX_FILES 64

static char *files[MAX_FILES];
static uint8_t findex;

#endif /* INCLUDE_FILETRACK */

#ifdef MEMDEBUG

void hal_platform_led_set(int led_preset);

#define END_MARKER "XX"

static uint8_t madd(void *ptr, char *file, int line, uint32_t size);
static int mremove(void *ptr);

static void *upcn_dbg_memfail(
	char *msg, void *ptr, char *file, int line, int ret)
{
	hal_platform_led_set(5);
	if (ret) {
		taskEXIT_CRITICAL();
#ifndef QUIET
		hal_io_write_string(msg);
		if (ptr != NULL) {
			hal_io_write_string(": 0x");
			hal_platform_sprintu32x(wbuf, (uint32_t)ptr);
			hal_io_write_string(wbuf);
		}
		hal_io_write_string(" @ ");
		hal_io_write_string(file);
		hal_io_write_string(":");
		hal_platform_sprintu32(wbuf, (uint32_t)line);
		hal_io_write_string(wbuf);
		hal_io_write_string("\n");
#endif /* QUIET */
	} else {
		int i;

		hal_task_suspend_scheduler();
		taskEXIT_CRITICAL();
		for (i = 0; i < INT32_MAX; i++) /* Memdebug FAIL */
			;
		hal_platform_hard_restart_upcn();
	}
	return NULL;
}

/* MALLOC */

#define MADDR(ptr) (&((uint32_t *)ptr)[-1])
#define MSIZE(ptr) ((((uint32_t *)ptr)[-1]) & 0x00FFFFFF)
#define MFILE(ptr) ((((uint32_t *)ptr)[-1]) >> 24)

void *upcn_dbg_malloc(size_t size, char *file, int line)
{
	void *m;
	int ret;
	size_t strsz;

	taskENTER_CRITICAL();
	strsz = strlen(END_MARKER) + 1;
	m = malloc(sizeof(uint32_t) + size + strsz);
	if (m == NULL) {
#ifdef MEMDEBUG_STRICT
		ret = 0;
#else
		ret = 1;
#endif
		return upcn_dbg_memfail("malloc(): memory allocation failed",
			NULL, file, line, ret);
	}
	((uint32_t *)m)[0] = (uint32_t)size & 0x00FFFFFF;
	m = &((uint32_t *)m)[1];
	memcpy(m + size, END_MARKER, strsz);
	if (!madd(m, file, line, size)) {
		free(MADDR(m));
		m = NULL;
#ifdef MEMDEBUG_STRICT
		ret = 0;
#else
		ret = 1;
#endif
		return upcn_dbg_memfail("malloc(): memory allocation failed",
			NULL, file, line, ret);
	}
	taskEXIT_CRITICAL();
	return m;
}

void *upcn_dbg_calloc(size_t num, size_t size, char *file, int line)
{
	void *ret = upcn_dbg_malloc(num * size, file, line);

	memset(ret, 0, num * size);
	return ret;
}

/* FREE + CHECKING */

static int buffer_overflow(void *ptr)
{
	return strcmp(ptr + MSIZE(ptr), END_MARKER) != 0;
}

void upcn_dbg_free(void *ptr, char *file, int line)
{
	if (ptr == NULL)
		return;
	taskENTER_CRITICAL();
	if (!mremove(ptr))
		upcn_dbg_memfail("free(): memory was not allocated",
			ptr, file, line, 0);
	if (buffer_overflow(ptr))
		upcn_dbg_memfail("free(): buffer overflow detected",
			ptr, file, line, 0);
	free(MADDR(ptr));
	taskEXIT_CRITICAL();
}

void *upcn_dbg_realloc(void *ptr, size_t size, char *file, int line)
{
	size_t strsz;
	void *newmem;
	int ret;

	if (ptr == NULL)
		return upcn_dbg_malloc(size, file, line);
	if (size == 0) {
		upcn_dbg_free(ptr, file, line);
		return NULL;
	}
	taskENTER_CRITICAL();
	if (!mremove(ptr))
		return upcn_dbg_memfail("realloc(): memory was not allocated",
			ptr, file, line, 0);
	if (buffer_overflow(ptr))
		return upcn_dbg_memfail("realloc(): buffer overflow detected",
			ptr, file, line, 0);
	strsz = strlen(END_MARKER) + 1;
	newmem = realloc(MADDR(ptr), sizeof(size_t) + 1 + size + strsz);
	if (newmem == NULL) {
#ifdef MEMDEBUG_STRICT
		ret = 0;
#else
		ret = 1;
#endif
		return upcn_dbg_memfail("realloc(): memory allocation failed",
			ptr, file, line, ret);
	}
	((uint32_t *)newmem)[0] = (uint32_t)size & 0x00FFFFFF;
	newmem = &((uint32_t *)newmem)[1];
	memcpy(newmem + size, END_MARKER, strsz);
	if (!madd(newmem, file, line, size)) {
		free(MADDR(newmem));
		newmem = NULL;
#ifdef MEMDEBUG_STRICT
		ret = 0;
#else
		ret = 1;
#endif
		return upcn_dbg_memfail("realloc(): memory allocation failed",
			ptr, file, line, ret);
	}
	taskEXIT_CRITICAL();
	return newmem;
}

void upcn_dbg_mem_lock(void *ptr)
{
	((char *)ptr)[MSIZE(ptr)] = 'L';
}

void upcn_dbg_mem_unlock(void *ptr)
{
	((char *)ptr)[MSIZE(ptr)] = (END_MARKER)[0];
}


/* TRACKING */

uint8_t upcn_dbg_fileindex(char *file);
char *upcn_dbg_fileget(uint8_t index);

#ifndef MEMDEBUG_NOFILETRACK
static struct memlist {
	void *ptr;
	uint8_t file;
	uint16_t line;
	struct memlist *next;
} *mlist;
#endif

static uint32_t cur_mem[MAX_FILES];
static uint32_t max_mem[MAX_FILES];
static uint32_t cur_mem_total;
static uint32_t max_mem_total;
static uint32_t cur_mallocs[MAX_FILES];
static uint32_t mallocs[MAX_FILES];
static uint32_t cur_mallocs_total;
static uint32_t mallocs_total;

static uint8_t madd(void *ptr, char *filename, int line, uint32_t size)
{
	uint8_t file;
#ifndef MEMDEBUG_NOFILETRACK
	struct memlist *item = malloc(sizeof(struct memlist));

	if (item == NULL)
		return 0;
#endif /* MEMDEBUG_NOFILETRACK */
	file = upcn_dbg_fileindex(filename);
	((uint32_t *)ptr)[-1] |= (uint32_t)file << 24;
	cur_mem[file] += size;
	cur_mem_total += size;
	if (max_mem[file] < cur_mem[file])
		max_mem[file] = cur_mem[file];
	if (max_mem_total < cur_mem_total)
		max_mem_total = cur_mem_total;
	cur_mallocs[file]++;
	mallocs[file]++;
	cur_mallocs_total++;
	mallocs_total++;
#ifndef MEMDEBUG_NOFILETRACK
	item->ptr = ptr;
	item->file = file;
	item->line = (uint16_t)line;
	item->next = mlist;
	mlist = item;
#endif /* MEMDEBUG_NOFILETRACK */
	return 1;
}

static int mremove(void *ptr)
{
	uint8_t file = MFILE(ptr);
	uint32_t size = MSIZE(ptr);
#ifndef MEMDEBUG_NOFILETRACK
	struct memlist **itemp = &mlist, *next;
#endif /* MEMDEBUG_NOFILETRACK */

	cur_mem[file] -= size;
	cur_mem_total -= size;
	cur_mallocs[file]--;
	cur_mallocs_total--;
#ifndef MEMDEBUG_NOFILETRACK
	while (*itemp != NULL) {
		if ((*itemp)->ptr == ptr) {
			next = (*itemp)->next;
			free(*itemp);
			*itemp = next;
			return 1;
		}
		itemp = &(*itemp)->next;
	}
	return 0;
#else /* MEMDEBUG_NOFILETRACK */
	return 1;
#endif /* MEMDEBUG_NOFILETRACK */
}

uint32_t upcn_dbg_memstat_get_cur(void)
{
	return cur_mem_total;
}

uint32_t upcn_dbg_memstat_get_max(void)
{
	return max_mem_total;
}

void upcn_dbg_memstat_reset(void)
{
	int i;

	max_mem_total = cur_mem_total;
	mallocs_total = cur_mallocs_total;
	for (i = 0; i < findex; i++) {
		max_mem[i] = cur_mem[i];
		mallocs[i] = cur_mallocs[i];
	}
}

void upcn_dbg_memstat_print(void)
{
	int i;

	hal_io_lock_com_semaphore();
	hal_io_write_string("\nMALLOC STATS\n============\n\n");
	for (i = 0; i < findex; i++) {
		write_padded_num(cur_mallocs[i], 3);
		hal_io_write_string(" allocs w/ ");
		write_padded_num(cur_mem[i], 5);
		hal_io_write_string(" B (");
		write_padded_num(mallocs[i], 3);
		hal_io_write_string(" since reset, ");
		write_padded_num(max_mem[i], 5);
		hal_io_write_string(" B max) in ");
		hal_io_write_string(upcn_dbg_fileget(i));
		hal_io_write_string("\n");
	}
	hal_io_write_string("\nTotal: ");
	hal_platform_sprintu32(wbuf, cur_mem_total);
	hal_io_write_string(wbuf);
	hal_io_write_string(" B in ");
	hal_platform_sprintu32(wbuf, cur_mallocs_total);
	hal_io_write_string(wbuf);
	hal_io_write_string(" allocs (");
	hal_platform_sprintu32(wbuf, mallocs_total);
	hal_io_write_string(wbuf);
	hal_io_write_string(" since reset, ");
	hal_platform_sprintu32(wbuf, max_mem_total);
	hal_io_write_string(wbuf);
	hal_io_write_string(" B max)\n\n");
	hal_io_unlock_com_semaphore();
}

void upcn_dbg_memprint(void)
{
#ifndef MEMDEBUG_NOFILETRACK
	int16_t counter = 0;
	struct memlist *cur = mlist;

	hal_io_lock_com_semaphore();
	hal_io_write_string("\nMALLOC TRACING INFO\n===================\n\n");
	while (cur != NULL) {
		hal_io_write_string("0x");
		hal_platform_sprintu32x(wbuf, (uint32_t)cur->ptr);
		hal_io_write_string(wbuf);
		hal_io_write_string(": ");
		hal_platform_sprintu32(wbuf, MSIZE(cur->ptr));
		hal_io_write_string(wbuf);
		hal_io_write_string(" bytes, ");
		hal_io_write_string(upcn_dbg_fileget(cur->file));
		hal_io_write_string(":");
		hal_platform_sprintu32(wbuf, cur->line);
		hal_io_write_string(wbuf);
		hal_io_write_string("\n");
		cur = cur->next;
		counter++;
	}
	hal_io_write_string("\n");
	hal_platform_sprintu32(wbuf, counter);
	hal_io_write_string(wbuf);
	hal_io_write_string(" allocation(s)\n\n");
	hal_io_unlock_com_semaphore();
#endif /* MEMDEBUG_NOFILETRACK */
}

#endif /* MEMDEBUG */

#ifdef INCLUDE_FILETRACK

#define INVALID_FILE 0xFF
static char *invalid_file_string = "<invalid>";

uint8_t upcn_dbg_fileindex(char *file)
{
	uint8_t i;
	size_t len;

	if (findex == MAX_FILES)
		return INVALID_FILE;
	for (i = 0; i < findex; i++) {
		if (strcmp(files[i], file) == 0)
			return i;
	}
	len = strlen(file) + 1;
	files[findex] = malloc(len);
	strncpy(files[findex], file, len);
	return findex++;
}

char *upcn_dbg_fileget(uint8_t index)
{
	if (index == INVALID_FILE)
		return invalid_file_string;
	return files[index];
}

#endif /* INCLUDE_FILETRACK */

#ifdef DEBUG_FREERTOS_TRACE

#define MAX_TASK_TAGS 8
#define MAX_STORED_TRACES 20

static int cur_task;
static uint64_t last_ts;
static uint64_t task_rt[MAX_TASK_TAGS];

static char *task_names[MAX_TASK_TAGS] = {
	"IDLE",
	"INPT",
	"ROUT",
	"BPRC",
	"CMAN",
	"GRST",
	"ROPT",
	"FSTK"
};

static struct {
	uint32_t rt_ppm[MAX_TASK_TAGS];
} stored_traces[MAX_STORED_TRACES];
static uint16_t st_index;

void upcn_dbg_trace(int task_tag)
{
	uint64_t cur_ts;

	taskDISABLE_INTERRUPTS();
	cur_ts = hal_time_get_system_time(); /* us */
	task_rt[cur_task] += cur_ts - last_ts;
	last_ts = cur_ts;
	if (task_tag > 0 && task_tag < MAX_TASK_TAGS)
		cur_task = task_tag;
	else
		cur_task = 0;
	taskENABLE_INTERRUPTS();
}

void upcn_dbg_printtrace(void)
{
	uint16_t i;
	uint64_t total_rt = 0;

	for (i = 0; i < MAX_TASK_TAGS; i++)
		total_rt += task_rt[i];
	hal_io_lock_com_semaphore();
	hal_io_write_string("\nCPU TRACING INFO\n================\n\n");
	for (i = 0; i < MAX_TASK_TAGS; i++) {
		hal_io_write_string(task_names[i]);
		hal_io_write_string(": ");
		write_padded_num(task_rt[i] * 1000000 / total_rt, 6);
		hal_io_write_string(" ppm = ");
		hal_platform_sprintu64(wbuf, task_rt[i]);
		hal_io_write_string(wbuf);
		hal_io_write_string(" us\n");
	}
	hal_io_write_string("\nTotal: ");
	hal_platform_sprintu64(wbuf, total_rt);
	hal_io_write_string(wbuf);
	hal_io_write_string(" us\n\n");
	hal_io_unlock_com_semaphore();
	/* Send CPU stats as PERFDATA */
	hal_io_send_packet(task_rt, sizeof(task_rt[0]) * MAX_TASK_TAGS,
		COMM_TYPE_PERF_DATA);
}

void upcn_dbg_resettrace(void)
{
	uint16_t i;

	taskDISABLE_INTERRUPTS();
	for (i = 0; i < MAX_TASK_TAGS; i++)
		task_rt[i] = 0;
	last_ts = hal_time_get_system_time();
	taskENABLE_INTERRUPTS();
}

void upcn_dbg_storetrace(void)
{
	uint16_t i;
	uint64_t total_rt = 0;

	if (st_index == MAX_STORED_TRACES)
		return;

	taskDISABLE_INTERRUPTS();
	for (i = 0; i < MAX_TASK_TAGS; i++)
		total_rt += task_rt[i];
	for (i = 0; i < MAX_TASK_TAGS; i++) {
		stored_traces[st_index].rt_ppm[i]
			= (uint32_t)(task_rt[i] * 1000000ULL / total_rt);
	}
	st_index++;
	taskENABLE_INTERRUPTS();
}

void upcn_dbg_cleartraces(void)
{
	st_index = 0;
}

void upcn_dbg_printtraces(void)
{
	uint16_t i, t, length = st_index;
	int32_t mean, var, cur;

	if (length <= 1)
		return;
	hal_io_lock_com_semaphore();
	hal_io_write_string("\nCPU TRACING STATS\n=================\n\n");
	for (t = 0; t < MAX_TASK_TAGS; t++) {
		mean = 0;
		for (i = 0; i < length; i++)
			mean += (int32_t)stored_traces[i].rt_ppm[t];
		mean /= length;
		var = 0;
		for (i = 0; i < length; i++) {
			cur = ((int32_t)stored_traces[i].rt_ppm[t] - mean);
			var += cur * cur;
		}
		var /= length;
		hal_io_write_string(task_names[t]);
		hal_io_write_string(": MEAN = ");
		write_padded_num(mean, 6);
		hal_io_write_string(" ppm, VARIANCE = ");
		write_padded_num(var, 8);
		hal_io_write_string("\n");
	}
	hal_io_write_string("\nTotal: ");
	hal_platform_sprintu32(wbuf, st_index);
	hal_io_write_string(wbuf);
	hal_io_write_string(" values\n\n");
	hal_io_unlock_com_semaphore();
}

#endif /* DEBUG_FREERTOS_TRACE */
