#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#include <zmq.h>

#include <tools.h>
#include <test.h>

static void *pub, *sub;
static int comm_brk;

static jmp_buf jmp;
static int test_result;
static int in_test;

#define MAX_ALLOCATIONS 65536
static void *allocations[MAX_ALLOCATIONS];
static size_t allocation_count;

void test_init(void *zmq_pub, void *zmq_sub)
{
	pub = zmq_pub;
	sub = zmq_sub;
}

int test_run(struct test_def test)
{
	if (test.run == NULL) {
		printf("Error: No test runner defined!\n");
		return TEST_FAILURE;
	}
	allocation_count = 0;
	if (test.init != NULL)
		test.init();
	test_result = TEST_SUCCESS;
	in_test = 1;
	if (!setjmp(jmp))
		test.run();
	in_test = 0;
	if (test.cleanup != NULL)
		test.cleanup();
	for (size_t i = 0; i < allocation_count; i++)
		free(allocations[allocation_count]);
	allocation_count = 0;
	return test_result;
}

int test_comm_broken(void)
{
	return comm_brk;
}

/* Test "library" */

void test_log(const char *message, const char *file, const int line)
{
	if (!in_test)
		return;
	printf("%s [%s:%d]", message, file, line);
}

void test_assert(const int condition, const char *assertion,
	const char *file, const int line)
{
	if (!in_test)
		return;
	if (!condition) {
		printf("Assertion (%s) failed [%s:%d]\n",
			assertion, file, line);
		test_result = TEST_FAILURE;
		longjmp(jmp, 1);
	}
}

void test_fail(const char *message, const char *file, const int line)
{
	if (!in_test)
		return;
	printf("Test failed: %s [%s:%d]\n", message, file, line);
	test_result = TEST_FAILURE;
	longjmp(jmp, 1);
}

static uint8_t rbuf[10];

void test_send(const void *data, const size_t length,
	const char *file, const int line)
{
	if (
		zmq_send(pub, data, length, 0) < 0 ||
		zmq_recv(pub, rbuf, 10, 0) <= 0
	) {
		perror("zmq_send()");
		printf("Error: Sending data failed [%s:%d]\n",
			file, line);
		comm_brk = 1;
		test_result = TEST_FAILURE;
		longjmp(jmp, 1);
	}
}

void test_sendstr(char *message, const char *file, const int line)
{
	while (*message != '\0') {
		if (
			zmq_send(pub, message, 1, 0) < 0 ||
			zmq_recv(pub, rbuf, 10, 0) <= 0
		) {
			perror("zmq_send()");
			printf("Error: Sending data failed [%s:%d]\n",
				file, line);
			comm_brk = 1;
			test_result = TEST_FAILURE;
			longjmp(jmp, 1);
		}
		message++;
	}
}

int test_receive(
	void *buffer, int max, int noblock, const char *file, const int line)
{
	int len;

	if (!in_test)
		return 0;
	len = zmq_recv(sub, buffer, max, noblock ? ZMQ_DONTWAIT : 0);
	if (len == -1) {
		if (errno == EAGAIN)
			return 0; /* Timeout */
		printf("Error: Receive failed [%s:%d]\n", file, line);
		comm_brk = 1;
		test_result = TEST_FAILURE;
		longjmp(jmp, 1);
		return 0;
	}
	return len;
}

void *test_alloc(const size_t size, const char *file, const int line)
{
	void *res;

	if (!in_test)
		return NULL;
	if (allocation_count == MAX_ALLOCATIONS)
		goto fail;
	res = malloc(size);
	if (res == NULL)
		goto fail;
	allocations[allocation_count++] = res;
	return res;
fail:
	printf("Error: Allocation failed [%s:%d]\n", file, line);
	test_result = TEST_FAILURE;
	longjmp(jmp, 1);
	return NULL;
}

void test_free(void *ptr)
{
	/* Do nothing. This is done on cleanup. */
	(void)ptr;
}

void *test_alloc_unmanaged(const size_t size)
{
	return malloc(size);
}

void test_free_unmanaged(void *ptr)
{
	free(ptr);
}
