/*
 * hal_platform.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for platform-specific functionality
 *
 */

#include "hal_platform.h"
#include "upcn/buildFlags.h"

#include "hal_crypto.h"
#include "hal_io.h"
#include "hal_debug.h"
#include "hal_random.h"
#include "hal_time.h"
#include "hal_task.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <signal.h>
#include <unistd.h>
#include <hal_defines.h>
#include <hal_config.h>
#include "upcn/upcn.h"
#include <cla.h>


#if LINUX_SPECIFIC_API
#include <malloc.h>
#endif

static char cmd_arg[CMD_ARG_MAX_LENGTH];

static void exit_handler(int signal)
{
	if (cla_exit() != EXIT_SUCCESS)
		LOG("Closing all sockets in a graceful manner failed!");


	if (signal == SIGHUP) {
		LOG("SIGHUP detected, terminating");
		exit(EXIT_SUCCESS);
	}

	if (signal == SIGTERM) {
		LOG("SIGTERM detected, terminating");
		exit(EXIT_SUCCESS);
	}
}

static void setup_exit_handler(void)
{
	struct sigaction sa;

	/* Setup the sighub handler */
	sa.sa_handler = &exit_handler;

	/* Restart the system call, if at all possible */
	sa.sa_flags = SA_RESTART;

	/* Block every signal during the handler */
	sigfillset(&sa.sa_mask);

	/* Intercept SIGHUP with this handler */
	if (sigaction(SIGHUP, &sa, NULL) == -1)
		LOG("Error: cannot handle SIGHUP"); /* Should not happen */

	/* Intercept SIGTERM with this handler	 */
	if (sigaction(SIGTERM, &sa, NULL) == -1)
		LOG("Error: cannot handle SIGHUP"); /* Should not happen */
}

void hal_platform_led_pin_set(uint8_t led_identifier, int mode)
{
	/* not relevant for the POSIX implementation */
}


void hal_platform_led_set(int led_preset)
{
	/* not relevant for the POSIX implementation */
}

void mpu_init(void)
{
	/* currently not relevant for the POSIX implementation */
}


void hal_platform_init(uint16_t io_socket_port)
{
	setup_exit_handler();
	snprintf(cmd_arg, CMD_ARG_MAX_LENGTH, "%" PRIu16, io_socket_port);
}


void hal_platform_print_system_info(void)
{
	//static char wbuf[16];

#if LINUX_SPECIFIC_API
	struct mallinfo mi = mallinfo();
#endif

	/* hal_io_lock_com_semaphore();
	 * hal_debug_write_string("\nSTATE INFO\n==========\n");
	 * hal_debug_write_string("Current time:        ");
	 * hal_platform_sprintu64(wbuf, hal_time_get_timestamp_s());
	 * hal_debug_write_string(wbuf);
	 */

#if LINUX_SPECIFIC_API
	/* hal_debug_write_string("\nTotal mem allocated: ");
	 * hal_platform_sprintu32(wbuf, mi.uordblks);
	 * hal_debug_write_string(wbuf);
	 * hal_debug_write_string(" bytes\nTotal mem free:      ");
	 * hal_platform_sprintu32(wbuf, mi.fordblks);
	 * hal_debug_write_string(wbuf);
	 * hal_debug_write_string(" bytes\nTotal mem in pool:   ");
	 * hal_platform_sprintu32(wbuf, mi.arena);
	 * hal_debug_write_string(wbuf);
	 * hal_debug_write_string(" bytes\n\n");
	 */
#endif

	//hal_io_unlock_com_semaphore();

}

static const char digits[] = "0123456789abcdef";

#define sprintu_generic(T, cur, base, num) do { \
	T tmp = num; \
	do { \
		++cur; \
		tmp /= base; \
	} while (tmp); \
	*cur = '\0'; \
	do { \
		--cur; \
		*cur = digits[num % base]; \
		num /= base; \
	} while (num); \
} while (0)

char *hal_platform_sprintu32(char *out, uint32_t num)
{
	char *cur = out;

	sprintu_generic(uint32_t, cur, 10LL, num);
	return out;
}

char *hal_platform_sprintu32x(char *out, uint32_t num)
{
	char *cur = out;

	*(cur++) = '0';
	*(cur++) = 'x';
	sprintu_generic(uint32_t, cur, 16LL, num);
	return out;
}

char *hal_platform_sprintu64(char *out, uint64_t num)
{
	char *cur = out;

	sprintu_generic(uint64_t, cur, 10LL, num);
	return out;
}

char *hal_platform_sprintu64x(char *out, uint64_t num)
{
	char *cur = out;

	*(cur++) = '0';
	*(cur++) = 'x';
	sprintu_generic(uint64_t, cur, 16LL, num);
	return out;
}

void hal_platform_restart_upcn(void)
{
	/* try to close the open port */
	hal_io_exit();
	cla_exit();

	LOG("Restarting!");
	char * const args[] = {
		"/proc/self/exe",
		cmd_arg,
		NULL
	};

	execv("/proc/self/exe", args);
}



void hal_platform_hard_restart_upcn(void)
{
	/* TODO test proper functionality */
	LOG("Restarting hard!");
	char * const args[] = {
		"/proc/self/exe",
		cmd_arg,
		NULL
	};

	execv("/proc/self/exe", args);
}
