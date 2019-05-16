#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define SERIAL_READ_TIMEOUT 200
#define MAX_SND_BYTES (1024 * 1024)
#define MAX_RCV_BYTES (1024 * 1024)

/* uPCN Message Format, see upcn/src/comm.c */
static const uint8_t DATA_DELIMITER = 0x0A;
static const uint8_t BEGIN_MARKER = 0x42;

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

static int wait_for_device(int *dev, const char *path);
static enum pollresult poll_both(int rdev, void *subscriber);
static int receive_data(int rdev, void *publisher);
static int send_data(int wdev, void *subscriber);
static ssize_t blocking_write(int fd, const void *buf, size_t count);
static inline void fprintu8s(FILE *stream, uint8_t u8);

int main(int argc, char *argv[])
{
	int rc, dev = -1, linger = 100;
	void *context, *publisher, *subscriber;
	enum pollresult pr;

	if (argc != 4) {
		fprintf(stderr, "Usage: upcn_connect <device> <pub_sock> " \
			"<rep_sock>\n");
		return EXIT_FAILURE;
	}

	/* Test readability of device */
	if (access((char *)argv[1], R_OK | W_OK) != 0) {
		perror("access()");
		fprintf(stderr, "Failed to access communication device\n");
		return EXIT_FAILURE;
	}

	/* Initialize ZeroMQ: Publisher and Subscriber */
	context = zmq_ctx_new();
	publisher = zmq_socket(context, ZMQ_PUB);
	zmq_setsockopt(publisher, ZMQ_LINGER, &linger, sizeof(int));
	rc = zmq_bind(publisher, argv[2]);
	if (rc != 0) {
		perror("zmq_bind()");
		fprintf(stderr, "Could not bind to publisher (PUB) socket\n");
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
		zmq_close(publisher);
		zmq_close(subscriber);
		zmq_ctx_destroy(context);
		return EXIT_FAILURE;
	}

	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, &exit_handler);
	signal(SIGTERM, &exit_handler);
	signal(SIGHUP, &exit_handler);

	do {
		if (wait_for_device(&dev, (char *)argv[1]) != EXIT_SUCCESS)
			break;
		printf("Device opened successfully\n");
		rc = EXIT_SUCCESS;
		pr = PR_NONE;
		while (pr != PR_ERROR && rc == EXIT_SUCCESS && !exiting) {
			pr = poll_both(dev, subscriber);
			if (pr == PR_FILE)
				rc = receive_data(dev, publisher);
			else if (pr == PR_SOCKET)
				rc = send_data(dev, subscriber);
		}
	} while (!exiting);

	if (dev >= 0)
		close(dev);
	zmq_close(publisher);
	zmq_close(subscriber);
	zmq_ctx_destroy(context);
	return exiting == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int wait_for_device(int *dev, const char *path)
{
	struct timespec req1 = { 0, 1000000 * 20 }; /* 20 ms */
	struct termios opts;

	if (*dev >= 0) {
		printf("Reconnecting...\n");
		close(*dev);
		*dev = -1;
	}
	/* Try to open POSIX file descriptors */
	do {
		/* We use O_NONBLOCK for read and implement our own blocking */
		*dev = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
		nanosleep(&req1, NULL);
		if (exiting)
			return EXIT_FAILURE;
	} while (*dev < 0);
	/* Enable exclusive mode */
	if (ioctl(*dev, TIOCEXCL) != 0) {
		perror("ioctl()");
		fprintf(stderr, "Failed to set exclusive mode\n");
		return EXIT_FAILURE;
	}
	/* Set correct attributes on opened file descriptor */
	if (tcgetattr(*dev, &opts) != 0) {
		perror("tcgetattr()");
		fprintf(stderr, "Failed to get term attributes\n");
		return EXIT_FAILURE;
	}
	/* Set some baud rate (may be removed, this is not used for USB CDC) */
	cfsetispeed(&opts, B115200);
	cfsetospeed(&opts, B115200);
	/* Use RAW mode */
	cfmakeraw(&opts);
	/* Ignore parity errors */
	opts.c_iflag &= ~IGNPAR;
	/* Disable all SW flow control also for input */
	opts.c_iflag &= ~(IXOFF | IXANY);
	/* Unset out NL/CR processing */
	opts.c_oflag &= ~(ONLCR | ONLCR);
	/* Ignore modem ctrl lines */
	opts.c_cflag |= CLOCAL;
	/* One stop bit and no HW flow control */
	opts.c_cflag &= ~(CSTOPB | CRTSCTS);
	/* Timeouts and Blocking - we use poll, so this should not be needed */
	opts.c_cc[VMIN]  = 1; /* read returns when N chars are available */
	opts.c_cc[VTIME] = 0; /* 0.N seconds read timeout */
	/* We do not reset special chars (EOF, ...) here - icanon is disabled */
	/* Apply options */
	if (tcsetattr(*dev, TCSANOW, &opts) != 0) {
		perror("tcsetattr()");
		fprintf(stderr, "Failed to set term attributes\n");
		return EXIT_FAILURE;
	}
	/* Flush output buffers */
	if (tcflush(*dev, TCOFLUSH) != 0) {
		perror("tcflush()");
		fprintf(stderr, "Failed to flush device\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

static enum pollresult poll_both(int rdev, void *subscriber)
{
	zmq_pollitem_t poll_items[2];

	poll_items[0].socket = subscriber;
	poll_items[0].events = ZMQ_POLLIN;
	poll_items[1].socket = NULL;
	poll_items[1].fd = rdev;
	poll_items[1].events = ZMQ_POLLIN | ZMQ_POLLERR;
	if (zmq_poll(poll_items, 2, -1) > 0) {
		if ((poll_items[1].revents & ZMQ_POLLERR) != 0)
			return PR_ERROR;
		else if (poll_items[1].revents)
			return PR_FILE;
		else
			return PR_SOCKET;
	}
	return PR_ERROR;
}

static int receive_data(int rdev, void *publisher)
{
	static uint8_t sendbuffer[MAX_RCV_BYTES + 3];

	int ret = 0;
	uint8_t buffer, datatype;
	size_t datalength, dataremain, dataindex;
	struct pollfd pfd = {
		.fd = rdev,
		.events = POLLIN,
		.revents = 0
	};
	enum {
		ST_EXPECT_DATA_DELIMITER,
		ST_EXPECT_BEGIN_MARKER,
		ST_EXPECT_TYPE,
		ST_EXPECT_LENGTH_MSB,
		ST_EXPECT_LENGTH_LSB,
		ST_EXPECT_DATA
	} st = ST_EXPECT_DATA_DELIMITER;

	dataindex = 0;
	dataremain = 1;
	datalength = 0;
	datatype = 0xFF;
	while (dataremain && poll(&pfd, 1, st == ST_EXPECT_DATA_DELIMITER
			? 0 : SERIAL_READ_TIMEOUT) > 0) {
		if (pfd.revents & POLL_ERRMASK)
			break;
		ret = read(rdev, &buffer, 1);
		if (ret <= 0) {
			if (ret == 0 || errno == EWOULDBLOCK || errno == EAGAIN)
				continue;
			break;
		}
		ret = 0;
		switch (st) {
		case ST_EXPECT_DATA_DELIMITER:
			if (buffer == DATA_DELIMITER)
				st = ST_EXPECT_BEGIN_MARKER;
			break;
		case ST_EXPECT_BEGIN_MARKER:
			if (buffer == BEGIN_MARKER)
				st = ST_EXPECT_TYPE;
			else if (buffer != DATA_DELIMITER)
				st = ST_EXPECT_DATA_DELIMITER;
			break;
		case ST_EXPECT_TYPE:
			datatype = buffer;
			if (buffer != 0x00 && buffer < 0x10)
				st = ST_EXPECT_LENGTH_MSB;
			else if (buffer == DATA_DELIMITER)
				st = ST_EXPECT_BEGIN_MARKER;
			else if (buffer != BEGIN_MARKER)
				st = ST_EXPECT_DATA_DELIMITER;
			break;
		case ST_EXPECT_LENGTH_MSB:
			datalength = (size_t)buffer << 8;
			st = ST_EXPECT_LENGTH_LSB;
			break;
		case ST_EXPECT_LENGTH_LSB:
			datalength |= (size_t)buffer;
			if (datalength > MAX_RCV_BYTES) {
				st = ST_EXPECT_DATA_DELIMITER;
				break;
			}
			dataremain = datalength;
			snprintf((char *)sendbuffer, 4, "%02X ", datatype);
			dataindex = 3;
			st = ST_EXPECT_DATA;
			break;
		case ST_EXPECT_DATA:
			sendbuffer[dataindex++] = buffer;
			--dataremain;
			break;
		}
	}

	if (dataremain) {
		if (ret < 0)
			perror("read()");
		fprintf(stderr, "Data dropped: type = 0x%02X, remain = %zu, " \
			"state = %d, rc = %d (%s)\n",
			datatype, dataremain, st, ret,
			(ret < 0) ? "read error" : "polling error/timeout");
		if (dataindex > 0) {
			fprintf(stderr, "> Str: \"");
			datalength = dataindex;
			for (dataindex = 0; dataindex < datalength; dataindex++)
				fprintu8s(stderr, sendbuffer[dataindex]);
			fprintf(stderr, "\"\n");
		}
		if (ret == 0 || errno == EWOULDBLOCK || errno == EAGAIN)
			return EXIT_SUCCESS;
		return EXIT_FAILURE;
	}

	zmq_send(publisher, sendbuffer, datalength + 3, 0);
	return EXIT_SUCCESS;
}

static int send_data(int wdev, void *subscriber)
{
	static uint8_t recvbuffer[MAX_SND_BYTES];
	static uint8_t replybuffer[3] = { 0xFF, 0x42, 0x00 };
	int len;

	len = zmq_recv(subscriber, recvbuffer, MAX_SND_BYTES, ZMQ_DONTWAIT);
	if (len < 0)
		return EXIT_SUCCESS;
	if (zmq_send(subscriber, replybuffer, sizeof(replybuffer), 0) < 0)
		fprintf(stderr, "Failed sending reply to received cmd\n");
	if (blocking_write(wdev, recvbuffer, len) < 0) {
		fprintf(stderr, "Serial write failed\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

static ssize_t blocking_write(int fd, const void *buf, size_t count)
{
	const char *buf_c = (const char *)buf;
	struct pollfd pfd = {
		.fd = fd,
		.events = POLLOUT,
		.revents = 0
	};
	ssize_t bytes;

	while (count) {
		if (poll(&pfd, 1, -1) <= 0 || (pfd.revents & POLL_ERRMASK))
			return -1;
		bytes = write(fd, buf_c, count);
		if (bytes < 0) {
			if (errno == EWOULDBLOCK || errno == EAGAIN)
				bytes = 0;
			else
				return bytes;
		}
		count -= bytes;
		buf_c += bytes;
	}
	return buf_c - (const char *)buf;
}

static inline void fprintu8s(FILE *stream, uint8_t u8)
{
	if (u8 < 128 && isprint((char)u8) && (char)u8 != '\\') {
		fprintf(stream, "%c", (char)u8);
	} else {
		switch (u8) {
		case '\\':
			fprintf(stream, "\\\\");
			break;
		case '\n':
			fprintf(stream, "\\n");
			break;
		case '\r':
			fprintf(stream, "\\r");
			break;
		case '\t':
			fprintf(stream, "\\t");
			break;
		case '\0':
			fprintf(stream, "\\0");
			break;
		default:
			fprintf(stream, "\\x%02hhx", u8);
			break;
		}
	}
}
