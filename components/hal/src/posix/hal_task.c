/*
 * hal_task.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for thread-related functionality
 *
 */


/* enable linux features when linux OS */
#if linux
#define _GNU_SOURCE
#endif

#include "hal_config.h"
#include "hal_task.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <hal_defines.h>
#include <hal_debug.h>
#include <hal_time.h>

struct task_description {
	void (*task_function)(void *param);
	void *task_parameter;
};

static void *execute_pthread_compat(void *task_description)
{
	struct task_description *desc =
		(struct task_description *)task_description;
	void (*task_function)(void *param) = desc->task_function;
	void *task_parameter = desc->task_parameter;

	free(task_description);
	task_function(task_parameter);
	return NULL;
}

Task_t hal_task_create(void (*task_function)(void *), const char *task_name,
		    int task_priority, void *task_parameters,
		    size_t task_stack_size, void *task_tag)
{
	pthread_t *thread = malloc(sizeof(pthread_t));

	if (thread == NULL)
		return NULL;

	struct sched_param param;
	pthread_attr_t tattr;
	int error_code;
	struct task_description *desc = malloc(sizeof(*desc));

	if (desc == NULL) {
		LOG("malloc failed");
		free(thread);
		return NULL;
	}

	/* initialize an attribute to the default value */
	if (pthread_attr_init(&tattr)) {
		/* abort if error occurs */
		LOG("Initialising the tasks attributes failed!");
		free(thread);
		free(desc);
		return NULL;
	}

	/* set the scheduling policy */
	if (pthread_attr_setschedpolicy(&tattr, SCHED_RR)) {
		/* abort if error occurs */
		LOG("Setting the scheduling policy failed!");
		free(thread);
		free(desc);
		return NULL;
	}

	/* Create thread in detached state, so that no cleanup is necessary */
	if (pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED))
		LOG("Setting detached state failed!");

	/* set the scheduling priority (just use the absolute minimum and add */
	/* the given priority */
	param.sched_priority = sched_get_priority_min(SCHED_RR) +
			task_priority;
	if (pthread_attr_setschedparam(&tattr, &param)) {
		/* abort if error occurs */
		LOG("Setting the scheduling priority failed!");
		free(thread);
		free(desc);
		return NULL;
	}

	/* update the stack size of the thread (only if greater than 0, */
	/* otherwise set stack size to the default value */
	if (task_stack_size != 0 &&
			pthread_attr_setstacksize(&tattr, task_stack_size)) {
		/* abort if error occurs */
		LOG("Setting the tasks stack size failed! Wrong value!");
		free(thread);
		free(desc);
		return NULL;
	}

	desc->task_function = task_function;
	desc->task_parameter = task_parameters;

	error_code = pthread_create(thread, &tattr,
				    execute_pthread_compat, desc);

	if (error_code) {
		free(thread);
		free(desc);
		LOG("Thread Creation failed!");
		return NULL;
	}

#if LINUX_SPECIFIC_API
	if (pthread_setname_np(thread, task_name)) {
		LOG("Could not set thread name!");
		exit(NULL);
	}
#endif

	/* destroy the attr-object */
	pthread_attr_destroy(&tattr);

	return thread;
}


void hal_task_start_scheduler(void)
{
	/* Put the calling thread (in this case the main thread) to */
	/* sleep indefinitely */
	pause();
}


void hal_task_delay(int delay)
{
	usleep(delay*1000);
}


void hal_task_delete(Task_t task)
{
	pthread_kill(*task, SIGKILL);
}


void hal_task_suspend_scheduler(void)
{
	/* not possible in POSIX, RTOS necessary */
}


void hal_task_resume_scheduler(void)
{
	/* not possible in POSIX, RTOS necessary */
}
