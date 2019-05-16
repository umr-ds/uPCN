#ifndef BEACONCOOKIEMANAGER_H_INCLUDED
#define BEACONCOOKIEMANAGER_H_INCLUDED

#ifdef ND_USE_COOKIES

#include <stddef.h>

#include "upcn/upcn.h"
#include "upcn/beacon.h"

enum upcn_result beacon_cookie_manager_init(const uint8_t *const secret);
void beacon_cookie_manager_add_cookie(
	const char *const eid, const unsigned long time);
int beacon_cookie_manager_is_valid(
	const char *const eid, const uint8_t *const cookie_bytes,
	const unsigned long cur_time);
enum upcn_result beacon_cookie_manager_write_to_tlv(
	struct tlv_definition *const target);

#endif /* ND_USE_COOKIES */

#endif /* BEACONCOOKIEMANAGER_H_INCLUDED */
