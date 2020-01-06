#include "bundle6/create.h"

#include "upcn/bundle.h"
#include "upcn/common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct bundle *bundle6_create_local(
	void *payload, size_t payload_length,
	const char *source, const char *destination,
	uint64_t creation_time, uint64_t lifetime,
	enum bundle_proc_flags proc_flags)
{
	struct bundle *bundle = bundle_init();

	if (bundle == NULL) {
		free(payload);
		return NULL;
	}

	bundle->protocol_version = 0x6;
	bundle->proc_flags = proc_flags | BUNDLE_V6_FLAG_SINGLETON_ENDPOINT;

	// Creation time
	bundle->creation_timestamp = creation_time;
	bundle->sequence_number = 1;
	bundle->lifetime = lifetime * 1000000;

	// Create payload block and block list
	bundle->payload_block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	bundle->payload_block->flags = BUNDLE_V6_BLOCK_FLAG_LAST_BLOCK;
	bundle->blocks = bundle_block_entry_create(bundle->payload_block);

	if (bundle->payload_block == NULL || bundle->blocks == NULL)
		goto fail;

	bundle->source = strdup(source);
	if (bundle->source == NULL || strchr(source, ':') == NULL)
		goto fail;

	bundle->destination = strdup(destination);
	if (bundle->destination == NULL || strchr(destination, ':') == NULL)
		goto fail;

	bundle->report_to = strdup("dtn:none");
	bundle->current_custodian = strdup("dtn:none");

	bundle->payload_block->data = payload;
	bundle->payload_block->length = payload_length;

	if (bundle_recalculate_header_length(bundle) == UPCN_FAIL) {
		bundle->payload_block->data = NULL; // prevent double-free
		goto fail;
	}

	return bundle;

fail:
	free(payload);
	bundle_free(bundle);
	return NULL;
}
