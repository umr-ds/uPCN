#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "upcn/bundle.h"
#include "upcn/eidManager.h"
#include "bundle7/parser.h"
#include "bundle7/eid.h"

#define FILLED(parser) \
	(((uint8_t *) parser.basedata->next_buffer) - parser.buffer)

static struct bundle *bundle;


void on_finished(struct bundle *_bundle)
{
	bundle = _bundle;
}


int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: upcn_parse <file>\n");
		return EXIT_FAILURE;
	}

	struct bundle7_parser parser;

	eidmanager_init();
	bundle7_parser_init(&parser, on_finished);

	FILE *fd = fopen(argv[1], "r");
	if (fd == NULL) {
		fprintf(stderr, "Error: Could not open file '%s'\n", argv[1]);
		return EXIT_FAILURE;
	}

	// Detect file size
	fseek(fd, 0, SEEK_END);
	size_t fsize = ftell(fd);
	fseek(fd, 0, SEEK_SET); // rewind

	// Read the whole file into memory
	uint8_t *buffer = malloc(fsize);

	if (buffer == NULL) {
		fclose(fd);
		printf("Error: Not enough memory\n");
		return EXIT_FAILURE;
	}

	fread(buffer, fsize, sizeof(uint8_t), fd);
	fclose(fd);

	bundle7_parser_read(&parser, buffer, fsize);

	if (bundle == NULL) {
		fprintf(stderr, "Error: Invalid bundle");
		return EXIT_FAILURE;
	}

	printf("Destination: %s\n", bundle->destination);
	printf("Source:      %s\n", bundle->source);
	printf("Report-To:   %s\n", bundle->report_to);

	struct bundle_block_list *cur_block = bundle->blocks;
	char *eid;

	while (cur_block != NULL) {
		printf("\nBlock %u:\n", cur_block->data->number);

		switch (cur_block->data->type) {
		case BUNDLE_BLOCK_TYPE_PAYLOAD:
			printf("    Type: payload\n");
			break;
		case BUNDLE_BLOCK_TYPE_PREVIOUS_NODE:
			printf("    Type: previous_node\n");
			eid = bundle7_eid_parse(cur_block->data->data,
				cur_block->data->length);
			printf("    Previous Node: %s\n", eid);
			free(eid);
			break;
		case BUNDLE_BLOCK_TYPE_BUNDLE_AGE:
			printf("    Type: bundle_age\n");
			break;
		case BUNDLE_BLOCK_TYPE_HOP_COUNT:
			printf("    Type: hop_count\n");
			break;
		default:
			printf("    Type: unknown\n");
			break;
		}

		printf("    Length: %d\n", cur_block->data->length);

		cur_block = cur_block->next;
	}

	return EXIT_SUCCESS;
}
