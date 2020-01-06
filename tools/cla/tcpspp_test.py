#!/usr/bin/env python3

import socket

from pyupcn.agents import ConfigMessage, make_contact
from pyupcn.bundle7 import serialize_bundle7, Bundle
from pyupcn.bundle6 import serialize_bundle6
from pyupcn.spp import SPPPacket, SPPPacketHeader, SPPTimecode


DEFAULT_OUTGOING_EID = "dtn:2"
DEFAULT_INCOMING_EID = "dtn:1"
DEFAULT_CONTACT_CLA_ADDRESS = "127.0.0.1:42420"


def hexify(data):
    return " ".join(map("{:02x}".format, data))


def send_debug(sock, data):
    print(">> {}".format(hexify(data)))
    sock.sendall(data)


def recv_debug(sock, n):
    result = sock.recv(n)
    if result:
        print("<< {}".format(hexify(result)))
    return result


def main():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-l", "--host",
        default="127.0.0.1",
        help="IP address to connect to (defaults to 127.0.0.1)",
    )
    parser.add_argument(
        "-p", "--port",
        type=int,
        default=4223,
        help="Port to connect to (defaults to 4223)",
    )
    parser.add_argument(
        "-b", "--bundle-version",
        default="7",
        choices="67",
        help="Version of the bundle protocol to use (defaults to 6): "
        "6 == RFC 5050, 7 == BPv7-bis"
    )
    parser.add_argument(
        "--crc",
        dest="with_crc",
        action="store_true",
        default=False,
        help="Use CRC16"
    )

    args = parser.parse_args()

    serialize_bundle = {
        "6": serialize_bundle6,
        "7": serialize_bundle7,
    }[args.bundle_version]

    s = socket.socket(socket.AF_INET,   # FIXME: discover AF from host
                      socket.SOCK_STREAM,
                      0)
    with s:
        s.connect((args.host, args.port))

        config_msg1 = ConfigMessage(
            DEFAULT_OUTGOING_EID,
            "127.0.0.1:42421",
            contacts=[
                make_contact(2, 2, 500),
            ],
        )
        config_msg2 = ConfigMessage(
            DEFAULT_INCOMING_EID,
            DEFAULT_CONTACT_CLA_ADDRESS,
            contacts=[
                make_contact(5, 5, 500),
            ],
        )

        def wrap_spp(data, with_crc):
            return bytes(SPPPacket(
                SPPPacketHeader(
                    timecode=SPPTimecode(),
                ),
                data,
                has_crc=with_crc,
            ))

        print("Configuring contact:", config_msg1)
        send_debug(s, wrap_spp(
            serialize_bundle(
                DEFAULT_OUTGOING_EID,
                "dtn://ops-sat.dtn/config",
                bytes(config_msg1),
            ),
            args.with_crc,
        ))
        print("Configuring contact:", config_msg2)
        send_debug(s, wrap_spp(
            serialize_bundle(
                DEFAULT_OUTGOING_EID,
                "dtn://ops-sat.dtn/config",
                bytes(config_msg2),
            ),
            args.with_crc,
        ))

        send_debug(s, wrap_spp(
            serialize_bundle(
                DEFAULT_OUTGOING_EID,
                DEFAULT_INCOMING_EID,  # bundle to the configured EID
                b"42" * 3,
            ),
            args.with_crc,
        ))

        hdr_raw = recv_debug(s, 6)
        data_length, has_secondary = SPPPacketHeader.preparse_data_length(
            hdr_raw
        )
        data_plus_secondary = recv_debug(s, data_length)
        packet, _ = SPPPacket.parse(
            hdr_raw + data_plus_secondary,
            timecode_used=True,
            has_crc=args.with_crc,
        )
        print(packet)
        if args.bundle_version == "7":
            b = Bundle.parse(packet.payload)
            print("Received bundle: {}".format(repr(b)))
            for block in b:
                print((
                    "Block {} {} CRC: provided=0x{:08x}, calculated=0x{:08x} "
                    "({})"
                ).format(
                    block.block_number,
                    repr(block.block_type),
                    block.crc_provided,
                    block.calculate_crc(),
                    ("OK" if block.crc_provided == block.calculate_crc()
                     else "FAIL"),
                ))
        else:
            # Cannot decode 5050 bundles yet
            print(packet.payload)


if __name__ == "__main__":
    main()
