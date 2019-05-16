#ifndef CUSTODYMANAGER_H_INCLUDED
#define CUSTODYMANAGER_H_INCLUDED

#include <stdbool.h>

#include "upcn/upcn.h"
#include "upcn/bundle.h"

bool custody_manager_has_redundant_bundle(struct bundle *bundle);
bool custody_manager_storage_is_acceptable(struct bundle *bundle);
bool custody_manager_has_accepted(struct bundle *bundle);
struct bundle *custody_manager_get_by_record(
	struct bundle_administrative_record *record);

enum upcn_result custody_manager_accept(struct bundle *bundle);
void custody_manager_release(struct bundle *bundle);

#endif /* CUSTODYMANAGER_H_INCLUDED */
