#include <unity_fixture.h>
#include "upcn/upcn.h"
#include <test.h>
#include "upcn/init.h"

#ifndef ARCHITECTURE_STM32
#include <stdlib.h>
#endif

#define WR(c) hal_io_write_raw(c, 1)

static int test_errors;

void upcntest_print(void)
{
	if (!test_errors)
		hal_debug_printf("uPCN unittests succeeded\n");
	else
		hal_debug_printf("uPCN unittests resulted in %d error(s)\n",
			test_errors);
}

void test_task(void *args)
{
	static char *argv[1] = { "testupcn" };

	LOG("Starting testsuite...");


#ifdef ARCHITECTURE_STM32

	int8_t led, c;

	hal_platform_led_set(1);
	LOG("Blinking some LEDs...");
	for (led = 0; led < 5; led++) {
		hal_platform_led_set(led);
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
	for (led = 4; led >= 0; led--) {
		hal_platform_led_set(led);
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}

	hal_io_write_string("\n");
#endif

	/* Disable the logger spamming out output */
	upcn_dbg_log_disable_output();
	/* Start Unity */
	test_errors = UnityMain(1, argv, testupcn);
	upcn_dbg_log_enable_output();

#ifdef ARCHITECTURE_STM32

	/* Blink to indicate that tests are done */
	led = 0;
	for (c = 0; c < 6; c++) {
		hal_platform_led_set(led = 1 - led);
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}

	/* Print logfile */
	upcn_dbg_printlogs();
	/* Start upcn */
	LOG("Tests done, switching to uPCN...");
	start_tasks();
	/* Delete task */
	hal_task_delete(NULL);
#else
	if (test_errors == 0) {
		LOG("Unittests finished without errors! SUCCESS!");
		exit(EXIT_SUCCESS);
	} else {
		LOG("Unittests finished with errors! FAILURE!");
		exit(EXIT_FAILURE);
	}
#endif
}

int main(void)
{
	init(0);

#ifdef ARCHITECTURE_STM32

	hal_task_create(test_task, "test_task", 0, NULL, 1024, NULL);

#else

	hal_task_create(test_task, "test_task", 0, NULL, 0, NULL);

#endif
	return start_os();
}
