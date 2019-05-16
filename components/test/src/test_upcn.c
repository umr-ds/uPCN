#include <unity_fixture.h>
#include "upcn/upcn.h"

TEST_GROUP(upcn);

TEST_SETUP(upcn)
{
}

TEST_TEAR_DOWN(upcn)
{
}

TEST(upcn, clock)
{
	hal_time_init(1234);
	TEST_ASSERT_EQUAL_UINT64(1234, hal_time_get_timestamp_s());
	hal_time_init(0);
	TEST_ASSERT_EQUAL_UINT64(0, hal_time_get_timestamp_s());
}

TEST(upcn, sprintu64)
{
	char tst[22];

	hal_platform_sprintu64(tst, 0);
	TEST_ASSERT_EQUAL_STRING("0", tst);
	hal_platform_sprintu64(tst, 18446744073709551615ULL);
	TEST_ASSERT_EQUAL_STRING("18446744073709551615", tst);
}

TEST_GROUP_RUNNER(upcn)
{
	RUN_TEST_CASE(upcn, clock);
	RUN_TEST_CASE(upcn, sprintu64);
}
