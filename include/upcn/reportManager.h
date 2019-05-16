#ifndef REPORTMANAGER_H_INCLUDED
#define REPORTMANAGER_H_INCLUDED

#include "upcn/bundle.h"


struct bundle *generate_status_report(
	const struct bundle * const bundle,
	const struct bundle_status_report *report);


struct bundle_list *generate_custody_signals(
	const struct bundle * const bundle,
	const struct bundle_custody_signal *signal);


/**
 * Parses the payload block of an administrative record bundle.
 * If case of error, NULL will be returned.
 */
struct bundle_administrative_record *parse_administrative_record(
	const struct bundle * const record_bundle);


void free_administrative_record(struct bundle_administrative_record *record);

#endif /* REPORTMANAGER_H_INCLUDED */
