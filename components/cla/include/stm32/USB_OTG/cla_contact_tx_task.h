#ifndef CLA_CONTACT_TX_TASK_H_INCLUDED
#define CLA_CONTACT_TX_TASK_H_INCLUDED

#include "upcn/upcn.h"
#include "upcn/bundle.h"
#include "upcn/groundStation.h"

enum cla_contact_tx_task_command_type {
	GS_COMMAND_UNDEFINED, /* 0x00 */
	GS_COMMAND_BUNDLES,   /* 0x01 */
	GS_COMMAND_FINALIZE,  /* 0x02 */
};

struct cla_contact_tx_task_command {
	enum cla_contact_tx_task_command_type type;
	struct routed_bundle_list *bundles;
	struct contact *contact;
};

struct cla_contact_tx_task_creation_result {
	Task_t task_handle;
	QueueIdentifier_t queue_handle;
	enum upcn_result result;
};

struct cla_contact_tx_task_creation_result cla_launch_contact_tx_task(
	QueueIdentifier_t router_signaling_queue,
	struct cla_task_tx *task);
void cla_contact_tx_task_delete(QueueIdentifier_t queue);

#endif /* CLA_CONTACT_TX_TASK_H_INCLUDED */
