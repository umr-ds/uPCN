#ifndef AGENT_MANAGER_H_INCLUDED
#define AGENT_MANAGER_H_INCLUDED

#include <stdint.h>

#include "upcn/upcn.h"

struct agent {
	const char *sink_identifier;
	void (*callback)(const void *payload, const size_t);
};

struct agent_list {
	struct agent *agent_data;
	struct agent_list *next;
};

int agent_forward(char *sink_identifier,
		  char *data,
		  size_t length);

int agent_register(const char *sink_identifier,
		   void (*callback)(const void *payload, const size_t));

int agent_deregister(char *sink_identifier);

struct agent *agent_search(const char *sink_identifier);

int agents_setup(void);

#endif /* AGENT_MANAGER_H_INCLUDED */
