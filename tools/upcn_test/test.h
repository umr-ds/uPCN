#ifndef TEST_H_INCLUDED
#define TEST_H_INCLUDED

#include <stddef.h>
#include <stdio.h>

#define TEST_SUCCESS 1
#define TEST_FAILURE 0

struct test_def {
	void (*run)(void);
	void (*init)(void);
	void (*cleanup)(void);
};

/* Call this from the main routine */

void test_init(void *zmq_pub, void *zmq_sub);
int test_run(const struct test_def test);
int test_comm_broken(void);

#endif /* TEST_H_INCLUDED */
