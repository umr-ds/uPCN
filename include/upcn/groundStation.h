#ifndef GROUNDSTATION_H_INCLUDED
#define GROUNDSTATION_H_INCLUDED

#include <stdint.h>

#include "upcn/upcn.h"
#include "upcn/bundle.h"
#include "upcn/rrnd.h"

#ifndef TOOLS
#include <cla.h>
#endif

struct endpoint_list {
	char *eid;
	float p;
	struct endpoint_list *next;
};

struct routed_bundle {
	bundleid_t id;
	char *destination; /* TODO: Optimize */
	uint8_t preemption_improvement;
	enum bundle_routing_priority prio;
	uint32_t size;
	uint64_t exp_time;
	struct contact **contacts;
	uint8_t contact_count;
	uint8_t serialized;
	uint8_t transmitted;
};

struct routed_bundle_list {
	struct routed_bundle *data;
	struct routed_bundle_list *next;
};

struct contact {
	struct ground_station *ground_station;
	uint64_t from;
	uint64_t to;
	uint32_t bitrate;
	uint32_t total_capacity;
	int32_t remaining_capacity_p0;
	int32_t remaining_capacity_p1;
	int32_t remaining_capacity_p2;
	struct endpoint_list *contact_endpoints;
	struct routed_bundle_list *contact_bundles;
	uint8_t bundle_count;
	int8_t active;
};

struct contact_list {
	struct contact *data;
	struct contact_list *next;
};

struct associated_contact_list {
	struct contact *data;
	float p;
	struct associated_contact_list *next;
};

enum ground_station_flags {
	GS_FLAG_NONE = 0,
	GS_FLAG_DISCOVERED = 0x1,
	GS_FLAG_INTERNET_ACCESS = 0x2
};

struct ground_station {
	char *eid;
	enum convergence_layer cla;
	/* Currently only a link layer address (= an X.25 address) */
	char *cla_addr;
	enum ground_station_flags flags;
	float trustworthiness;
	struct endpoint_list *endpoints;
	struct contact_list *contacts;
	struct bloom_filter *nbf;
	struct rrnd_gs_info *rrnd_info;
#ifndef TOOLS
	struct cla_config *cla_config;
#endif
};

struct ground_station_list {
	struct ground_station *station;
	struct ground_station_list *next;
};

#define CONTACT_CAPACITY(contact, p) \
	(p == 1 ? contact->remaining_capacity_p1 : \
	(p == 2 ? contact->remaining_capacity_p2 : \
	contact->remaining_capacity_p0))

struct ground_station *ground_station_create(char *eid);
struct ground_station_discovery_info
	*ground_station_discovery_info_create(void);
struct rrnd_gs_info *ground_station_rrnd_info_create(
	struct ground_station *const gs);
struct contact *contact_create(struct ground_station *station);

void free_contact(struct contact *contact);
void free_ground_station(struct ground_station *gs);

struct endpoint_list *endpoint_list_add(
	struct endpoint_list **list, char *eid);
int endpoint_list_remove(struct endpoint_list **list, char *eid);
struct endpoint_list *endpoint_list_free(struct endpoint_list *e);
struct endpoint_list *endpoint_list_union(
	struct endpoint_list *a, struct endpoint_list *b, const int copy_b);
struct endpoint_list *endpoint_list_difference(
	struct endpoint_list *a, struct endpoint_list *b, const int free_b);

int contact_list_sorted(struct contact_list *cl, const int order_by_from);
struct contact_list *contact_list_free(struct contact_list *e);
struct contact_list *contact_list_union(
	struct contact_list *a, struct contact_list *b,
	struct contact_list **modified);
struct contact_list *contact_list_difference(
	struct contact_list *a, struct contact_list *b, const int free_b,
	struct contact_list **modified, struct contact_list **deleted);

struct endpoint_list *endpoint_list_strip_and_sort(struct endpoint_list *el);
int ground_station_prepare_and_verify(struct ground_station *gs);
void recalculate_contact_capacity(struct contact *contact);
int32_t contact_get_cur_remaining_capacity(
	struct contact *contact, enum bundle_routing_priority prio);
int add_contact_to_ordered_list(
	struct contact_list **list, struct contact *contact,
	const int order_by_from);
int remove_contact_from_list(
	struct contact_list **list, struct contact *contact);
int add_contact_to_ordered_assoc_list(
	struct associated_contact_list **list, struct contact *contact,
	float p, const int order_by_from);
int remove_contact_from_assoc_list(
	struct associated_contact_list **list, struct contact *contact);

struct contact *contact_from_rrnd_contact_info(
	struct ground_station *station, struct rrnd_contact_info *rci);

#endif /* GROUNDSTATION_H_INCLUDED */
