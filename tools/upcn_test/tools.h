#ifndef TOOLS_H_INCLUDED
#define TOOLS_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* Case-insensitive strcmp */
static inline int striequal(char const *a, char const *b)
{
	while (*a != '\0' && *b != '\0') {
		if (tolower(*(a++)) != tolower(*(b++)))
			return 0;
	}
	return (*a == *b);
}

#endif /* TOOLS_H_INCLUDED */
