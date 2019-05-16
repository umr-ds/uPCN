#ifndef ROUTERPARSER_H_INCLUDED
#define ROUTERPARSER_H_INCLUDED

#include <stdint.h>

#include "upcn/upcn.h"
#include "upcn/parser.h"
#include "upcn/routerTask.h"

enum router_parser_stage {
	RP_EXPECT_COMMAND_TYPE,
	RP_EXPECT_GS_START_DELIMITER,
	RP_EXPECT_GS_EID,
	RP_EXPECT_GS_RELIABILITY_SEPARATOR,
	RP_EXPECT_GS_RELIABILITY,
	RP_EXPECT_GS_CLA_ADDR_SEPARATOR,
	RP_EXPECT_CLA_ADDR_START_DELIMITER,
	RP_EXPECT_CLA_ADDR,
	RP_EXPECT_CLA_ADDR_NODES_SEPARATOR,
	RP_EXPECT_NODE_LIST_START_DELIMITER,
	RP_EXPECT_NODE_START_DELIMITER,
	RP_EXPECT_NODE_EID,
	RP_EXPECT_NODE_SEPARATOR,
	RP_EXPECT_NODES_CONTACTS_SEPARATOR,
	RP_EXPECT_CONTACT_LIST_START_DELIMITER,
	RP_EXPECT_CONTACT_START_DELIMITER,
	RP_EXPECT_CONTACT_START_TIME,
	RP_EXPECT_CONTACT_END_TIME,
	RP_EXPECT_CONTACT_BITRATE,
	RP_EXPECT_CONTACT_NODE_LIST_START_DELIMITER,
	RP_EXPECT_CONTACT_NODE_START_DELIMITER,
	RP_EXPECT_CONTACT_NODE_EID,
	RP_EXPECT_CONTACT_NODE_SEPARATOR,
	RP_EXPECT_CONTACT_END_DELIMITER,
	RP_EXPECT_CONTACT_SEPARATOR,
	RP_EXPECT_COMMAND_END_MARKER
};

struct router_parser {
	struct parser *basedata;
	void (*send_callback)(struct router_command *);
	enum router_parser_stage stage;
	struct router_command *router_command;
	int current_index;
	char *current_int_data;
	struct endpoint_list *current_eid;
	struct contact_list *current_contact;
};

struct parser *router_parser_init(
	struct router_parser *parser,
	void (*send_callback)(struct router_command *));
size_t router_parser_read(struct router_parser *parser,
	const uint8_t *buffer, size_t length);
enum upcn_result router_parser_reset(struct router_parser *parser);
void process_unmanaged_eids(struct ground_station *gs, const bool add);

#endif /* ROUTERPARSER_H_INCLUDED */
