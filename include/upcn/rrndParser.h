#ifndef RRNDPARSER_H_INCLUDED
#define RRNDPARSER_H_INCLUDED

#include "parser.h"
#include "rrndCommand.h"
#include "upcn.h"

#include <stdint.h>

enum rrnd_parser_stage {
	RRNDP_EXPECT_COMMAND_TYPE,
	RRNDP_EXPECT_GS_EID_LENGTH,
	RRNDP_EXPECT_GS_EID,
	RRNDP_EXPECT_SOURCE_GS_EID_LENGTH,
	RRNDP_EXPECT_SOURCE_GS_EID,
	RRNDP_EXPECT_TLE_LENGTH,
	RRNDP_EXPECT_TLE,
	RRNDP_EXPECT_RELIABILITY,
};

struct rrnd_parser {
	// "base class"
	struct parser *basedata;
	// Function which is called when the parsing process has been completed
	void (*send_callback)(struct rrnd_command *);
	// Private / intermediary data: current stage and associated data
	enum rrnd_parser_stage stage;
	struct rrnd_command *command;
	uint32_t current_index;
	uint32_t current_int;
	char *current_string;
};

struct parser *rrnd_parser_init(
	struct rrnd_parser *parser,
	void (*send_callback)(struct rrnd_command *));
size_t rrnd_parser_read(
	struct rrnd_parser *const parser,
	const uint8_t *buffer, size_t length);
enum upcn_result rrnd_parser_reset(struct rrnd_parser *parser);

#endif /* RRNDPARSER_H_INCLUDED */
