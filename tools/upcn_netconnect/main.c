#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include <zmq.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>


#define SERIAL_READ_TIMEOUT 200
#define MAX_SND_BYTES (100000000)
#define MAX_RCV_BYTES (100000000)

#define POLL_ERRMASK (POLLERR | POLLHUP | POLLNVAL)

enum pollresult {
	PR_NONE,
	PR_ERROR,
	PR_FILE,
	PR_SOCKET
};

static int exiting;
static void exit_handler(int sig)
{
	(void)sig;
	exiting = 1;
}

static enum pollresult poll_both(void *tcp_socket, void *subscriber);
static int receive_data(void *tcp_socket, void *publisher);
static int send_data(void *tcp_socket, void *subscriber);

static uint8_t id[256];
static size_t id_size = 256;

int main(int argc, char *argv[])
{
	int rc, linger = 100;
	void *context, *publisher, *subscriber;
	enum pollresult pr;

	if (argc != 4) {
		fprintf(stderr, "Usage: upcn_connect <url device>" \
			" <pub_sock> <rep_sock>\n");
		return EXIT_FAILURE;
	}

	/* create ZeroMQ context */
	context = zmq_ctx_new();
	assert(context);

	/* Create a ZMQ_STREAM socket */
	void *tcp_socket = zmq_socket(context, ZMQ_STREAM);

	assert(tcp_socket);
	rc = zmq_connect(tcp_socket, (char *)argv[1]);
	if (rc != 0) {
		perror("zmq_connect()");
		fprintf(stderr, "Could not connect to the device\n");
		zmq_close(tcp_socket);
		zmq_ctx_destroy(context);
		return EXIT_FAILURE;
	}

	/* get the socket identity (necessary for sending) */
	rc = zmq_getsockopt(tcp_socket, ZMQ_IDENTITY, id, &id_size);
	assert(rc == 0);

	/* Initialize ZeroMQ: Publisher and Subscriber */
	publisher = zmq_socket(context, ZMQ_PUB);
	zmq_setsockopt(publisher, ZMQ_LINGER, &linger, sizeof(int));
	rc = zmq_bind(publisher, argv[2]);
	if (rc != 0) {
		perror("zmq_bind()");
		fprintf(stderr, "Could not bind to publisher (PUB) socket\n");
		zmq_close(tcp_socket);
		zmq_close(publisher);
		zmq_ctx_destroy(context);
		return EXIT_FAILURE;
	}
	subscriber = zmq_socket(context, ZMQ_REP);
	zmq_setsockopt(subscriber, ZMQ_LINGER, &linger, sizeof(int));
	rc = zmq_bind(subscriber, argv[3]);
	if (rc != 0) {
		perror("zmq_connect()");
		fprintf(stderr, "Could not bind to reply (REP) socket\n");
		zmq_close(tcp_socket);
		zmq_close(publisher);
		zmq_close(subscriber);
		zmq_ctx_destroy(context);
		return EXIT_FAILURE;
	}

	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, &exit_handler);
	signal(SIGTERM, &exit_handler);
	signal(SIGHUP, &exit_handler);

	printf("Initialisation finished successfully!\n");

	rc = EXIT_SUCCESS;
	pr = PR_NONE;
	while (pr != PR_ERROR && rc == EXIT_SUCCESS && !exiting) {
		pr = poll_both(tcp_socket, subscriber);
		if (pr == PR_FILE)
			rc = receive_data(tcp_socket, publisher);
		else if (pr == PR_SOCKET)
			rc = send_data(tcp_socket, subscriber);
	}

	zmq_close(tcp_socket);
	zmq_close(publisher);
	zmq_close(subscriber);
	zmq_ctx_destroy(context);
	return exiting == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static enum pollresult poll_both(void *tcp_socket, void *subscriber)
{
	zmq_pollitem_t poll_items[2];

	poll_items[0].socket = subscriber;
	poll_items[0].events = ZMQ_POLLIN;
	poll_items[1].socket = tcp_socket;
	poll_items[1].events = ZMQ_POLLIN;
	if (zmq_poll(poll_items, 2, -1) > 0) {
		if (poll_items[1].revents)
			return PR_FILE;
		else
			return PR_SOCKET;
	}
	return PR_ERROR;
}

static size_t get_embedded_bundle_size(char *bundle_ptr)
{
	size_t length1, length2, msg_emb_length;
	char *size_ptr;

	/* extract the msg size */
	size_ptr = bundle_ptr+3;
	length1 = *size_ptr;
	size_ptr = bundle_ptr+4;
	length2 = *size_ptr;

	/* calculate length */
	msg_emb_length = (length1 << 8) & 0xFF00;
	msg_emb_length += length2 & 0x00FF;

	return msg_emb_length;
}

static char *modify_header(char *str_ptr)
{

	char *type_ptr;
	char type;
	char type_str[4];

	/* extract type from data
	 * and write it to the necessary position
	 */
	type_ptr = str_ptr+2;
	type = *type_ptr;

	/* generate a string representation of the data type */
	snprintf((char *)&type_str, 4, "%02X ", type);

	/* write the generated string representation to the data */
	type_ptr = ((char *)str_ptr)+2;

	memcpy(type_ptr, &type_str, 3);

	return type_ptr;
}

static int receive_data(void *tcp_socket, void *publisher)
{
	zmq_msg_t msg;
	int rc;
	size_t msg_size;
	static char sndbuffer[MAX_RCV_BYTES];
	static char *cur_buffer_pos;
	static size_t buffered_bundle_size;
	static size_t rem_buffered_bundle_size;

	/* throw the first fragment away -> zmq-internal data */
	rc = zmq_msg_init(&msg);
	assert(rc == 0);
	rc = zmq_recvmsg(tcp_socket, &msg, ZMQ_DONTWAIT);
	assert(rc != -1);

	/* the second fragment contains the data */
	rc = zmq_msg_init(&msg);
	assert(rc == 0);
	rc = zmq_recvmsg(tcp_socket, &msg, ZMQ_DONTWAIT);
	assert(rc != -1);
	msg_size = zmq_msg_size(&msg);

	char *cur_pos;
	size_t cur_size;

	cur_pos = zmq_msg_data(&msg);

	if (buffered_bundle_size != 0) {

		if (rem_buffered_bundle_size > msg_size) {
			/*
			 * bundle is larger than paket -> copy to buffer
			 * and read next paket
			 */
			memcpy(cur_buffer_pos, cur_pos, msg_size);
			rem_buffered_bundle_size -= msg_size;
			cur_buffer_pos += msg_size;
			return EXIT_SUCCESS;
		}

		/*
		 * bundle is smaller than paket -> copy to buffer
		 * and transmit to receiver
		 */

		memcpy(cur_buffer_pos,
		       cur_pos,
		       rem_buffered_bundle_size);

		/* transmit the bundle */
		zmq_send(publisher,
			 modify_header(&sndbuffer[0]),
			 buffered_bundle_size,
			 0);

		msg_size -= rem_buffered_bundle_size;
		cur_pos += rem_buffered_bundle_size;

		/* reset the variables */
		buffered_bundle_size = 0;
		rem_buffered_bundle_size = 0;
		cur_buffer_pos = &sndbuffer[0];

		if (rem_buffered_bundle_size >= msg_size)
			return EXIT_SUCCESS;
	}

	if (msg_size <= 5) {
		/* nothing to do here */
		return EXIT_SUCCESS;
	}


	/* check if multiple bundles have been sent at once */
	while (msg_size > 0) {

		cur_size = get_embedded_bundle_size(cur_pos);

		if ((cur_size + 3) >= msg_size) {
			memcpy(&sndbuffer[0], cur_pos, msg_size);
			buffered_bundle_size = cur_size + 3 - msg_size;
			rem_buffered_bundle_size = buffered_bundle_size;
			cur_buffer_pos = &sndbuffer[0] + cur_size;

			zmq_msg_close(&msg);
			return EXIT_SUCCESS;
		}

		/* send the modified data */
		zmq_send(publisher, modify_header(cur_pos), cur_size+3, 0);

		cur_pos += (cur_size + 5);
		msg_size -= (cur_size + 5);
	}

	zmq_msg_close(&msg);
	return EXIT_SUCCESS;
}

static int send_data(void *tcp_socket, void *subscriber)
{
	static uint8_t recvbuffer[MAX_SND_BYTES];
	static uint8_t replybuffer[3] = { 0xFF, 0x42, 0x00 };
	zmq_msg_t identity, msg;
	int len, rc;

	len = zmq_recv(subscriber, recvbuffer, MAX_SND_BYTES, ZMQ_DONTWAIT);

	if (len <= 0)
		return EXIT_SUCCESS;
	if (zmq_send(subscriber, replybuffer, sizeof(replybuffer), 0) < 0)
		fprintf(stderr, "Failed sending reply to received cmd\n");

	zmq_msg_init_data(&identity, &id, id_size, NULL, NULL);
	zmq_msg_init_data(&msg, &recvbuffer, len, NULL, NULL);
	rc = zmq_sendmsg(tcp_socket, &identity, ZMQ_SNDMORE);
	assert(rc != -1);
	rc = zmq_sendmsg(tcp_socket, &msg, ZMQ_SNDMORE);
	assert(rc != -1);

	return EXIT_SUCCESS;
}
