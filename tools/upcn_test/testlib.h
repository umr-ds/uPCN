#ifndef TESTLIB_H_INCLUDED
#define TESTLIB_H_INCLUDED

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

#include <tools.h>
#include <test.h>

/* Test "library" */

#ifdef LOG
#undef LOG
#endif /* LOG */

#define LOG(message) test_log(message, __FILE__, __LINE__)

void test_log(const char *message, const char *file, const int line);

#ifdef ASSERT
#undef ASSERT
#endif /* ASSERT */

#define ASSERT(condition) do { \
	test_assert(condition, #condition, __FILE__, __LINE__); \
	if (!(condition)) \
		for (;;) \
			; \
} while (0)

void test_assert(const int condition, const char *assertion,
	const char *file, const int line);

#define FAIL(message) test_fail(message, __FILE__, __LINE__)

void test_fail(const char *message, const char *file, const int line);

#define SEND(data, length) test_send(data, length, __FILE__, __LINE__)
#define SENDSTR(message) test_sendstr(message, __FILE__, __LINE__)
#define RECEIVE(buf, max) test_receive(buf, max, 0, __FILE__, __LINE__)
#define RECEIVE_NOBLOCK(buf, max) test_receive(buf, max, 1, __FILE__, __LINE__)

void test_send(const void *data, const size_t length,
	const char *file, const int line);
void test_sendstr(char *message, const char *file, const int line);
int test_receive(
	void *buffer, int max, int noblock, const char *file, const int line);

#ifndef NO_MALLOC_REDEF

#define malloc(size) test_alloc(size, __FILE__, __LINE__)
#define free(ptr) test_free(ptr)

#define malloc_unmanaged(size) test_alloc_unmanaged(size)
#define free_unmanaged(ptr) test_free_unmanaged(ptr)

#else /* NO_MALLOC_REDEF */

#define malloc_unmanaged(size) malloc(size)
#define free_unmanaged(ptr) free(ptr)

#endif /* NO_MALLOC_REDEF */

void *test_alloc(const size_t size, const char *file, const int line);
void test_free(void *ptr);
void *test_alloc_unmanaged(const size_t size);
void test_free_unmanaged(void *ptr);

/* Generic functions for inside tests */

bool conv_bool(const char *input);
unsigned long conv_ulong(const char *input);
void check_eid(char *eid);
size_t eid_ssp_index(char *eid);
uint64_t dtn_timestamp(void);
void wait_for(int64_t msec);
void resettime(uint64_t ms);
void wait_until(int64_t t);
uint64_t testtime(void);
int bundleheadercmp(struct bundle *a, struct bundle *b);
int bundlecmp(struct bundle *a, struct bundle *b);

#endif /* TESTLIB_H_INCLUDED */
