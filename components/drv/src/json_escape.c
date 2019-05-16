/*
 * This has been ported from https://github.com/nlohmann/json
 *
 * The library is licensed under the MIT License 
 * <http://opensource.org/licenses/MIT>:
 *
 * Copyright (c) 2013-2015 Niels Lohmann
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stddef.h>
#include <string.h>

#include "drv/json_escape.h"

#define min(a,b) ({ \
	__typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_b < _a ? _b : _a; \
})

static size_t extra_space(const char *s, size_t *length)
{
	size_t result = 0;

	*length = 0;
	while (*s != '\0') {
		switch (*s) {
		case '"':
		case '\\':
		case '\b':
		case '\f':
		case '\n':
		case '\r':
		case '\t':
			/* from c (1 byte) to \x (2 bytes) */
			result += 1;
			break;
		default:
			/* from c (1 byte) to \uxxxx (6 bytes) */
			if (*s <= 0x1f)
				result += 5;
		break;
		}
		s++;
		(*length)++;
	}
	return result;
}

size_t json_escape_string(
	char *result, const size_t buffer_size, const char *s)
{
	size_t length;
	size_t space = extra_space(s, &length);
	size_t max_pos = buffer_size - 6;
	size_t eos = min(length, buffer_size);
	size_t pos = 0;

	if (space == 0) {
		memcpy(result, s, eos);
		result[eos] = '\0';
		return eos;
	}
	length += space;
	eos = min(length, buffer_size);
	memset(result, '\\', eos);
	while (*s != '\0' && pos < max_pos) {
		switch (*s) {
		case '"':
			/* quotation mark (0x22) */
			result[pos + 1] = '"';
			pos += 2;
			break;
		case '\\':
			/* reverse solidus (0x5c) */
			/* nothing to change */
			pos += 2;
			break;
		case '\b':
			/* backspace (0x08) */
			result[pos + 1] = 'b';
			pos += 2;
			break;
		case '\f':
			/* formfeed (0x0c) */
			result[pos + 1] = 'f';
			pos += 2;
			break;
		case '\n':
			/* newline (0x0a) */
			result[pos + 1] = 'n';
			pos += 2;
			break;
		case '\r':
			/* carriage return (0x0d) */
			result[pos + 1] = 'r';
			pos += 2;
			break;
		case '\t':
			/* horizontal tab (0x09) */
			result[pos + 1] = 't';
			pos += 2;
			break;
		default:
			if (*s <= 0x1f) {
				result[pos + 1] = 'u';
				result[pos + 2] = '0';
				result[pos + 3] = '0';
				result[pos + 4] = ((*s >> 4) < 10)
					? ('0' + (*s >> 4))
					: ('a' + (*s >> 4) - 10);
				result[pos + 5] = ((*s & 0x0f) < 10)
					? ('0' + (*s & 0x0f))
					: ('a' + (*s & 0x0f) - 10);
				pos += 6;
			} else {
				/* all other characters are added as-is */
				result[pos++] = *s;
			}
			break;
		}
		s++;
	}
	result[pos] = '\0';
	return pos;
}
