#include <stdio.h>
#include <stdarg.h>

#include "drv/mini-printf.h"
#include "drv/json_escape.h"
#include "upcn/rrnd.h"

#if !defined(UPCN_LOCAL) && defined(DEBUG)
#include <hal_debug.h>
#endif // !defined(UPCN_LOCAL) && defined(DEBUG)

size_t rrnd_print_gs_info(
	char *const buf, const size_t bufsize, const char *const eid,
	const struct rrnd_gs_info *const gs)
{
	char *out = buf;
	size_t bytes_written, eid_bytes;
	struct rrnd_contact_info p;

	if (bufsize < 15)
		goto fail;
	bytes_written = mini_snprintf(out, bufsize, "{\"eid\": \"");
	if (bytes_written >= bufsize)
		goto fail_fatal;
	out += bytes_written;
	eid_bytes = json_escape_string(out, bufsize - bytes_written, eid);
	bytes_written += eid_bytes;
	out += eid_bytes;
	if (bytes_written >= bufsize)
		goto fail;
	if (gs == NULL) {
		bytes_written += mini_snprintf(out, bufsize - bytes_written,
			"\", \"success\": false}");
		if (bytes_written >= bufsize)
			goto fail;
		return bytes_written;
	}
	p = gs->predictions.last;
	/* TODO: Remove snprintf dependency! */
	bytes_written += snprintf(out, bufsize - bytes_written,
		"\", \"reliability\": %f, \"variance\": %f, \"flags\": %u, " \
			"\"tx_bitrate\": %lu, \"rx_bitrate\": %lu, " \
			"\"last_contact_start_time\": %llu, " \
			"\"last_contact_end_time\": %llu, " \
			"\"beacon_period\": %hu, \"beacon_count\": %hu, " \
			"\"locator_elements\": %hhu, " \
			"\"location\": [%f, %f, %f], " \
			"\"ap_contacts\": %hhu, \"ap\": [%lu, %lu, %lu], " \
			"\"history_elements\": %hhu, " \
			"\"history_missed_contacts\": %d, " \
			"\"predictions\": %hhu, \"next_prediction\": " \
			"[%llu, %lu, %f, %f, %lu, %f], \"success\": true}",
		gs->prob_metrics.reliability,
		rrnd_estimator_get_reliability_variance(&gs->prob_metrics),
		gs->flags,
		gs->tx_bitrate, gs->rx_bitrate,
		gs->last_contact_start_time,
		gs->last_contact_end_time,
		gs->beacon_period, gs->beacon_count,
		gs->locator_data.elements,
		gs->location.x, gs->location.y, gs->location.z,
		gs->availability_pattern.contacts_honored,
		gs->availability_pattern.offset,
		gs->availability_pattern.on_time,
		gs->availability_pattern.period,
		gs->contact_history.contacts_in_bitmap,
		__builtin_popcountll(
			gs->contact_history.failed_contacts_bitmap
		),
		gs->predictions.count,
		p.start, p.duration, p.probability, p.max_elevation,
		p.capacity.mean, p.capacity.std_deviation
	);
	if (bytes_written <= bufsize)
		return bytes_written;
fail:
	bytes_written = mini_snprintf(buf, bufsize, "{\"success\": false}");
	if (bytes_written < bufsize)
		return bytes_written;
fail_fatal:
	if (bufsize > 3)
		return mini_snprintf(buf, bufsize, "{}");
	else
		return 0;
}

#ifdef DEBUG

static bool debug_io_enabled = true;

int rrnd_io_debug_printf(const char *format, ...)
{
	va_list argp;
	int rc;

	if (!debug_io_enabled)
		return 0;
	va_start(argp, format);
#ifdef UPCN_LOCAL
	rc = vprintf(format, argp);
#else // UPCN_LOCAL
	rc = hal_debug_vprintf(format, argp);
#endif // UPCN_LOCAL
	va_end(argp);
	return rc;
}

void rrnd_io_debug_set_enabled(const bool enabled)
{
	debug_io_enabled = enabled;
}

#endif // DEBUG
