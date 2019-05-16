#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "upcn/upcn.h"
#include "upcn/sdnv.h"
#include "upcn/beacon.h"
#include "upcn/beaconSerializer.h"

#define HAS_EID(b) (HAS_FLAG(beacon->flags, BEACON_FLAG_HAS_EID) \
	&& beacon->eid != NULL)
#define HAS_SERVICES(b) (HAS_FLAG(beacon->flags, BEACON_FLAG_HAS_SERVICES) \
	&& beacon->services != NULL)
#define HAS_PERIOD(b) (HAS_FLAG(beacon->flags, BEACON_FLAG_HAS_PERIOD) \
	&& (beacon->period & 0x80000000) == 0)

struct tlv_serializer_stack_frame {
	struct tlv_definition *tlv;
	uint16_t i; /* index in children array */
};

static uint16_t tlv_update_serialized_length(struct tlv_definition *tlv)
{
	ASSERT(tlv != NULL);
	int stackptr = 0;
	struct tlv_serializer_stack_frame stack[TLV_MAX_DEPTH];
	struct tlv_definition *e;
	uint16_t i;

	stack[0].tlv = tlv;
	stack[0].i = 0;
	do {
		e = stack[stackptr].tlv;
		i = stack[stackptr].i;
		if (e->tag >= 64) {
			if (i == 0)
				e->length = 0;
			if (i < e->value.children.count) {
				/* go deeper */
				stack[stackptr++].i++;
				stack[stackptr].tlv
					= &e->value.children.list[i];
				stack[stackptr].i = 0;
			} else {
				/* go up */
				stackptr--;
				if (stackptr != -1)
					stack[stackptr].tlv->length
						+= sdnv_get_size_u16(e->length)
							+ e->length + 1;
			}
		} else {
			int nextlen;

			switch (e->tag) {
			case TLV_TYPE_BOOLEAN:
				e->length = 1;
				nextlen = 2;
				break;
			/* FIXED */
			case TLV_TYPE_FIXED16:
				e->length = 2;
				nextlen = 3;
				break;
			case TLV_TYPE_FIXED32:
			case TLV_TYPE_FLOAT:
				e->length = 4;
				nextlen = 5;
				break;
			case TLV_TYPE_FIXED64:
			case TLV_TYPE_DOUBLE:
				e->length = 8;
				nextlen = 9;
				break;
			/* SDNV */
			case TLV_TYPE_UINT64:
			case TLV_TYPE_SINT64:
				e->length = sdnv_get_size_u64(
					e->value.u64);
				nextlen = e->length + 1;
				break;
			/* BYTES / STRING */
			case TLV_TYPE_STRING:
			case TLV_TYPE_BYTES:
				if (e->value.b == NULL
					|| e->length == 0
				) {
					e->length = 0;
					e->value.b = NULL;
					nextlen = 3;
					break;
				}
				fallthrough;
			/* explicit length values */
			default:
				nextlen = 1 + e->length
					+ sdnv_get_size_u16(e->length);
				break;
			}
			/* go up */
			stackptr--;
			if (stackptr != -1) {
				stack[stackptr].tlv->length += nextlen;
				ASSERT(stack[stackptr].tlv->tag >= 64);
			}
		}
	} while (stackptr != -1);
	return stack[0].tlv->length
		+ sdnv_get_size_u16(stack[0].tlv->length) + 1;
}

/* tlv_update_serialized_length must have been run before */
static uint16_t tlv_serialize(uint8_t *buffer, struct tlv_definition *tlv)
{
	ASSERT(buffer != NULL);
	ASSERT(tlv != NULL);
	int c = 0;
	int stackptr = 0;
	struct tlv_serializer_stack_frame stack[TLV_MAX_DEPTH];
	struct tlv_definition *e;
	uint16_t i;

	stack[0].tlv = tlv;
	stack[0].i = 0;
	do {
		e = stack[stackptr].tlv;
		i = stack[stackptr].i;
		if (e->tag >= 64) {
			if (i == 0) {
				/* write tlv header (t, l) */
				buffer[c++] = (uint8_t)e->tag;
				c += sdnv_write_u16(&buffer[c], e->length);
			}
			if (i < e->value.children.count) {
				/* go deeper */
				stack[stackptr++].i++;
				stack[stackptr].tlv
					= &e->value.children.list[i];
				stack[stackptr].i = 0;
			} else {
				/* go up */
				stackptr--;
			}
		} else {
			buffer[c++] = (uint8_t)e->tag;
			switch (e->tag) {
			case TLV_TYPE_BOOLEAN:
				buffer[c++] = e->value.u8;
				break;
			case TLV_TYPE_FIXED16:
				buffer[c++] = (uint8_t)((e->value.u16
					&0xFF00) >> 8);
				buffer[c++] = e->value.u16 & 0x00FF;
				break;
			case TLV_TYPE_FIXED32:
			case TLV_TYPE_FLOAT:
				buffer[c++] = (uint8_t)((e->value.u32
					&0xFF000000) >> 24);
				buffer[c++] = (uint8_t)((e->value.u32
					&0x00FF0000) >> 16);
				buffer[c++] = (uint8_t)((e->value.u32
					&0x0000FF00) >> 8);
				buffer[c++] = (uint8_t)(e->value.u32
					&0x000000FF);
				break;
			case TLV_TYPE_FIXED64:
			case TLV_TYPE_DOUBLE:
				buffer[c++] = (uint8_t)((e->value.u64
					&0xFF00000000000000) >> 56);
				buffer[c++] = (uint8_t)((e->value.u64
					&0x00FF000000000000) >> 48);
				buffer[c++] = (uint8_t)((e->value.u64
					&0x0000FF0000000000) >> 40);
				buffer[c++] = (uint8_t)((e->value.u64
					&0x000000FF00000000) >> 32);
				buffer[c++] = (uint8_t)((e->value.u64
					&0x00000000FF000000) >> 24);
				buffer[c++] = (uint8_t)((e->value.u64
					&0x0000000000FF0000) >> 16);
				buffer[c++] = (uint8_t)((e->value.u64
					&0x000000000000FF00) >> 8);
				buffer[c++] = (uint8_t)(e->value.u64
					&0x00000000000000FF);
				break;
			/* SDNV */
			case TLV_TYPE_UINT64:
			case TLV_TYPE_SINT64:
				c += sdnv_write_u64(
					&buffer[c], e->value.u64);
				break;
			/* explicit length values */
			default:
				if (e->length == 0 || e->value.b == NULL) {
					buffer[c++] = 1;
					buffer[c++] = 0;
				} else {
					c += sdnv_write_u16(
						&buffer[c], e->length);
					memcpy(&buffer[c], e->value.b,
						e->length);
					c += e->length;
				}
				break;
			}
			/* go up */
			stackptr--;
		}
	} while (stackptr != -1);
	return c;
}

uint8_t *beacon_serialize(struct beacon *beacon, uint16_t *length)
{
	ASSERT(beacon != NULL);
	ASSERT(length != NULL);

	uint16_t eid_len = HAS_EID(beacon) ? strlen(beacon->eid) : 0;
	uint16_t len = 4;
	uint16_t i, c;
	uint8_t *out;

	if (HAS_EID(beacon))
		len += sdnv_get_size_u16(eid_len) + eid_len;
	if (HAS_SERVICES(beacon)) {
		len += sdnv_get_size_u16(beacon->service_count);
		for (i = 0; i < beacon->service_count; i++)
			len += tlv_update_serialized_length(
				&beacon->services[i]);
	}
	if (HAS_PERIOD(beacon))
		len += sdnv_get_size_u32(beacon->period);

	out = malloc(len);
	if (out == NULL) {
		*length = 0;
		return NULL;
	}
	c = 0;
	out[c++] = beacon->version;
	out[c++] = (uint8_t)beacon->flags;
	out[c++] = (uint8_t)((beacon->sequence_number & 0xFF00) >> 8);
	out[c++] = (uint8_t)(beacon->sequence_number & 0x00FF);
	if (HAS_EID(beacon)) {
		c += sdnv_write_u16(&out[c], eid_len);
		memcpy(&out[c], beacon->eid, eid_len);
		c += eid_len;
	}
	if (HAS_SERVICES(beacon)) {
		c += sdnv_write_u16(&out[c], beacon->service_count);
		for (i = 0; i < beacon->service_count; i++)
			c += tlv_serialize(&out[c], &beacon->services[i]);
	}
	if (HAS_PERIOD(beacon))
		c += sdnv_write_u32(&out[c], beacon->period);

	ASSERT(c == len);
	*length = len;
	return out;
}
