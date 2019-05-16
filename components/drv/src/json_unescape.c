/* Felix Walter, 2016, BSD3-licensed (see uPCN) */

#include <stddef.h>

#define HEX_TO_INT(x) \
	((x >= '0' && x <= '9') \
		? (x - '0') \
		: ((x >= 'A' && x <= 'F') \
			? (x - 'A' + 10) \
			: (x - 'a' + 10)))

size_t json_unescape_string(char *const string)
{
	char *r = string;
	char *w = string;
	size_t s = 0;

	for (; *r; r++, w++, s++) {
		if (*r != '\\') {
			*w = *r;
			continue;
		}
		r++;
		switch (*r) {
		case '\0':
			goto finish;
		case 'b':
			*w = '\b';
			break;
		case 'f':
			*w = '\f';
			break;
		case 'n':
			*w = '\n';
			break;
		case 'r':
			*w = '\r';
			break;
		case 't':
			*w = '\t';
			break;
		case 'u':
			*w = (char)HEX_TO_INT(r[3]) << 4
				| (char)HEX_TO_INT(r[4]);
			r += 4;
			break;
		default: /* ", \, ... */
			*w = *r;
			break;
		}
	}
finish:
	*w = '\0';
	return s;
}
