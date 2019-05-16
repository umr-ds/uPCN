#ifndef EIDMANAGER_H_INCLUDED
#define EIDMANAGER_H_INCLUDED

#include <inttypes.h>
#include <stdbool.h>

#include "upcn/simplehtab.h"

struct htab *eidmanager_init(void);
char *eidmanager_alloc_ref(char *value, const bool is_dynamic);
char *eidmanager_get_ref(char *value);
void eidmanager_free_ref(char *value);

#endif /* EIDMANAGER_H_INCLUDED */
