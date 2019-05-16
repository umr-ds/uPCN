#ifndef CONTACTMANAGER_H_INCLUDED
#define CONTACTMANAGER_H_INCLUDED

#include "upcn/upcn.h"
#include "upcn/groundStation.h"

struct contact_manager_params {
	Semaphore_t semaphore;
	QueueIdentifier_t control_queue;
	QueueIdentifier_t beacon_queue;
};

/* Flags what should be checked */
enum contact_manager_signal {
	CM_SIGNAL_NONE = 0x0,
	CM_SIGNAL_CONTACTS_UPDATED = 0x1,
	CM_SIGNAL_BUNDLE_SCHEDULED = 0x2,
	CM_SIGNAL_UNKNOWN = 0x3
};

struct contact_manager_params contact_manager_start(
	QueueIdentifier_t router_signaling_queue,
	struct contact_list **clistptr);

uint64_t contact_manager_get_next_contact_time(void);
uint8_t contact_manager_in_contact(void);
void contact_manager_reset_time(void);

#endif /* CONTACTMANAGER_H_INCLUDED */
