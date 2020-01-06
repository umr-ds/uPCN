#include "upcn/cmdline.h"
#include "upcn/common.h"
#include "upcn/config.h"
#include "upcn/eid.h"

#include "platform/hal_io.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct upcn_cmdline_options global_cmd_opts;

/**
 * Helper function for parsing a 64-bit unsigned integer from a given C-string.
 */
enum upcn_result parse_uint64(const char *str, uint64_t *result);

const struct upcn_cmdline_options *parse_cmdline(int argc, char *argv[])
{
	// For now, we use a global variable. (Because why not?)
	// Though, this may be refactored easily.
	struct upcn_cmdline_options *result = &global_cmd_opts;
	int opt;

	// If we override sth., first deallocate
	if (result->eid)
		free(result->eid);
	if (result->cla_options)
		free(result->cla_options);

	// Set default values
	result->aap_node = DEFAULT_AAP_NODE;
	result->aap_service = DEFAULT_AAP_SERVICE;
	result->bundle_version = DEFAULT_BUNDLE_VERSION;
	result->status_reporting = false;
	result->lifetime = DEFAULT_BUNDLE_LIFETIME;
	// The following values cannot be 0
	result->mbs = 0;
	// The strings are set afterwards if not provided as an option
	result->eid = NULL;
	result->cla_options = NULL;

	while ((opt = getopt(argc, argv, "e:c:b:A:a:n:m:l:r")) != -1) {
		switch (opt) {
		case 'e':
			if (!optarg || validate_eid(optarg) != UPCN_OK ||
					strcmp("dtn:none", optarg) == 0) {
				LOG("Invalid EID provided!");
				return NULL;
			}
			result->eid = strdup(optarg);
			break;
		case 'c':
			if (!optarg) {
				LOG("Invalid CLA options string provided!");
				return NULL;
			}
			result->cla_options = strdup(optarg);
			break;
		case 'b':
			if (!optarg || strlen(optarg) != 1 || (
					optarg[0] != '6' && optarg[0] != '7')) {
				LOG("Invalid BP version provided!");
				return NULL;
			}
			result->bundle_version = (optarg[0] == '6') ? 6 : 7;
			break;
		case 'A':
			if (!optarg || strlen(optarg) < 1) {
				LOG("Invalid AAP node provided!");
				return NULL;
			}
			result->aap_node = strdup(optarg);
			break;
		case 'a':
			if (!optarg || strlen(optarg) < 1) {
				LOG("Invalid AAP port provided!");
				return NULL;
			}
			result->aap_service = strdup(optarg);
			break;
		case 'm':
			if (parse_uint64(optarg, &result->mbs)
					!= UPCN_OK || !result->mbs) {
				LOG("Invalid maximum bundle size provided!");
				return NULL;
			}
			break;
		case 'l':
			if (parse_uint64(optarg, &result->lifetime)
					!= UPCN_OK || !result->lifetime) {
				LOG("Invalid lifetime provided!");
				return NULL;
			}
			break;
		case 'r':
			result->status_reporting = true;
			break;
		default: /* '?' */
			LOGF("Usage: %s [-e EID] [-c cla_opts] " \
			     "[-b bp_version] [-A aap_ip] [-a aap_port] " \
			     "[-m maximum bundle size (bytes)] " \
			     "[-l lifetime (seconds)] [-r]",
			     argv[0]);
			return NULL;
		}
	}

	if (!result->eid)
		result->eid = strdup(DEFAULT_EID);
	if (!result->cla_options)
		result->cla_options = strdup(DEFAULT_CLA_OPTIONS);

	return result;
}

enum upcn_result parse_uint64(const char *str, uint64_t *result)
{
	char *end;
	unsigned long long val;

	if (!str)
		return UPCN_FAIL;
	errno = 0;
	val = strtoull(str, &end, 10);
	if (errno == ERANGE || end == str || *end != 0)
		return UPCN_FAIL;
	*result = (uint64_t)val;
	return UPCN_OK;
}
