#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "upcn/upcn.h"
#include "drv/htab_hash.h"
#include "upcn/simplehtab.h"
#include "upcn/eidManager.h"

#ifdef UPCN_LOCAL
typedef void *Semaphore_t;
#define hal_semaphore_init_mutex() NULL
#define hal_semaphore_delete(a) ((void)a)
#define hal_semaphore_take_blocking(a) ((void)a)
#define hal_semaphore_release(a) ((void)a)
#else // UPCN_LOCAL
#include <hal_semaphore.h>
#endif // UPCN_LOCAL

/*
 * eidManager - Zero copy approach for EIDs
 *
 * TODO:
 * 1. Optimize. This is a proof of concept written to be readable and
 *    conserve memory. CPU usage can definitely be reduced in the future.
 * 2. Make generic. Apply to other data types, possibly integrate into BSM...
 *
 */

#define EIDMGR_HTAB_SLOT_COUNT 512
#define EIDMGR_PLOOKUP_SLOT_COUNT 256

static struct htab_entrylist *htab_elem[EIDMGR_HTAB_SLOT_COUNT];
static struct htab eid_table;
static uint16_t ptr_lookup_table[EIDMGR_PLOOKUP_SLOT_COUNT];

static Semaphore_t sem;

#define HASHL(key, len) (hashlittle(key, len, 0))
/* Gets length of string, should be optimized out... */
#define HASH(key) (HASHL(key, strlen(key)))
#define PLSLOT(value) ((uintptr_t)(value) % EIDMGR_PLOOKUP_SLOT_COUNT)

struct htab *eidmanager_init(void)
{
	if (sem == NULL)
		sem = hal_semaphore_init_mutex();
	else
		hal_semaphore_take_blocking(sem);
	memset(ptr_lookup_table, 0xFF,
		EIDMGR_PLOOKUP_SLOT_COUNT * sizeof(uint16_t));
	if (eid_table.elements != NULL) {
		hal_semaphore_release(sem);
		return &eid_table;
	}
	eid_table.slot_count = EIDMGR_HTAB_SLOT_COUNT;
	eid_table.elements = htab_elem;
	htab_init(&eid_table);
	hal_semaphore_release(sem);
	return &eid_table;
}

static struct htab_entrylist *get_pair(char *value, uint16_t *hash)
{
	struct htab_entrylist *rec_pair;

	*hash = ptr_lookup_table[PLSLOT(value)];
	if (*hash != 0xFFFFU) {
		rec_pair = htab_get_known_pair(&eid_table, value, *hash, 1);
		if (rec_pair != NULL)
			return rec_pair;
	}
	*hash = HASH(value) % EIDMGR_HTAB_SLOT_COUNT;
	return htab_get_known_pair(&eid_table, value, *hash, 0);
}

char *eidmanager_alloc_ref(char *value, const bool is_dynamic)
{
	uint16_t hash;
	struct htab_entrylist *rec_pair;
	uint32_t *ref_count;

	hal_semaphore_take_blocking(sem);
	rec_pair = get_pair(value, &hash);
	if (rec_pair != NULL) {
		ref_count = (uint32_t *)(&rec_pair->value);
		(*ref_count)++;
		if (is_dynamic && value != rec_pair->key)
			free(value);
	} else {
		/* XXX Not completely zero-copy as htab copies once.
		 * TODO: Optimize this out.
		 */
		rec_pair = htab_add(&eid_table, value, (void *)1);
		if (is_dynamic)
			free(value);
	}
	ptr_lookup_table[PLSLOT(rec_pair->key)] = hash;
	hal_semaphore_release(sem);
	return rec_pair->key;
}

char *eidmanager_get_ref(char *value)
{
	uint16_t hash;
	struct htab_entrylist *rec_pair;
	void *result;

	hal_semaphore_take_blocking(sem);
	rec_pair = get_pair(value, &hash);
	if (rec_pair == NULL)
		result = NULL;
	else
		result = rec_pair->key;
	hal_semaphore_release(sem);
	return result;
}

void eidmanager_free_ref(char *value)
{
	uint16_t hash;
	struct htab_entrylist *rec_pair;
	uint32_t *ref_count;

	if (value == NULL)
		return;

	hal_semaphore_take_blocking(sem);
	rec_pair = get_pair(value, &hash);
	ref_count = (uint32_t *)(&rec_pair->value);
	/* ASSERT(rec_pair != NULL); */
	if (rec_pair == NULL || rec_pair->key != value) {
		/* This is currently needed for parsers (reset) */
		free(value);
	} else {
		(*ref_count)--;
		if (*ref_count == 0) {
			htab_remove(&eid_table, value);
			if (ptr_lookup_table[PLSLOT(value)] == hash)
				ptr_lookup_table[PLSLOT(value)] = 0xFFFFU;
		}
	}
	hal_semaphore_release(sem);
}
