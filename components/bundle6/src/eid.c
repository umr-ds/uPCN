#include <stddef.h>
#include <string.h>
#include "upcn/upcn.h"
#include "drv/mini-printf.h"
#include "bundle6/eid.h"
#include "upcn/eidManager.h"


char *bundle6_read_eid(struct bundle *bundle, struct eid_reference eidref)
{
	/* Both parts can have up to 1024 bytes including the trailing zero */
	/* as well as the colon between the parts... */
	int max_buf = 2049;
	char *eid_buffer;
	int str_len;

	/* ...but they can't be longer than the dictionary. */
	if ((bundle->dict_length + 1) < max_buf)
		max_buf = bundle->dict_length + 1;
	eid_buffer = malloc(sizeof(char) * max_buf);
	if (eid_buffer == NULL)
		return NULL;
	str_len = mini_snprintf(eid_buffer, max_buf, "%s:%s",
		(char *)(bundle->dict + eidref.scheme_offset),
		(char *)(bundle->dict + eidref.ssp_offset)) + 1;
	if (str_len <= 0) {
		free(eid_buffer);
		return NULL;
	} else if (str_len < max_buf) {
		char *new_buffer = realloc(eid_buffer, sizeof(char) * str_len);

		if (new_buffer != NULL)
			eid_buffer = new_buffer;
	}
	return eidmanager_alloc_ref(eid_buffer, 1);
}


bool bundle6_eid_equals(
	const struct bundle *bundle, const struct eid_reference ref,
	const char *scheme, const char *ssp)
{
	return (strcmp(bundle->dict + ref.scheme_offset, scheme) == 0 &&
		strcmp(bundle->dict + ref.ssp_offset, ssp) == 0);
}


bool bundle6_eid_equals_string(
	const struct bundle *bundle, const struct eid_reference ref,
	const char *eid, size_t eid_len)
{
	size_t scheme_len = strlen(bundle->dict + ref.scheme_offset);

	return (strncmp(bundle->dict + ref.scheme_offset, eid, scheme_len) == 0
		&& strncmp(bundle->dict + ref.ssp_offset,
			eid + scheme_len + 1, eid_len - scheme_len - 1) == 0);
}
