"""BPbis bundle Generator

A simple Python package for generating and serialize BPbis bundles into CBOR.

Dependencies:
    This module depends on the ``cbor`` package which can be installed via pip:

    .. code:: bash

        pip install cbor

Usage:
    If you are in the root directory of the project you can simply run:

    .. code:: bash

        python3 -m tools.bundle7

    This will execute the ``__main__.py`` script next to this file.
    See PEP 3122 for more details about it.

    If you want to use this package in other scripts, make sure that the
    ``tools/`` directory is in the ``sys.path`` list. Then you can perform an
    import as usual:

    .. code:: python

        # Add "tools/" directory to the system path
        import os
        import sys
        sys.path.append(os.abspath(os.path.join(__file__), '..'))

        # Now you can simply import all Python modules from the "tools/"
        # directory
        from bundle7 import Bundle

"""
import struct
from enum import IntEnum, IntFlag
from datetime import datetime, tzinfo, timedelta
from random import random
import cbor
from cbor.cbor import CBOR_ARRAY
from .crc import crc16_x25, crc32


__all__ = [
    'BundleProcFlag',
    'CRCType',
    'BlockType',
    'BlockProcFlag',
    'RecordType',
    'ReasonCode',
    'StatusCode',
    'EID',
    'dtn_start',
    'CreationTimestamp',
    'print_hex',
    'PrimaryBlock',
    'CanonicalBlock',
    'PayloadBlock',
    'AdministrativeRecord',
    'BundleStatusReport',
    'PreviousNodeBlock',
    'BundleAgeBlock',
    'HopCountBlock',
    'Bundle',
]


class BundleProcFlag(IntFlag):
    NONE                       = 0x0000
    IS_FRAGMENT                = 0x0001
    ADMINISTRATIVE_RECORD      = 0x0002
    MUST_NOT_BE_FRAGMENTED     = 0x0004
    ACKNOWLEDGEMENT_REQUESTED  = 0x0020
    REPORT_STATUS_TIME         = 0x0040
    CONTAINS_MANIFEST          = 0x0080
    REPORT_RECEPTION           = 0x0100
    REPORT_FORWARDING          = 0x0400
    REPORT_DELIVERY            = 0x0800
    REPORT_DELETION            = 0x1000


class CRCType(IntEnum):
    NONE  = 0  # indicates "no CRC is present."
    CRC16 = 1  # indicates "a CRC-16 (a.k.a., CRC-16-ANSI) is present."
    CRC32 = 2  # indicates "a standard IEEE 802.3 CRC-32 is present."


class BlockType(IntEnum):
    PAYLOAD           = 1
    PREVIOUS_NODE     = 7
    BUNDLE_AGE        = 8
    HOP_COUNT         = 9


class BlockProcFlag(IntFlag):
    NONE                    = 0x00
    MUST_BE_REPLICATED      = 0x01
    DISCARD_IF_UNPROC       = 0x02
    REPORT_IF_UNPROC        = 0x04
    DELETE_BUNDLE_IF_UNPROC = 0x08


class RecordType(IntEnum):
    BUNDLE_STATUS_REPORT = 1


class ReasonCode(IntEnum):
    """Bundle status report reason codes"""
    NO_INFO                  = 0
    LIFETIME_EXPIRDE         = 1
    FORWARDED_UNIDIRECTIONAL = 2
    TRANSMISSION_CANCELED    = 3
    DEPLETED_STORAGE         = 4
    DEST_EID_UNINTELLIGIBLE  = 5
    NO_KNOWN_ROUTE           = 6
    NO_TIMELY_CONTACT        = 7
    BLOCK_UNINTELLIGIBLE     = 8


class StatusCode(IntEnum):
    RECEIVED_BUNDLE  = 0x01
    FORWARDED_BUNDLE = 0x04
    DELIVERED_BUNDLE = 0x08
    DELETED_BUNDLE   = 0x10


class EID(tuple):
    """BPbis Endpoint Identifier"""

    def __new__(cls, eid):
        if eid is None or eid == 'dtn:none':
            return super().__new__(cls, (1, 0))

        # Copy existing EID
        if isinstance(eid, EID):
            return super().__new__(cls, (eid[0], eid[1]))

        try:
            schema, ssp = eid.split(":")

            if schema == 'dtn':
                return super().__new__(cls, (1, ssp))
            elif schema == 'ipn':
                nodenum, servicenum = ssp.split('.')
                return super().__new__(cls, (2, (int(nodenum), int(servicenum))))
        except ValueError:
            raise ValueError("Invalid EID {!r}".format(eid))

        raise ValueError("Unknown schema {!r}".format(schema))

    @property
    def schema(self):
        if self[0] == 1:
            return 'dtn'
        elif self[0] == 2:
            return 'ipn'
        else:
            raise ValueError("Unknown schema {!r}".format(self[0]))

    @property
    def ssp(self):
        if self[0] == 2:
            return "{}.{}".format(*self[1])
        else:
            if self[1] == 0:
                return "none"
            else:
                return self[1]

    def __str__(self):
        return "{}:{}".format(self.schema, self.ssp)

    def __repr__(self):
        # Reuse the __str__ method here
        return "<EID '{}'>".format(self)


# --------
# DTN Time
# --------
#
ZERO = timedelta(0)

class UTC(tzinfo):
    """UTC timezone info class that can be used by :class:`datetime.datetime`
    objects."""

    def utcoffset(self, dt):
        return ZERO

    def tzname(self, dt):
        return "UTC"

    def dst(self, dt):
        return ZERO

utc = UTC()
dtn_start = datetime(2000, 1, 1, 0, 0, tzinfo=utc)


def dtn_time(dt):
    return int((dt - dtn_start).total_seconds())


class CreationTimestamp(tuple):

    def __new__(cls, time, sequence_number):
        return super().__new__(cls, [dtn_time(time), sequence_number])

    @property
    def time(self):
        return dtn_start +  timedelta(seconds=self[0])

    @property
    def sequence_number(self):
        return self[1]

    def __repr__(self):
        return "<CreationTimestamp time={} sequence={}>".format(
                    self.time, self.sequence_number)


def print_hex(binary):
    """Prints hexadecimal representation of a binary input in one line"""
    for b in binary:
        print("0x{:02x}, ".format(b), end='')
    print()


class PrimaryBlock(object):
    """The primary bundle block contains the basic information needed to
    forward bundles to their destinations.
    """

    def __init__(self, bundle_proc_flags=BundleProcFlag.NONE,
                       crc_type=CRCType.CRC16,
                       destination=None,
                       source=None,
                       report_to=None,
                       creation_time=None,
                       fragment_offset=None,
                       lifetime=24 * 60 * 60):
        self.version = 7
        self.bundle_proc_flags = bundle_proc_flags
        self.crc_type = crc_type
        self.destination = EID(destination)
        self.source = EID(source)
        self.report_to = EID(report_to)

        if not creation_time:
            creation_time = CreationTimestamp(datetime.now(utc), 0)

        self.creation_time = creation_time
        self.lifetime = lifetime

        # Optional fields
        self.fragment_offset = fragment_offset

    def has_flag(self, required):
        return self.bundle_proc_flags & required == required


    def serialize(self):
        primary_block = [
            self.version,
            self.bundle_proc_flags,
            self.crc_type,
            self.destination,
            self.source,
            self.report_to,
            self.creation_time,
            self.lifetime,
        ]

        # Fragmentation
        if self.has_flag(BundleProcFlag.IS_FRAGMENT):
            # Generate random fragment offset
            if not self.fragment_offset:
                primary_block.append(int(random() * 10))
            else:
                primary_block.append(self.fragment_offset)

        # CRC
        if self.crc_type != CRCType.NONE:
            binary = [
                # CBOR Array header
                struct.pack('B', 0x80 | len(primary_block) + 1),
            ]

            # encode each field
            for field in primary_block:
                binary.append(cbor.dumps(field))

            if self.crc_type == CRCType.CRC32:
                # Empty CRC-32 bit field: CBOR "byte string"
                binary.append(b'\x44\x00\x00\x00\x00')
                binary = b''.join(binary)

                crc = crc32(binary)
                primary_block.append(struct.pack('!I', crc))
            else:
                binary.append(b'\x42\x00\x00')
                binary = b''.join(binary)

                crc = crc16_x25(binary)
                primary_block.append(struct.pack('!H', crc))


        return cbor.dumps(primary_block)


class CanonicalBlock(object):
    """Every block other than the primary block"""

    def __init__(self, block_type, data,
                       block_number=None,
                       block_proc_flags=BlockProcFlag.NONE,
                       crc_type=CRCType.CRC32):
        self.block_type = block_type
        self.block_proc_flags = block_proc_flags
        self.block_number = block_number
        self.crc_type = crc_type
        self.data = data

    def serialize_data(self):
        return self.data

    def serialize(self):
        block = [
            self.block_type,
            self.block_number,
            self.block_proc_flags,
            self.crc_type,
        ]

        serialized_data = self.serialize_data()

        # Block data length
        # If the value can be expressed in one byte, we have to ensure
        # that we do not return 0 as block length. Therefore max(1, ...)
        block.append(max(1, len(cbor.dumps(serialized_data))))

        # Block-specific data
        block.append(serialized_data)

        assert len(block) == 6, "Block must contain 6 items"
        # CRC
        if self.crc_type != CRCType.NONE:
            if self.crc_type == CRCType.CRC32:
                # Empty CRC-32 bit field: CBOR "byte string"
                empty_crc = b'\x44\x00\x00\x00\x00'
            else:
                empty_crc = b'\x42\x00\x00'

            binary = struct.pack('B', 0x80 | 7) + b''.join(
                [cbor.dumps(item) for item in block]
            ) + empty_crc

            if self.crc_type == CRCType.CRC32:
                crc = crc32(binary)
                # unsigned long in network byte order
                block.append(struct.pack('!I', crc))
            else:
                crc = crc16_x25(binary)
                # unsigned short in network byte order
                block.append(struct.pack('!H', crc))

        return cbor.dumps(block)


class PayloadBlock(CanonicalBlock):

    def __init__(self, data, **kwargs):
        super().__init__(BlockType.PAYLOAD,
                         data,
                         block_number=0,
                         **kwargs)


# ----------------------
# Administrative Records
# ----------------------

class AdministrativeRecord(PayloadBlock):

    def __init__(self, bundle, record_type_code, record_data):
        record_data.extend([
            bundle.primary_block.source,
            bundle.primary_block.creation_time
        ])

        if bundle.is_fragmented:
            record_data.extend([
                bundle.primary_block.fragment_offset,
                bundle.primary_block.total_payload_length
            ])

        super().__init__(data=[
            record_type_code,
            record_data
        ])


class BundleStatusReport(AdministrativeRecord):

    def __init__(self, infos, reason, bundle, time=None):
        status_info = [
            [infos & StatusCode.RECEIVED_BUNDLE  != 0],
            [infos & StatusCode.FORWARDED_BUNDLE != 0],
            [infos & StatusCode.DELIVERED_BUNDLE != 0],
            [infos & StatusCode.DELETED_BUNDLE   != 0],
        ]

        if bundle.primary_block.has_flag(BundleProcFlag.REPORT_STATUS_TIME):
            if not time:
                time = datetime.now(utc)
            for info in status_info:
                if info[0]:
                    info.append(dtn_time(time))

        record_data = [
            status_info,
            reason,
        ]

        super().__init__(bundle,
                         record_type_code=RecordType.BUNDLE_STATUS_REPORT,
                         record_data=record_data)

    @property
    def status_info(self):
        return self.data[1][0]

    def __repr__(self):
        return "<BundleStatusReport {!r}>".format(self.status_info)


# ----------------
# Extension Blocks
# ----------------


class PreviousNodeBlock(CanonicalBlock):

    def __init__(self, eid, **kwargs):
        super().__init__(BlockType.PREVIOUS_NODE, EID(eid), **kwargs)


class BundleAgeBlock(CanonicalBlock):

    def __init__(self, age, **kwargs):
        super().__init__(BlockType.BUNDLE_AGE, age, **kwargs)


class HopCountBlock(CanonicalBlock):

    def __init__(self, hop_limit, hop_count, **kwargs):
        super().__init__(BlockType.HOP_COUNT, (hop_limit, hop_count), **kwargs)


class Bundle(object):

    def __init__(self, primary_block, payload_block=None, blocks=None):
        if payload_block is None:
            payload_block = primary_block[-1]
            blocks        = primary_block[1:-1]
            primary_block = primary_block[0]

        self.primary_block = primary_block
        self.payload_block = payload_block
        self.blocks = []

        if blocks:
            for block in blocks:
                self.add(block)

    @property
    def is_fragmented(self):
        return self.primary_block.has_flag(BundleProcFlag.IS_FRAGMENT)

    def add(self, new_block):
        num = 0

        for block in self.blocks:
            message = "Block {} number already assigned".format(block.block_number)

            assert block.block_number != new_block.block_number, message

            # Search for a new block number if the block does not already
            # contains a block number
            num = max(num, block.block_number)

            # Assert unique block types
            if block.block_type == new_block.block_type:
                # Previous Node
                if block.block_type == BlockType.PREVIOUS_NODE:
                    raise ValueError("There must be only one 'Previous Node' block")
                # Hop Count
                elif block.block_type == BlockType.HOP_COUNT:
                    raise ValueError("There must be only one 'Hop Count' block")
                # Bundle Age
                elif block.block_type == BlockType.BUNDLE_AGE:
                    creation_time, _ = self.primary_block.creation_time
                    if dtn_time(creation_time) == 0:
                        raise ValueError("There must be only one 'Bundle Age' block "
                                         "if the bundle creation time is 0")

        if new_block.block_number is None:
            new_block.block_number = num + 1

        # Previous Node blocks must be the first blocks after the primary block
        if new_block.block_type == BlockType.PREVIOUS_NODE:
            self.blocks.insert(0, new_block)
        else:
            self.blocks.append(new_block)


    def serialize(self):
        # Header for indefinite array
        head = struct.pack('B', CBOR_ARRAY | 31)
        # Stop-code for indefinite array
        stop = b'\xff'
        blocks = [self.primary_block] + self.blocks + [self.payload_block]
        blocks = [block.serialize() for block in blocks]
        return head + b''.join(blocks) + stop
