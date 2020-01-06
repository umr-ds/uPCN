#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "cla/cla.h"
#include "cla/cla_contact_tx_task.h"
#include "cla/posix/cla_tcp_common.h"
#include "cla/posix/cla_tcp_util.h"

#include "platform/hal_io.h"
#include "platform/hal_semaphore.h"

#include "upcn/common.h"
#include "upcn/result.h"

#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

enum upcn_result cla_tcp_config_init(
	struct cla_tcp_config *config,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	if (cla_config_init(&config->base, bundle_agent_interface) != UPCN_OK)
		return UPCN_FAIL;

	config->listen_task = NULL;
	config->socket = -1;

	return UPCN_OK;
}

enum upcn_result cla_tcp_single_config_init(
	struct cla_tcp_single_config *config,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	if (cla_tcp_config_init(&config->base,
				bundle_agent_interface) != UPCN_OK)
		return UPCN_FAIL;

	config->link = NULL;

	return UPCN_OK;
}

enum upcn_result cla_tcp_link_init(
	struct cla_tcp_link *link, int connected_socket,
	struct cla_tcp_config *config)
{
	ASSERT(connected_socket >= 0);
	link->connection_socket = connected_socket;

	// This will fire up the RX and TX tasks
	if (cla_link_init(&link->base, &config->base) != UPCN_OK)
		return UPCN_FAIL;

	return UPCN_OK;
}

enum upcn_result cla_tcp_read(struct cla_link *link,
			      uint8_t *buffer, size_t length,
			      size_t *bytes_read)
{
	struct cla_tcp_link *tcp_link = (struct cla_tcp_link *)link;

	const ssize_t ret = recv(
		tcp_link->connection_socket,
		buffer,
		length,
		0
	);

	if (ret < 0) {
		LOGF("TCP: Error reading from socket: %s", strerror(errno));
		link->config->vtable->cla_disconnect_handler(link);
		return UPCN_FAIL;
	} else if (ret == 0) {
		LOGF("TCP: A peer (via CLA %s) has disconnected gracefully!",
		     link->config->vtable->cla_name_get());
		link->config->vtable->cla_disconnect_handler(link);
		return UPCN_FAIL;
	}
	if (bytes_read)
		*bytes_read = ret;
	return UPCN_OK;
}

enum upcn_result cla_tcp_connect(struct cla_tcp_config *const config,
				 const char *node, const char *service)
{
	if (node == NULL || service == NULL)
		return UPCN_FAIL;

	config->socket = create_tcp_socket(
		node,
		service,
		true,
		NULL
	);

	if (config->socket < 0)
		return UPCN_FAIL;

	LOGF(
		"TCP: CLA %s is now connected to [%s]:%s",
		config->base.vtable->cla_name_get(),
		node,
		service
	);

	return UPCN_OK;
}

enum upcn_result cla_tcp_listen(struct cla_tcp_config *config,
				const char *node, const char *service,
				int backlog)
{
	if (node == NULL || service == NULL)
		return UPCN_FAIL;

	config->socket = create_tcp_socket(
		node,
		service,
		false,
		NULL
	);

	if (config->socket < 0)
		return UPCN_FAIL;

	// Listen for incoming connections.
	if (listen(config->socket, backlog) < 0) {
		LOGF("TCP: Listening to socket failed: %s", strerror(errno));
		close(config->socket);
		config->socket = -1;
		return UPCN_FAIL;
	}

	LOGF(
		"TCP: CLA %s is now listening on [%s]:%s",
		config->base.vtable->cla_name_get(),
		node,
		service
	);

	return UPCN_OK;
}

int cla_tcp_accept_from_socket(struct cla_tcp_config *config,
			       const int listener_socket,
			       char **const addr)
{
	const int enable = 1;
	struct sockaddr_storage sockaddr_tmp;
	socklen_t sockaddr_tmp_len = sizeof(struct sockaddr_storage);
	int sock = -1;

	while ((sock = accept(listener_socket,
			      (struct sockaddr *)&sockaddr_tmp,
			      &sockaddr_tmp_len)) == -1) {
		const int err = errno;

		LOGF("TCP: Accepting connection failed: %s", strerror(err));

		// See "Error handling" section of Linux man page
		if (err != EAGAIN && err != EINTR && err != ENETDOWN &&
				err != EPROTO && err != ENOPROTOOPT &&
				err != EHOSTDOWN && err != ENONET &&
				err != EHOSTUNREACH && err != EOPNOTSUPP &&
				err != ENETUNREACH && err != EWOULDBLOCK)
			return -1;
	}

	char *const cla_addr = cla_tcp_sockaddr_to_cla_addr(
		(struct sockaddr *)&sockaddr_tmp,
		sockaddr_tmp_len
	);

	if (cla_addr == NULL) {
		close(sock);
		return -1;
	}

	LOGF("TCP: Connection accepted from %s (via CLA %s)!",
	     cla_addr, config->base.vtable->cla_name_get());

	if (addr != NULL)
		*addr = cla_addr;
	else
		free(cla_addr);

	/* Disable the nagle algorithm to prevent delays in responses */
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		       &enable, sizeof(int)) < 0) {
		LOGF("TCP: setsockopt(TCP_NODELAY) failed: %s",
		     strerror(errno));
	}

	return sock;
}

void cla_tcp_single_disconnect_handler(struct cla_link *link)
{
	struct cla_tcp_single_config *tcp_config
		= (struct cla_tcp_single_config *)link->config;

	cla_generic_disconnect_handler(link);
	tcp_config->link = NULL;
}

void cla_tcp_establish_socket_link(struct cla_tcp_single_config *config,
				   int sock, const size_t struct_size)
{
	ASSERT(struct_size >= sizeof(struct cla_tcp_link));
	struct cla_tcp_link *link = malloc(struct_size);

	ASSERT(!config->link);
	config->link = link;

	if (cla_tcp_link_init(link, sock, &config->base) != UPCN_OK)
		LOG("TCP: Error creating a link instance!");
	else
		cla_link_wait_cleanup(&link->base);
	config->link = NULL;
	free(link);
}

void cla_tcp_single_connect_link(struct cla_tcp_single_config *config,
				 const size_t struct_size)
{
	cla_tcp_establish_socket_link(config, config->base.socket, struct_size);
}

void cla_tcp_single_listen_task(struct cla_tcp_single_config *config,
				const size_t struct_size)
{
	int sock;

	for (;;) {
		sock = cla_tcp_accept_from_socket(
			&config->base,
			config->base.socket,
			NULL
		);
		if (sock == -1)
			break; // cla_tcp_accept_from_socket failing is fatal

		cla_tcp_establish_socket_link(config, sock, struct_size);

		LOGF("TCP: CLA \"%s\" is looking for a new connection now!",
		     config->base.base.vtable->cla_name_get());
	}

	LOG("TCP: Socket connection broke, terminating listener.");
	ASSERT(0);
}

void cla_tcp_single_link_creation_task(struct cla_tcp_single_config *config,
				       const size_t struct_size)
{
	if (config->tcp_active)
		cla_tcp_single_connect_link(config, struct_size);
	else
		cla_tcp_single_listen_task(config, struct_size);
}

struct cla_tx_queue cla_tcp_single_get_tx_queue(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	// For single-connection CLAs, these parameters are unused...
	(void)eid;
	(void)cla_addr;

	struct cla_tcp_link *const link =
		((struct cla_tcp_single_config *)config)->link;

	// No active link!
	// Currently, no reconnect in case of active TCP variant is supported.
	if (!link)
		return (struct cla_tx_queue){ NULL, NULL };

	hal_semaphore_take_blocking(link->base.tx_queue_sem);

	// Freed while trying to obtain it
	if (!link->base.tx_queue_handle)
		return (struct cla_tx_queue){ NULL, NULL };

	return (struct cla_tx_queue){
		.tx_queue_handle = link->base.tx_queue_handle,
		.tx_queue_sem = link->base.tx_queue_sem,
	};
}

enum upcn_result cla_tcp_single_start_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	// STUB - UNUSED
	(void)config;
	(void)eid;
	(void)cla_addr;

	return UPCN_OK;
}

enum upcn_result cla_tcp_single_end_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	// STUB - UNUSED
	(void)config;
	(void)eid;
	(void)cla_addr;

	return UPCN_OK;
}

enum upcn_result parse_tcp_active(const char *str, bool *tcp_active)
{
	if (!strcmp(str, CLA_OPTION_TCP_ACTIVE))
		*tcp_active = true;
	else if (!strcmp(str, CLA_OPTION_TCP_PASSIVE))
		*tcp_active = false;
	else
		return UPCN_FAIL;

	return UPCN_OK;
}
