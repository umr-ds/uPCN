/* System includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <zmq.h>

/* Test includes */
#include <tools.h>
#include <test.h>
#include <upcnTest.h>
#include <upcn/eidManager.h>

static void *ctx, *pub, *sub;

static int run_test(const char *test_name,
	void (*run)(void), void (*init)(void), void (*cleanup)(void))
{
	if (test_comm_broken())
		return 0;
	printf("Running: %s\n", test_name);
	struct test_def def = {
		.run = run,
		.init = init,
		.cleanup = cleanup
	};
	return (test_run(def) == TEST_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Tests */

/* Routing Test */
void rt_run(void);
void rt_init(void);
void rt_cleanup(void);

/* Load Test */
void blt_run(void);
void blt_init(void);
void blt_cleanup(void);

/* DoS Test */
void bdt_run(void);
void bdt_init(void);
void bdt_cleanup(void);

/* Frag Test */
void bft_run(void);
void bft_init(void);
void bft_cleanup(void);

/* Prob. Routing Test */
void prt_run(void);
void prt_init(void);
void prt_cleanup(void);

/* "Simple Bundle Test" */
void sbt_run(void);
void sbt_init(void);
void sbt_cleanup(void);

/* DoS Test 2 */
void bdt2_run(void);
void bdt2_init(void);
void bdt2_cleanup(void);

/* IPND DoS Test */
void idt_run(void);
void idt_init(void);
void idt_cleanup(void);

/* Performance Test */
void pt_run(void);
void pt_init(void);
void pt_cleanup(void);

/* Throughput Test */
void tt_run(void);
void tt_init(void);
void tt_cleanup(void);

static int run_numbered_test(int test)
{
	switch (test) {
	case 0:
		return run_test("Routing Test", &rt_run, &rt_init, &rt_cleanup);
	case 1:
		return run_test("Load Test", &blt_run, &blt_init, &blt_cleanup);
	case 2:
		return run_test("Frag Test", &bft_run, &bft_init, &bft_cleanup);
	case 3:
		return run_test("Prob Test", &prt_run, &prt_init, &prt_cleanup);
	case 4:
		return run_test("DoS Test", &bdt_run, &bdt_init, &bdt_cleanup);
	case 5:
		return run_test("SB Test", &sbt_run, &sbt_init, &sbt_cleanup);
	case 6:
		return run_test("DoS Test 2 (measured)",
			&bdt2_run, &bdt2_init, &bdt2_cleanup);
	case 7:
		return run_test("IDoS Test", &idt_run, &idt_init, &idt_cleanup);
	case 8:
		return run_test("Perf Test", &pt_run, &pt_init, &pt_cleanup);
	case 9:
		return run_test("Throughput Test",
				&tt_run, &tt_init, &tt_cleanup);
	default:
		return 0;
	}
}

#define TEST_COUNT 10
static char *test_names[TEST_COUNT] = {
	"routing",
	"load",
	"frag",
	"prob",
	"dos",
	"sbt",
	"dos2",
	"ipnddos",
	"perf",
	"throughput"
};

/* Runs the "test" command */
static int testcmd(int argc, char *argv[])
{
	int i;
	int found = 0, success = 0;

	if (argc != 0) {
		for (i = 0; i < TEST_COUNT; i++) {
			if (striequal(argv[0], test_names[i])) {
				success = run_numbered_test(i) == EXIT_SUCCESS;
				printf("Test %s.\n",
					success
					? "was successful" : "has failed");
				found = 1;
				break;
			}
		}
	}
	if (!found) {
		printf("Possible test names:\n");
		for (i = 0; i < TEST_COUNT; i++)
			printf("  %s\n", test_names[i]);
	}

	return (found && success) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Other commands are also handled using a "test" */

static void usage(void);

static int run_cmdtest(enum upcntest_command cmd, char *cmdname,
	int argc, char *argv[])
{
	upcntest_setparam(cmd, argc, argv);
	return run_test(cmdname, &upcntest_run,
		&upcntest_init, &upcntest_cleanup);
}

#define TESTCMD_COUNT 17
static char *testcmd_names[TESTCMD_COUNT] = {
	"check",
	"reset",
	"query",
	"rquery",
	"resettime",
	"resetstats",
	"storetrace",
	"cleartraces",
	"bundle",
	"beacon",
	"mkgs",
	"rmgs",
	"mkct",
	"rmct",
	"mkep",
	"rmep",
	"check-unit"
};

static int process_cmd(int argc, char *argv[])
{
	char *cmd = argv[0];
	int i;

	if (striequal(cmd, "test"))
		return testcmd(argc - 1, argv + 1);
	for (i = 0; i < TESTCMD_COUNT; i++) {
		if (striequal(cmd, testcmd_names[i]))
			return run_cmdtest((enum upcntest_command)(i + 1),
				testcmd_names[i], argc, argv);
	}
	usage();
	return EXIT_FAILURE;
}

/* Main functions */

static void usage(void)
{
	printf("Usage: upcn_test <sub> <pub> [commands] [...]\n\n"
		"Possible commands and parameters (<mandatory>/[optional]):\n"
		"  check                        | check connectivity to uPCN\n"
		"  reset                        | hard-reset uPCN\n"
		"  query                        | send query command\n"
		"  rquery                       | send router query command\n"
		"  resettime                    | send reset-time command\n"
		"  resetstats                   | send reset-stats (CPU, MEM)\n"
		"  storetrace                   | store the current CPU stats\n"
		"  cleartraces                  | empty stored CPU stats\n"
		"  bundle [dest] [size] [frag?] | send bundle\n"
		"  beacon [from]                | send beacon\n"
		"  mkgs <name> [cla]            | create ground station\n"
		"  mkct <gs> <from> [to] [rate] | create contact\n"
		"  rmgs <name>                  | remove ground station\n"
		"  rmct <gs> <from> [to]        | remove contact\n"
		"  mkep <gs> <eid>              | add endpoint to gs\n"
		"  rmep <gs> <eid>              | remove endpoint from gs\n"
		"  test [name]                  | run pre-defined tests\n"
	);

}

static void exit_handler(int sig);

int main(int argc, char *argv[])
{
	const int TIMEOUT = 2000, LINGER = 0;
	int result;

	if (argc < 3) {
		usage();
		return EXIT_FAILURE;
	}

	ctx = zmq_ctx_new();
	sub = zmq_socket(ctx, ZMQ_SUB);
	zmq_setsockopt(sub, ZMQ_RCVTIMEO, &TIMEOUT, sizeof(int));
	zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "", 0);
	if (zmq_connect(sub, argv[1]) != 0) {
		perror("zmq_connect()");
		fprintf(stderr, "Could not connect subscriber!\n");
		zmq_close(sub);
		return EXIT_FAILURE;
	}
	pub = zmq_socket(ctx, ZMQ_REQ);
	zmq_setsockopt(pub, ZMQ_SNDTIMEO, &TIMEOUT, sizeof(int));
	zmq_setsockopt(pub, ZMQ_RCVTIMEO, &TIMEOUT, sizeof(int));
	zmq_setsockopt(pub, ZMQ_LINGER, &LINGER, sizeof(int));
	if (zmq_connect(pub, argv[2]) != 0) {
		perror("zmq_connect()");
		fprintf(stderr, "Could not connect request socket!\n");
		zmq_close(sub);
		zmq_close(pub);
		return EXIT_FAILURE;
	}

	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, &exit_handler);
	signal(SIGTERM, &exit_handler);
	signal(SIGHUP, &exit_handler);

	test_init(pub, sub);
	srand(time(NULL));
	eidmanager_init();

	if (argc == 3) {
		usage();
		result = EXIT_FAILURE;
	} else {
		result = process_cmd(argc - 3, argv + 3);
	}

	zmq_close(sub);
	zmq_close(pub);
	zmq_ctx_destroy(ctx);
	return result;
}

/* Handler for exiting the program */
static void exit_handler(int sig)
{
	zmq_close(sub);
	zmq_close(pub);
	zmq_ctx_destroy(ctx);
	exit(EXIT_SUCCESS);
}
