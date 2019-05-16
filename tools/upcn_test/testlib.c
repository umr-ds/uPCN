#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include <upcn/upcn.h>
#include <upcn/bundle.h>
#include <testlib.h>

/* Generic functions for parameters, ... */

bool conv_bool(const char *input)
{
	char first;

	ASSERT(*input != '\0');
	first = tolower(*input);
	if (first == 'y' || first == '1')
		return true;
	else if (first == 'n' || first == '0')
		return false;
	FAIL("Value of boolean argument not determinable!");
	return false;
}

unsigned long conv_ulong(const char *input)
{
	unsigned long result;

	errno = 0;
	result = strtoul(input, NULL, 0);
	if (errno != 0)
		FAIL("Value of unsigned integral argument not determinable!");
	return result;
}

void check_eid(char *eid)
{
	while (*eid != '\0') {
		if (!(
			(*eid >= 'A' && *eid <= 'Z') ||
			(*eid >= 'a' && *eid <= 'z') ||
			(*eid >= '0' && *eid <= '9') ||
			*eid == '/' || *eid == '.' ||
			*eid == '_' || *eid == ':' ||
			*eid == '-'
		)) {
			FAIL("Invalid character in EID argument!");
		}
		eid++;
	}
}

size_t eid_ssp_index(char *eid)
{
	size_t index = 0;

	while (*eid != '\0' && *eid != ':') {
		eid++;
		index++;
	}
	return index;
}

uint64_t dtn_timestamp(void)
{
	return DTN_TIMESTAMP_OFFSET + (uint64_t)time(NULL);
}

void wait_for(int64_t msec)
{
	struct timespec req = { msec / 1000, 1000000 * (msec % 1000) };

	if (msec <= 0)
		return;
	else if (msec > 100)
		printf("Waiting %llu ms...\n", (unsigned long long)msec);
	nanosleep(&req, NULL);
}

static struct timespec test_time;
static uint64_t test_ms;

void resettime(uint64_t s)
{
	clock_gettime(CLOCK_REALTIME, &test_time);
	test_ms = s * 1000LL;
}

static uint64_t get_ms(void)
{
	struct timespec newtime;
	uint64_t clock_ms;

	clock_gettime(CLOCK_REALTIME, &newtime);
	clock_ms = (newtime.tv_sec - test_time.tv_sec) * 1000;
	clock_ms += (newtime.tv_nsec - test_time.tv_nsec) / 1000000;
	return clock_ms + test_ms;
}

void wait_until(int64_t t)
{
	wait_for(t * 1000LL - (int64_t)get_ms());
}

uint64_t testtime(void)
{
	return get_ms() / 1000LL;
}

int bundleheadercmp(struct bundle *a, struct bundle *b)
{
	int eq = 1;

	eq &= (a->protocol_version == b->protocol_version);
	eq &= (a->creation_timestamp == b->creation_timestamp);
	eq &= (a->sequence_number == b->sequence_number);
	eq &= (a->lifetime == b->lifetime);
	return eq;
}

int bundlecmp(struct bundle *a, struct bundle *b)
{
	size_t pli;
	int eq = 1;

	eq &= bundleheadercmp(a, b);
	eq &= (a->proc_flags == b->proc_flags);
	eq &= (a->fragment_offset == b->fragment_offset);
	eq &= (a->total_adu_length == b->total_adu_length);
	eq &= (a->payload_block->length == b->payload_block->length);
	if (!eq)
		return eq;
	for (pli = 0; pli < a->payload_block->length; pli++)
		eq &= (a->payload_block->data[pli]
			== b->payload_block->data[pli]);
	return eq;
}
