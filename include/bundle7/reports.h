#ifndef BUNDLE7_REPORTS_INCLUDED
#define BUNDLE7_REPORTS_INCLUDED

#include "upcn/bundle.h"  // struct bundle, struct bundle_status_report

/**
 * Generates a BPv7-bis administrative record bundle of type
 * "Bundle status report" for the given bundle that can be send
 * to the Report-To EID.
 */
struct bundle *bundle7_generate_status_report(
	const struct bundle * const bundle,
	const struct bundle_status_report *report,
	char *source);


/**
 * Generates a BPv7-bis administrative record bundle of type
 * "Custody Signal" for the given bundle that can be send to
 * the current custodian EID.
 *
 * @param bundle Bundle for which the custody signal should be created
 * @param signal
 * @oaram source Source EID for the custody signal bundle
 */
struct bundle_list *bundle7_generate_custody_signals(
	const struct bundle * const bundle,
	const struct bundle_custody_signal *signal,
	char *source);

/**
 * Parses the payload block of the a BPv7-bis administrative record
 */
struct bundle_administrative_record *bundle7_parse_administrative_record(
	const struct bundle * const record_bundle);

#endif /* BUNDLE7_REPORTS_INCLUDED */
