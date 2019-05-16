#include <stdlib.h>
#include <stdint.h>
#include "upcn/bundle.h"
#include "upcn/parser.h"
#include "upcn/sdnv.h"
#include "upcn/eidManager.h"
#include "upcn/reportManager.h"
#include <bundle6/reports.h>
#include <bundle7/reports.h>


struct bundle *generate_status_report(
	const struct bundle * const bundle,
	const struct bundle_status_report *report)
{
	switch (bundle->protocol_version) {
	// RFC 5050
	case 6:
		return bundle6_generate_status_report(bundle, report);
	// BPv7-bis
	case 7:
		return bundle7_generate_status_report(bundle, report,
			UPCN_SCHEME ":" UPCN_SSP);
	default:
		return NULL;
	}
}


struct bundle_list *generate_custody_signals(
	const struct bundle * const bundle,
	const struct bundle_custody_signal *signal)
{
	struct bundle *signal_bundle;

	switch (bundle->protocol_version) {
	// RFC 5050
	case 6:
		signal_bundle =
			bundle6_generate_custody_signal(bundle, signal);
		return bundle_list_entry_create(signal_bundle);
	default:
		return NULL;
	}
}


struct bundle_administrative_record *parse_administrative_record(
	const struct bundle * const record_bundle)
{
	switch (record_bundle->protocol_version) {
	// RFC 5050
	case 6:
		return bundle6_parse_administrative_record(record_bundle);
	// BPv7-bis
	case 7:
		return bundle7_parse_administrative_record(record_bundle);
	default:
		return NULL;
	}
}


void free_administrative_record(struct bundle_administrative_record *record)
{
	if (record != NULL) {
		if (record->custody_signal != NULL)
			free(record->custody_signal);
		if (record->status_report != NULL)
			free(record->status_report);
		if (record->bundle_source_eid != NULL)
			eidmanager_free_ref(record->bundle_source_eid);
		free(record);
	}
}
