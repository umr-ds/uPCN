#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <assert.h>

bool timeout;
FILE *dump_fp;

/*
 * argp stuff
 */

/* details about the program */
const char *argp_program_version = "pptt 0.1";
const char *argp_program_bug_address = "<robert.wiewel@tu-dresden.de>";
static const char doc[] = "pptt -- a lightweight posix performance test tool";

/* A description of the arguments we accept. */
static const char args_doc[] = "PROCESS_NAME";

/* options vector */
static struct argp_option options[] = {
	{"verbose", 'v', 0, 0,
	"Produce verbose output", 0 },
	{"quiet", 'q', 0, 0,
	 "Don't produce any output", 0 },
	{"filename", 'n', "FILENAME", 0,
	 "Use FILENAME instead of generated name. Max. 255 chars", 0 },
	{"force", 'f', 0, 0,
	 "Overwrite FILENAME if it already exists", 0 },
	{"pid", 'p', "PID", 0,
	 "Use PID to identify process instead of PROCESS_NAME", 0 },
	{"timeout", 't', "TIMEOUT", 0,
	 "Abort testing after TIMEOUT seconds.\nDefault: 300 s", 0 },
	{"interval", 'i', "INTERVAL", 0,
	 "Set probing interval to INTERVAL seconds. Default: 10 s", 0 }
};

/* arguments to get the information from the parser to main */
struct arguments {
	char *filename;
	char *process_name;
	bool force, verbose, silent;
	int pid;
	int timeout;
	int interval;
};

/* program arguments */
struct arguments arguments;

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;
	char bad_chars[] = "!@%^*~|";
	unsigned int i;
	bool inval_char = false;

	switch (key) {
	case 'f':
		arguments->force = true;
		break;
	case 'n':
		arguments->filename = (char *)arg;

		if (strlen(arguments->filename) > 255) {
			printf("Filename too long! 255 chars max!\n");
			argp_usage(state);
		}

		for (i = 0; i < strlen(bad_chars); ++i) {
			if (strchr(arguments->filename, bad_chars[i]) != NULL) {
				inval_char = true;
				break;
			}
		}

		if (inval_char) {
			printf("Invalid character in filename!\n");
			argp_usage(state);
		}
		break;

	case 'p':
		arguments->pid = atoi(arg);
		break;

	case 't':
		arguments->timeout = atoi(arg);
		break;

	case 'v':
		arguments->verbose = true;
		break;

	case 'q':
		arguments->silent = true;
		break;

	case 'i':
		arguments->interval = atoi(arg);
		break;

	case ARGP_KEY_ARG:
		if (state->arg_num >= 1)
			/* Too many arguments. */
			argp_usage(state);

		arguments->process_name = arg;

		break;

	case ARGP_KEY_END:
		if (arguments->process_name == NULL && arguments->pid == 0)
			/* Not enough arguments. */
			argp_usage(state);
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

/* instance of parser */
static struct argp argp = { options, parse_opt, args_doc, doc, 0, NULL, NULL };

/* static variables */
static int timeout_intervals;
static int target_pid;


void plog(bool verbose, const char *fmt, ...)
{
	if (!arguments.silent) {
		if (!verbose || arguments.verbose) {
			va_list args;

			va_start(args, fmt);
			vprintf(fmt, args);
			va_end(args);
		}
	}
}

int get_pid(char *process_name)
{

	DIR *dir;
	FILE *fp;
	char *path;
	char *data;
	struct dirent *dent;
	int pid;

	dir = opendir("/proc");
	path = malloc(256);
	data = malloc(256);
	pid = -1;

	assert(dir != NULL);

	while ((dent = readdir(dir)) != NULL) {
		if (isdigit(dent->d_name[0])) {

			/* reset path variable */
			path[0] = '\0';
			strncat(path, "/proc/", 6);
			strncat(path, dent->d_name, strlen(dent->d_name));
			strncat(path, "/stat", 5);

			plog(true, "investigating path: %s ",
			     path);

			fp = fopen(path, "r");

			fgets(data, 256, fp);

			if (strstr(data, process_name)) {
				plog(true, "... matching\n");
				plog(true, "detected process: %s\n", data);

				if (pid != -1) {
					plog(false, "More than one " \
					  "process with given name " \
					  "existing. Use pid!");
				}
				pid = atoi(dent->d_name);

				break;
			}

			plog(true, "... no match\n");
			fclose(fp);
		}
	}
	closedir(dir);
	free(path);
	free(data);

	return pid;
}


char *get_stat_process_path(int pid)
{
	char *path;
	char *pid_char;

	path = malloc(256);
	pid_char = malloc(20);

	/* reset path variable */
	path[0] = '\0';
	strncat(path, "/proc/", 6);

	sprintf(pid_char, "%d", pid);
	strncat(path, pid_char, strlen(pid_char));

	strncat(path, "/stat", 5);

	free(pid_char);

	return path;
}

char *get_status_process_path(int pid, char *path_mem)
{
	char *pid_char;

	pid_char = malloc(20);

	/* reset path variable */
	path_mem[0] = '\0';
	strncat(path_mem, "/proc/", 6);

	sprintf(pid_char, "%d", pid);
	strncat(path_mem, pid_char, strlen(pid_char));

	strncat(path_mem, "/status", 7);

	free(pid_char);

	return path_mem;
}

/**
 * @brief write_int_to_file Write a given int value to the (already opened)
 *			      File.
 * @param value The given value
 * @param file_pointer The file that the value should be written to
 * @return wether the action was successful or not
 */
unsigned int write_int_to_file(int value, FILE *file_pointer, bool new_line)
{

	if (file_pointer == NULL)
		return EXIT_FAILURE;

	if (new_line)
		fprintf(file_pointer, "%d\n", value);
	else
		fprintf(file_pointer, "%d;", value);

	return EXIT_SUCCESS;
}

/**
 * @brief write_string_to_file Write a given int value to the (already opened)
 *			      File.
 * @param value The given value
 * @param file_pointer The file that the value should be written to
 * @return wether the action was successful or not
 */
unsigned int write_string_to_file(char *value,
				  FILE *file_pointer,
				  bool new_line)
{
	if (file_pointer == NULL)
		return EXIT_FAILURE;

	if (new_line)
		fprintf(file_pointer, "%s\n", value);
	else
		fprintf(file_pointer, "%s;", value);

	return EXIT_SUCCESS;
}


/**
 * @brief get_stat_value Get a specific value in a given 'stat' string
 * @param stat the 'stat' string
 * @param pos the position of the element within the 'stat' string
 * @return the extracted value
 */
int get_stat_value(char *stat, int pos)
{
	char *char_ptr;
	int i, result;

	i = 1;
	char_ptr = stat;

	while (char_ptr != NULL && i <= pos) {
		char_ptr += 1;

		if (i == pos) {
			result = strtol(char_ptr, NULL, 10);
			return result;
		}

		char_ptr = strpbrk(char_ptr, " ");
		i++;
	}

	return -1;
}

int get_mem_usage(FILE *file_ptr)
{
	char *line;
	char *mem;
	size_t len;
	int result;


	mem = NULL;
	line = malloc(128);
	len = 128;

	while (!mem) {
		if (getline(&line, &len, file_ptr) == -1)
			return 1;

		if (!strncmp(line, "VmRSS:", 6))
			mem = strdup(&line[7]);
	}
	free(line);

	len = strlen(mem);
	mem[len - 4] = 0;

	result = strtol(mem, NULL, 10);

	free(mem);

	/* Success */
	return result;
}

/**
 * Calculates the amount of time the process has been scheduled in both modes
 * - user and kernel mode - in clock ticks.
 *
 * @param file_ptr proc stat file descriptor
 * @param char_ptr output buffer
 * @param nr_chars buffer length
 *
 * @return Returns the amount if time the process has been scheduled measured
 *         in clock ticks
 */
int get_proc_clk_ticks(FILE *file_ptr, char *char_ptr, int nr_chars)
{
	int ticks;

	/* reset stream position indicator */
	if (fseek(file_ptr, 0, SEEK_SET) != 0) {
		printf("Something went wrong when accessing the stat file!\n");
		exit(EXIT_FAILURE);
	}

	/* read nr_chars characters from file */
	fread(char_ptr, 1, nr_chars, file_ptr);

	/* from proc(5):
	 *
	 * (14) utime
	 *      Amount of time that this process has been scheduled
	 *      in user mode, measured in clock ticks
	 *
	 * (15) stime
	 *      Amount of time that this process has been scheduled
	 *      in kernel mode, measured in clock ticks
	 */
	ticks = get_stat_value(char_ptr, 14);
	ticks += get_stat_value(char_ptr, 15);

	return ticks;
}

/**
 * Returns the number of clock ticks elpased in the passed period
 * of time:
 *
 * It is calculated as
 *
 *     (Number of clock ticks per second) * (elapsed time) / 1000
 *
 * @param elapsed_time_ms
 * @return elapsed clock ticks in the given period of time
 */
float calculate_ref_ticks(int elapsed_time_ms)
{
	return (sysconf(_SC_CLK_TCK) * elapsed_time_ms) / 1000.0;
}

int get_processor_usage(int proc_clk_ticks, int ref_clk_ticks)
{
	return (proc_clk_ticks / ref_clk_ticks) * 1000;
}

/**
 * @brief log_process_information Gather all memory information of the given
 *				  process and store it in the file
 * @param dump_pointer The file pointer to the target file
 * @return Whether the operation was successful or not
 */
int log_memory_information(FILE *dump_pointer)
{
	FILE *proc_fp;
	int memory;
	char *path = malloc(256);

	/* open the process stat file */
	proc_fp = fopen(get_status_process_path(target_pid, path), "r");

	if (proc_fp == NULL) {
		plog(false, "status-File could not be opened!\n");

		proc_fp = fopen(get_status_process_path(target_pid, path), "r");
		if (proc_fp == NULL) {
			printf("status-File could not be opened twice" \
			       "! Aborting!\n");
			exit(EXIT_FAILURE);

		}
	}

	/* get the processor usage */
	memory = get_mem_usage(proc_fp);

	fclose(proc_fp);
	free(path);

	/* write info to file */
	write_int_to_file(memory, dump_pointer, false);

	return EXIT_SUCCESS;
}

char *get_task_process_path(int pid, char *path_mem)
{
	char *pid_char;

	pid_char = malloc(20);

	/* reset path variable */
	path_mem[0] = '\0';
	strncat(path_mem, "/proc/", 6);

	sprintf(pid_char, "%d", pid);
	strncat(path_mem, pid_char, strlen(pid_char));

	strncat(path_mem, "/task/", 6);

	free(pid_char);

	return path_mem;
}

int get_thread_processor_usage(FILE *dump_pointer,
			       int pid,
			       char *char_ptr,
			       int nr_chars)
{
	DIR *dir;
	FILE *fp;
	char *path;
	char *data;
	struct dirent *dent;

	static int rout_opt_t_prev;
	static int ground_st_t_prev;
	static int in_t_prev;
	static int cont_man_t_prev;
	static int bundl_proc_t_prev;
	static int router_t_prev;
	static int comm_t_prev;
	static int upcn_t_prev;

	path = malloc(256);

	dir = opendir(get_task_process_path(pid, path));

	if (dir == NULL) {
		plog(false, "error when opening folder!\n");
		free(path);
		return -1;
	}

	data = malloc(256);

	int rout_opt_t = 0;
	int ground_st_t = 0;
	int in_t = 0;
	int cont_man_t = 0;
	int bundl_proc_t = 0;
	int router_t = 0;
	int comm_t = 0;
	int upcn_t = 0;

	while ((dent = readdir(dir)) != NULL) {

		if (strcmp(dent->d_name, ".") == 0 ||
				strcmp(dent->d_name, "..") == 0)
			continue;

		/* reset path variable */
		get_task_process_path(pid, path);

		strncat(path, dent->d_name, strlen(dent->d_name));
		strncat(path, "/stat", 5);

		plog(true, "investigating path: %s\n", path);

		fp = fopen(path, "r");

		/* Failed to open path */
		if (fp == NULL) {
			plog(true, "Could not open path: %s\n", path);
			continue;
		}

		fgets(data, 256, fp);

		if (strstr(data, "rout_opt_t")) {
			rout_opt_t = get_proc_clk_ticks(fp, char_ptr, nr_chars);

		} else if (strstr(data, "ground_st_t")) {
			ground_st_t += get_proc_clk_ticks(fp, char_ptr,
							 nr_chars);

		} else if (strstr(data, "in_t")) {
			in_t = get_proc_clk_ticks(fp, char_ptr, nr_chars);

		} else if (strstr(data, "cont_man_t")) {
			cont_man_t = get_proc_clk_ticks(fp, char_ptr, nr_chars);

		} else if (strstr(data, "bundl_proc_t")) {
			bundl_proc_t = get_proc_clk_ticks(fp,
							  char_ptr,
							  nr_chars);

		} else if (strstr(data, "router_t")) {
			router_t = get_proc_clk_ticks(fp, char_ptr, nr_chars);

		} else if (strstr(data, "comm_t")) {
			comm_t = get_proc_clk_ticks(fp, char_ptr, nr_chars);
		} else if (strstr(data, "upcn")) {
			upcn_t = get_proc_clk_ticks(fp, char_ptr, nr_chars);
		}

		fclose(fp);
	}

	closedir(dir);
	free(path);
	free(data);

	write_int_to_file(rout_opt_t-rout_opt_t_prev, dump_pointer, false);
	write_int_to_file(ground_st_t-ground_st_t_prev, dump_pointer, false);
	write_int_to_file(in_t-in_t_prev, dump_pointer, false);
	write_int_to_file(cont_man_t-cont_man_t_prev, dump_pointer, false);
	write_int_to_file(bundl_proc_t-bundl_proc_t_prev, dump_pointer, false);
	write_int_to_file(router_t-router_t_prev, dump_pointer, false);
	write_int_to_file(comm_t-comm_t_prev, dump_pointer, false);
	write_int_to_file(upcn_t-upcn_t_prev, dump_pointer, true);

	rout_opt_t_prev = rout_opt_t;
	ground_st_t_prev = ground_st_t;
	in_t_prev = in_t;
	cont_man_t_prev = cont_man_t;
	bundl_proc_t_prev = bundl_proc_t;
	router_t_prev = router_t;
	comm_t_prev = comm_t;
	upcn_t_prev = upcn_t;

	return pid;
}


void signalHandler(int sig)
{
	static int counter;

	if (sig == SIGALRM) {
		plog(true, "measurement %d\n", counter++);
		timeout_intervals--;

		if (timeout_intervals <= 0) {
			timeout = true;
			plog(false, "Timeout occured! Will abort now!\n");
		}
	} else if (sig == SIGINT) {
		fclose(dump_fp);
		exit(EXIT_SUCCESS);
	}
}



int main(int argc, char *argv[])
{
	char *filename;
	char *file_buffer;
	struct itimerval timer;

	/* Default values. */
	arguments.silent = 0;
	arguments.verbose = 0;
	arguments.filename = NULL;
	arguments.process_name = NULL;
	arguments.interval = 10;
	arguments.pid = 0;
	arguments.timeout = 300;
	arguments.force = 0;

	/* Parse our arguments; every option seen by parse_opt will
	 * be reflected in arguments.
	 */
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	/* determine pid of running instance
	 * (if not provided as argument)
	 */
	if (arguments.pid == 0) {
		plog(false, "Monitoring process with name %s\n",
		    arguments.process_name);
		target_pid = get_pid(arguments.process_name);
		plog(false, "Determined pid %d matching for %s\n",
		     target_pid, arguments.process_name);
	} else {
		plog(false, "Monitoring process with PID %d\n", arguments.pid);
		target_pid = arguments.pid;
	}

	/* create file to write into */
	if (arguments.filename != NULL) {
		filename = arguments.filename;
	} else {
		/* if no filename is specified, generate filename out of time */
		filename = malloc(256);
		struct timespec spec;

		clock_gettime(CLOCK_REALTIME, &spec);

		snprintf(filename, 255, "%ld", spec.tv_sec);
	}

	plog(false, "Creating file with filename %s\n", filename);

	/* opening file */
	if (arguments.force)
		dump_fp = fopen(filename, "w+");
	else
		dump_fp = fopen(filename, "w+x");

	if (arguments.filename == NULL)
		free(filename);

	if (dump_fp == NULL && errno == EEXIST) {
		plog(false, "File already exists! Choose other name or -f!\n");
		exit(EXIT_FAILURE);
	}

	file_buffer = malloc(100*sizeof(char));

	timeout_intervals = arguments.timeout / arguments.interval;

	/* set timer expiration signal */
	signal(SIGALRM, signalHandler);
	signal(SIGINT, signalHandler);

	/* init file with header */

	write_string_to_file("total cycles", dump_fp, false);
	write_string_to_file("mem usage", dump_fp, false);
	write_string_to_file("rout_opt_t", dump_fp, false);
	write_string_to_file("ground_st_t", dump_fp, false);
	write_string_to_file("in_t", dump_fp, false);
	write_string_to_file("cont_man_t", dump_fp, false);
	write_string_to_file("bundl_proc_t", dump_fp, false);
	write_string_to_file("router_t", dump_fp, false);
	write_string_to_file("comm_t", dump_fp, false);
	write_string_to_file("upcn_t", dump_fp, true);

	/* Configure the timer to expire after the interval... */
	timer.it_value.tv_sec = arguments.interval;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = arguments.interval;
	timer.it_interval.tv_usec = 0;

	/* Start a virtual timer. It counts down whenever this process is
	 * executing.
	 */
	setitimer(ITIMER_REAL, &timer, NULL);

	/* loop */
	while (!timeout) {
		pause();

		/* Number of cycles elapsed in the interval */
		fprintf(dump_fp, "%.2f;",
			calculate_ref_ticks(arguments.interval));

		log_memory_information(dump_fp);

		get_thread_processor_usage(dump_fp,
					   target_pid,
					   file_buffer,
					   100);

	}

	/* if exit signal received, stop gathering data and close file
	 * then exit
	 */

	free(file_buffer);

	fclose(dump_fp);

	return 0;
}
