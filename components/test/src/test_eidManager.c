#include <stdlib.h>
#include <string.h>

#include <unity_fixture.h>

#include "upcn/upcn.h"
#include "upcn/eidManager.h"
#include "upcn/simplehtab.h"

TEST_GROUP(eidManager);

#define EMTEST_RUNS 10
#define EMTEST_EID_COUNT 20
#define EMTEST_DEFAULT_EID "emt00"

static struct htab *em_htab;
static char *eids[EMTEST_EID_COUNT];

TEST_SETUP(eidManager)
{
	em_htab = eidmanager_init();
}

TEST_TEAR_DOWN(eidManager)
{
	htab_trunc(em_htab);
}

TEST(eidManager, dyn_test)
{
	const size_t len = strlen(EMTEST_DEFAULT_EID);

	for (uint8_t i = 0; i < EMTEST_EID_COUNT; i++) {
		eids[i] = malloc(len + 1);

		memcpy(eids[i], EMTEST_DEFAULT_EID, len + 1);

		eids[i][len - 1] += i % 10;
		eids[i][len - 2] += i / 10;
	}
	for (uint8_t i = 0; i < EMTEST_EID_COUNT; i++)
		eids[i] = eidmanager_alloc_ref(eids[i], 1);
	for (uint8_t i = 0; i < EMTEST_EID_COUNT; i++)
		TEST_ASSERT_EQUAL_PTR(
			eids[i], eidmanager_alloc_ref(eids[i], 1));
	for (uint8_t i = 0; i < EMTEST_EID_COUNT; i++)
		eidmanager_free_ref(eids[i]);
}

TEST(eidManager, stat_test)
{
	char *ref1 = eidmanager_alloc_ref("TEST_A", 0);
	char *ref2 = eidmanager_alloc_ref("TEST_B", 0);
	char *ref3 = eidmanager_alloc_ref("TEST_C", 0);

	TEST_ASSERT_EQUAL_PTR(ref1, eidmanager_alloc_ref("TEST_A", 0));
	TEST_ASSERT_EQUAL_PTR(ref2, eidmanager_alloc_ref("TEST_B", 0));
	TEST_ASSERT_EQUAL_PTR(ref3, eidmanager_alloc_ref("TEST_C", 0));
}

TEST_GROUP_RUNNER(eidManager)
{
	RUN_TEST_CASE(eidManager, dyn_test);
	RUN_TEST_CASE(eidManager, stat_test);
}
