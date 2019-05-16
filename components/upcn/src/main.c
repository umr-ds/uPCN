#include <stdio.h>
#include <stdlib.h>
#include "upcn/init.h"
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include "upcn/agent_manager.h"

static int char_to_uint16_t(const char *str, uint16_t *res)
{
	char *end;
	long val;

	errno = 0;
	val = strtol(str, &end, 10);
	if (errno == ERANGE || val <= 0  || val > UINT16_MAX ||
	    end == str || *end != '\0')
		return 1;
	*res = (uint16_t)val;
	return 0;
}

int main(int argc, char *argv[])
{
	uint16_t io_socket_port = 0;

	if (argc >= 2 && char_to_uint16_t(argv[1], &io_socket_port))
		io_socket_port = 0;
	init(io_socket_port);
	start_tasks();
	agents_setup();
	return start_os();
}
