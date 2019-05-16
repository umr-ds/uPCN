#include <unity_fixture.h>

void testupcn(void)
{
	RUN_TEST_GROUP(upcn);
	RUN_TEST_GROUP(simplehtab);
	RUN_TEST_GROUP(sdnv);
	RUN_TEST_GROUP(groundStation);
	RUN_TEST_GROUP(routingTable);
	RUN_TEST_GROUP(bundleStorageManager);
	RUN_TEST_GROUP(beacon);
	RUN_TEST_GROUP(bloomFilter);
	RUN_TEST_GROUP(eidList);
	RUN_TEST_GROUP(eidManager);
	RUN_TEST_GROUP(random);
	RUN_TEST_GROUP(malloc);
	RUN_TEST_GROUP(crc);
	RUN_TEST_GROUP(bundle7Parser);
	RUN_TEST_GROUP(bundle7Serializer);
	RUN_TEST_GROUP(bundle7Reports);
	RUN_TEST_GROUP(bundle7Fragmentation);
	RUN_TEST_GROUP(spp);
	RUN_TEST_GROUP(spp_parser);
	RUN_TEST_GROUP(spp_timecodes);
}
