#include "unity.h"
#include <unity_fixture.h>
#include <stdint.h>
#include <stdio.h>

int testupcn_putc(int c)
{
	putchar(c);
	return 0;
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST_GROUP(simple_queue);
	return UNITY_END();
}
