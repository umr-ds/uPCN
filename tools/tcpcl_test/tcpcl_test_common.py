#!/usr/bin/env python3
# coding: utf-8

# Add the "tools/" directory to the sys path such that the "bundle7" module
# can be imported.
import os
import sys
sys.path.append(os.path.abspath(os.path.join(__file__, '..', '..')))

import time
import struct
import datetime
import enum

from bundle7 import (
    Bundle,
    PrimaryBlock,
    CreationTimestamp,
    PayloadBlock,
    BundleAgeBlock,
    PreviousNodeBlock,
    HopCountBlock,
)

# BP Constants

class RFC5050Flag(enum.IntFlag):
    """Default BPv6 flags"""
    IS_FRAGMENT                = 0x00001
    ADMINISTRATIVE_RECORD      = 0x00002
    MUST_NOT_BE_FRAGMENTED     = 0x00004
    CUSTODY_TRANSFER_REQUESTED = 0x00008
    SINGLETON_ENDPOINT         = 0x00010
    ACKNOWLEDGEMENT_REQUESTED  = 0x00020

    # Priority
    NORMAL_PRIORITY            = 0x00080
    EXPEDITED_PRIORITY         = 0x00100

    # Reports
    REPORT_RECEPTION           = 0x04000
    REPORT_CUSTODY_ACCEPTANCE  = 0x08000
    REPORT_FORWARDING          = 0x10000
    REPORT_DELIVERY            = 0x20000
    REPORT_DELETION            = 0x40000

RFC5050Flag.DEFAULT_OUTGOING = (
    RFC5050Flag.SINGLETON_ENDPOINT |
    RFC5050Flag.NORMAL_PRIORITY
)


class BlockType(enum.IntEnum):
    """(Extension) block types"""
    PAYLOAD       = 1
    PREVIOUS_NODE = 7
    BUNDLE_AGE    = 8
    HOP_COUNT     = 9
    MAX           = 255


class BlockFlag(enum.IntFlag):
    """(Extension) block flags"""
    NONE                    = 0x00
    MUST_BE_REPLICATED      = 0x01
    REPORT_IF_UNPROC        = 0x02
    DELETE_BUNDLE_IF_UNPROC = 0x04
    LAST_BLOCK              = 0x08
    DISCARD_IF_UNPROC       = 0x10
    FWD_UNPROC              = 0x20
    HAS_EID_REF_FIELD       = 0x40


class TCPCLConnectionFlag(enum.IntFlag):
    NONE                   = 0x00
    REQUEST_ACK            = 0x01
    REACTIVE_FRAGMENTATION = 0x02
    ALLOW_REFUSAL          = 0x04
    REQUEST_LENGTH         = 0x08

TCPCLConnectionFlag.DEFAULT = TCPCLConnectionFlag.NONE


class RouterCommand(enum.Enum):
    """uPCN Command Constants"""
    ADD    = "1"
    UPDATE = "2"
    DELETE = "3"
    QUERY  = "4"


# Default EIDs
NULL_EID = "dtn:none"


def sdnv_encode(value):
    value = int(value)
    if value == 0:
        return b"\0"
    result = bytearray()
    while value != 0:
        result.append((value & 0x7F) | 0x80)
        value >>= 7
    result[0] &= 0x7F
    return bytes(reversed(result))


def sdnv_read(buffer, offset=0):
    result = 0
    cur = 0x80
    while (cur & 0x80) != 0:
        cur = buffer[offset]
        offset += 1
        result <<= 7
        result |= (cur & 0x7F)
    return result, offset


def unix2dtn(unix_timestamp):
    return unix_timestamp - 946684800


def dtn2unix(dtn_timestamp):
    return dtn_timestamp + 946684800


def serialize_bundle6(source_eid, destination_eid, payload,
                      report_to_eid=NULL_EID, custodian_eid=NULL_EID,
                      creation_timestamp=None, sequence_number=0,
                      lifetime=300, flags=RFC5050Flag.DEFAULT_OUTGOING,
                      fragment_offset=None, total_adu_length=None):

    # RFC 5050 header: https://tools.ietf.org/html/rfc5050#section-4.5
    # Build part before "primary block length"
    header_part1 = bytearray()
    header_part1.append(0x06)
    header_part1 += sdnv_encode(flags)

    # Build part after "primary block length"
    header_part2 = bytearray()

    # NOTE: This does not do deduplication (which is optional) currently.
    dictionary = bytearray()
    cur_dict_offset = 0
    for eid in (destination_eid, source_eid, report_to_eid, custodian_eid):
        scheme, ssp = eid.encode("ascii").split(b":", 1)
        dictionary += scheme + b"\0"
        header_part2 += sdnv_encode(cur_dict_offset)
        cur_dict_offset += len(scheme) + 1
        dictionary += ssp + b"\0"
        header_part2 += sdnv_encode(cur_dict_offset)
        cur_dict_offset += len(ssp) + 1

    if creation_timestamp is None:
        creation_timestamp = unix2dtn(time.time())
    header_part2 += sdnv_encode(creation_timestamp)
    header_part2 += sdnv_encode(sequence_number)
    header_part2 += sdnv_encode(lifetime)

    header_part2 += sdnv_encode(len(dictionary))
    header_part2 += dictionary

    if fragment_offset is not None and total_adu_length is not None:
        assert (flags & RFC5050Flag.IS_FRAGMENT) != 0
        header_part2 += sdnv_encode(fragment_offset)
        header_part2 += sdnv_encode(total_adu_length)

    # Add the length of all remaining fields as primary block length
    header = header_part1 + sdnv_encode(len(header_part2)) + header_part2

    # Build payload block
    # https://tools.ietf.org/html/rfc5050#section-4.5.2
    payload_block = bytearray()
    payload_block.append(BlockType.PAYLOAD)
    payload_block += sdnv_encode(BlockFlag.LAST_BLOCK)
    # Block length is the length of the remaining part of the block (PL data)
    payload_block += sdnv_encode(len(payload))
    payload_block += payload

    # NOTE: This does _not_ support extension blocks currently
    return header + payload_block


def serialize_bundle7(source_eid, destination_eid, payload,
                      report_to_eid=NULL_EID,
                      creation_timestamp=None, sequence_number=0,
                      lifetime=300, flags=0,
                      fragment_offset=None,
                      hop_limit=30, hop_count=0, bundle_age=0,
                      previous_node_eid=NULL_EID):

    if creation_timestamp is None:
        creation_timestamp = time.time()
    creation_datetime = datetime.datetime.fromtimestamp(
        creation_timestamp,
        datetime.timezone.utc,
    )

    bundle_array = []
    bundle_array.append(PrimaryBlock(
        bundle_proc_flags=flags,
        destination=destination_eid,
        source=source_eid,
        report_to=report_to_eid,
        creation_time=CreationTimestamp(creation_datetime, sequence_number),
        lifetime=lifetime,
        fragment_offset=fragment_offset,
    ))

    if previous_node_eid != NULL_EID:
        bundle_array.append(PreviousNodeBlock(previous_node_eid))
    bundle_array.append(HopCountBlock(hop_limit, hop_count))
    bundle_array.append(BundleAgeBlock(bundle_age))

    bundle_array.append(PayloadBlock(payload))
    bundle = Bundle(bundle_array)

    return bundle.serialize()


def serialize_tcpcl_contact_header(source_eid,
                                   flags=TCPCLConnectionFlag.DEFAULT,
                                   keepalive_interval=0):
    # https://tools.ietf.org/html/rfc7242#section-4.1
    result = bytearray()
    result += b"dtn!"  # TCPCL header magic
    result += b"\x03"  # version = v3
    result.append(flags)
    result += struct.pack("!H", keepalive_interval)
    source_eid_bin = source_eid.encode("ascii")
    result += sdnv_encode(len(source_eid_bin))
    result += source_eid_bin
    return result


def decode_tcpcl_contact_header(header):
    assert header[0:4] == b"dtn!", "header magic not found"
    assert header[4] == 0x03, "only TCPCL v3 is supported"
    assert len(header) >= 9, "corrupt (too short) TCPCL header"
    result = {
        "version": header[4],
        "flags": header[5],
        "keepalive_interval": (header[6] << 8) | header[7],
    }
    eid_len, end_offset = sdnv_read(header, 8)
    assert len(header) == end_offset + eid_len, "header length does not match"
    result["eid"] = header[end_offset:].decode("ascii")
    return result


def serialize_tcpcl_single_bundle_segment(bundle_data):
    # https://tools.ietf.org/html/rfc7242#section-5.2
    result = bytearray()
    message_type = 0x1  # DATA_SEGMENT
    message_flags = 0x3  # S | E = first AND last segment of bundle
    result.append((message_type << 4) | message_flags)
    result += sdnv_encode(len(bundle_data))
    result += bundle_data
    return result


def serialize_upcn_config_message(eid, cla_address,
                                  reachable_eids=None, contacts=None,
                                  config_type=RouterCommand.ADD):
    # missing escaping has to be addresses in uPCN
    assert "(" not in "".join([eid, cla_address] + (reachable_eids or []))
    assert ")" not in "".join([eid, cla_address] + (reachable_eids or []))

    eid_list = (
        (
            "[" +
            ",".join("(" + eid + ")" for eid in reachable_eids) +
            "]"
        )
        if reachable_eids else ""
    )
    contact_list = (
        (
            "[" +
            ",".join(
                "{{{},{},{}}}".format(start, end, bitrate)
                for start, end, bitrate in contacts
            ) +
            "]"
        )
        if contacts else ""
    )

    return "{}({}):({}):{}:{};".format(
        config_type.value,
        eid,
        cla_address,
        eid_list,
        contact_list,
    ).encode("ascii")


def make_contact(start_offset, end_offset, bitrate):
    cur_time = int(time.time())
    return (
        unix2dtn(cur_time + start_offset),
        unix2dtn(cur_time + end_offset),
        bitrate,
    )
