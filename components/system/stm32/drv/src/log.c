#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "upcn/upcn.h"

#if defined(LOGGING) || defined(LOG_PRINT)

uint8_t _upcn_dbg_log_disabled;

void upcn_dbg_log_disable_output(void)
{
	_upcn_dbg_log_disabled = 1;
}

void upcn_dbg_log_enable_output(void)
{
	_upcn_dbg_log_disabled = 0;
}

#endif /* defined(LOGGING) || defined(LOG_PRINT) */

#ifdef LOGGING

#define LOG_INVALID_CALL    0xFF
#define LOG_INVALID_ACTION  0xFF
#define LOG_INVALID_ITEM_ID 0xFFFFFFFF

/* See debug.c: FILEINDEX must be defined (default for DEBUG and LOGGING) */
uint8_t upcn_dbg_fileindex(char *file);
char *upcn_dbg_fileget(uint8_t index);

static uint8_t callindex(char *description, char *file, uint16_t line);

struct log_call {
	char *description;
	uint8_t file;
	uint16_t line;
};

static struct log_call call_list[LOG_MAX_CALLS];
static uint8_t call_count;

struct log_event {
	/* A log entry should occur at least every 65 seconds => e.g. beacon */
	uint16_t time_offset;
	uint8_t call;
	uint8_t action;
	uint32_t identifier;
};

static uint64_t log_begin_time;
static uint64_t log_last_time;
static struct log_event events[LOG_MAX_EVENTS];
static uint16_t event_count;

static void print_event(struct log_event *e, uint32_t time);

void upcn_dbg_log(char *msg, uint8_t actid, uint32_t itemid,
	char *file, int line)
{
	uint8_t call;
	uint16_t index;
	uint64_t cur_time;

#ifdef VERBOSE
	if (!_upcn_dbg_log_disabled)
		hal_io_lock_com_semaphore();
#endif
	taskENTER_CRITICAL();
	if (event_count == LOG_MAX_EVENTS) {
		taskEXIT_CRITICAL();
#ifdef VERBOSE
		if (!_upcn_dbg_log_disabled)
			hal_io_unlock_com_semaphore();
#endif
		return;
	}
	call = callindex(msg, file, line);
	if (call == LOG_INVALID_CALL) {
		taskEXIT_CRITICAL();
#ifdef VERBOSE
		if (!_upcn_dbg_log_disabled)
			hal_io_unlock_com_semaphore();
#endif
		return;
	}
	cur_time = hal_time_get_system_time() / 1000;
	index = event_count++;
	if (index == 0) {
		log_begin_time = cur_time;
		events[0].time_offset = 0;
	} else {
		events[index].time_offset = cur_time - log_last_time;
	}
	log_last_time = cur_time;
	events[index].call = call;
	events[index].action = actid;
	events[index].identifier = itemid;
	taskEXIT_CRITICAL();
#ifdef VERBOSE
	if (!_upcn_dbg_log_disabled) {
		print_event(&events[index], (uint32_t)cur_time);
		hal_io_unlock_com_semaphore();
	}
#endif
}

void upcn_dbg_clearlogs(void)
{
	uint8_t c;

	taskENTER_CRITICAL();
	for (c = 0; c < call_count; c++)
		free(call_list[c].description);
	call_count = 0;
	event_count = 0;
	log_begin_time = hal_time_get_system_time() / 1000;
	log_last_time = hal_time_get_system_time() / 1000;
	taskEXIT_CRITICAL();
}

static void write_padded_num(uint32_t num, uint8_t digits)
{
	char numbuf[10];
	uint8_t len, i;

	hal_platform_sprintu32(numbuf, num);
	len = strlen(numbuf);
	if (digits > len) {
		for (i = digits; i > len; i--)
			hal_io_write_raw("0", 1);
	}
	hal_io_write_raw(numbuf, len);
}

static void print_event(struct log_event *e, uint32_t time)
{
	struct log_call *c;

	write_padded_num(time, 8);
	hal_io_write_raw(" > ", 3);
	write_padded_num(e->call, 3);
	hal_io_write_raw(": ", 2);
	if (e->call != LOG_INVALID_CALL) {
		c = &call_list[e->call];
		hal_io_write_string(c->description);
		hal_io_write_raw(" ", 1);
	} else {
		c = NULL;
	}
	if (e->identifier != LOG_INVALID_ITEM_ID) {
		hal_io_write_raw("#", 1);
		write_padded_num(e->identifier, 0);
		hal_io_write_raw(" ", 1);
	}
	if (e->action != LOG_INVALID_ACTION) {
		hal_io_write_raw("(", 1);
		write_padded_num(e->action, 0);
		hal_io_write_raw(") ", 2);
	}
	if (c != NULL) {
		hal_io_write_raw("[", 1);
		hal_io_write_string(upcn_dbg_fileget(c->file));
		hal_io_write_raw(":", 1);
		write_padded_num(c->line, 0);
		hal_io_write_raw("]\n", 2);
	}
}

void upcn_dbg_printlogs(void)
{
	uint16_t i;
	uint32_t time = 0;
	struct log_event *e;

	hal_io_lock_com_semaphore();
	hal_io_write_string("\nuPCN LOGFILE\n============\nLogs begin @ ");
	write_padded_num((uint32_t)log_begin_time, 8);
	hal_io_write_string(" ms after boot, ");
	write_padded_num(event_count, 0);
	hal_io_write_string(" entries, relative timestamps\n\n");
	for (i = 0; i < event_count; i++) {
		e = &events[i];
		write_padded_num(i, 3);
		hal_io_write_raw(" @ ", 3);
		time += e->time_offset;
		print_event(e, time);
	}
	hal_io_write_string("\n");
	hal_io_unlock_com_semaphore();
	upcn_dbg_clearlogs();
}

/* DESCRIPTION */

static uint8_t callindex(char *description, char *file, uint16_t line)
{
	uint8_t i;

	for (i = 0; i < call_count; i++) {
		if (call_list[i].line == line
				&& strcmp(call_list[i].description, description)
					== 0
				&& strcmp(upcn_dbg_fileget(call_list[i].file),
					file) == 0)
			return i;
	}
	if (call_count == LOG_MAX_CALLS)
		return LOG_INVALID_CALL;
	call_list[call_count].description = malloc(strlen(description) + 1);
	if (call_list[call_count].description == NULL)
		return LOG_INVALID_CALL;
	strcpy(call_list[call_count].description, description);
	call_list[call_count].file = upcn_dbg_fileindex(file);
	call_list[call_count].line = line;
	return call_count++;
}

#endif /* LOGGING */
