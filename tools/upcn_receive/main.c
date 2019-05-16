#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>

#include <zmq.h>

#include <upcn/hal/hal_io.h>

#define BUF_SIZE (1024 * 1024 + 3)
#define MAX_DATA_PREVIEW 32
#define F_DELIMITER "\0Type: "

static FILE *file;
static void *ctx, *sub;

static void sigint_handler(int sig)
{
	zmq_close(sub);
	zmq_ctx_destroy(ctx);
	if (file != NULL)
		fclose(file);
}

static void makeprint(char *const buf, const int len)
{
	int i;

	for (i = 3; i < len + 3; i++) {
		if (!isprint(buf[i]) && buf[i] != '\n') {
			if (buf[i] >= 0 && buf[i] <= 9)
				buf[i] += '0';
			else
				buf[i] = '?';
		}
	}
}

static void to_file(const char *const buf, const int length, const char type)
{
	if (file != NULL) {
		fwrite(F_DELIMITER, 1, sizeof(F_DELIMITER), file);
		fputc('0' + type, file);
		fputc('\0', file);
		fwrite(buf, 1, length, file);
	}
}

int main(int argc, char *argv[])
{
	const int LINGER = 0;
	static char buf[BUF_SIZE];
	int r;
	enum comm_type t;

	if (argc < 2 || argc > 3) {
		printf("Usage: upcn_receive <address> [data-out]\n");
		return EXIT_FAILURE;
	}

	if (argc == 3) {
		file = fopen(argv[2], "w");
		if (!file) {
			perror("fopen()");
			fprintf(stderr, "Could not open output file!\n");
			return EXIT_FAILURE;
		}
	}

	ctx = zmq_ctx_new();
	sub = zmq_socket(ctx, ZMQ_SUB);
	zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "", 0);
	zmq_setsockopt(sub, ZMQ_LINGER, &LINGER, sizeof(int));
	if (zmq_connect(sub, argv[1]) != 0) {
		perror("zmq_connect()");
		fprintf(stderr, "Could not connect!\n");
		return EXIT_FAILURE;
	}

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	for (;;) {
		r = zmq_recv(sub, buf, BUF_SIZE, 0);
		if (r == -1)
			break;
		if (r <= 3)
			continue;
		buf[r] = '\0';
		t = (enum comm_type)(
			buf[1] >= 'a' ? buf[1] - 'a' + 10 : buf[1] - '0');
		to_file(buf + 3, r - 3, t);
		switch (t) {
		case COMM_TYPE_MESSAGE:
			makeprint(buf, r - 3);
			fputs(buf + 3, stdout);
			break;
		case COMM_TYPE_BUNDLE:
			printf("*** Bundle received, l = %d\n", r - 3);
			break;
		case COMM_TYPE_BEACON:
			printf("*** Beacon received, l = %d\n", r - 3);
			break;
		case COMM_TYPE_ECHO:
			makeprint(buf, r - 3);
			printf("*** Echo packet received: %s\n", buf + 3);
			break;
		case COMM_TYPE_GS_INFO:
			printf("*** GS data received, l = %d\n", r - 3);
			break;
		case COMM_TYPE_PERF_DATA:
			printf("*** Perf. data received, l = %d\n", r - 3);
			break;
		case COMM_TYPE_RRND_STATUS:
			printf("*** RRND status received: 0x%02hhx%02hhx\n",
				buf[4], buf[3]);
			break;
		case COMM_TYPE_GENERIC_RESULT:
			if (r - 3 < 1)
				break;
			printf("*** Result received: %02x\n", buf[3]);
			break;
		case COMM_TYPE_CONTACT_STATE:
			if (r - 3 < 4)
				break;
			makeprint(buf + 1, r - 4);
			printf("*** Contact state received: %s - %s\n",
			       &buf[4], buf[3] ? "START" : "END");
			break;
		default:
			makeprint(buf, ((r - 3) < MAX_DATA_PREVIEW)
				? (r - 3) : MAX_DATA_PREVIEW);
			buf[MAX_DATA_PREVIEW + 3] = '.';
			buf[MAX_DATA_PREVIEW + 4] = '.';
			buf[MAX_DATA_PREVIEW + 5] = '.';
			buf[MAX_DATA_PREVIEW + 6] = '\0';
			printf("*** Data (%02x) received, l = %d: %s\n",
				t, r - 3, buf + 3);
			break;
		}
	}
	return EXIT_SUCCESS;
}
