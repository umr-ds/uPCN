#!/usr/bin/env python3
import socket

from datetime import datetime, timedelta

from tcpcl_test_common import (
    serialize_bundle6,
    serialize_bundle7,
    serialize_upcn_config_message,
    make_contact,
)


DEFAULT_OUTGOING_EID = "dtn:2"
DEFAULT_INCOMING_EID = "dtn:1"
DEFAULT_CONTACT_CLA_ADDRESS = "127.0.0.1:42420"
DTN_EPOCH = datetime(2000, 1, 1)
CCSDS_EPOCH = datetime(1958, 1, 1)



def wrap_spp(payload):
    assert payload

    secondary_header = bytes([
        # second preamble bit = 0b0
        # code type = 0b001 (ccsds unsegmented code)
        # base unit octets = 0b11 (3 + 1 = 4)
        # fractional octets = 0b00 (0)
        0b0_001_11_00,
    ]) + int((datetime.utcnow() - CCSDS_EPOCH).total_seconds()).to_bytes(
        4,
        'big',
    )

    # version = 0b000
    # packet type = 0b0
    # secondary header flag = 0b1
    # apid = 0b00000000001
    hdr_p1 = 0b000_0_1_00000000001
    # sequence flags = 0b11  (unsegmented)
    # sequence number = 0b00000000000000  (0)
    hdr_p2 = 0b11_00000000000000
    # length = len(payload) - 1
    hdr_len = len(payload) + len(secondary_header) - 1

    return b"".join(
        [hdr_p1.to_bytes(2, 'big'),
         hdr_p2.to_bytes(2, 'big'),
         hdr_len.to_bytes(2, 'big'),
         secondary_header,
         payload]
    )


def parse_spp_header(data):
    assert len(data) == 6

    hdr_p1 = int.from_bytes(data[:2], 'big')
    hdr_p2 = int.from_bytes(data[2:4], 'big')
    hdr_len = int.from_bytes(data[4:6], 'big')

    assert (hdr_p1 & 0b1110_0000_0000_0000) == 0
    is_request = (hdr_p1 & 0b0001_0000_0000_0000) != 0
    has_secondary_header = (hdr_p1 & 0b0000_1000_0000_0000) != 0
    apid = hdr_p1 & 0b0000_0111_1111_1111

    sequence_flags = (hdr_p2 & 0b1100_0000_0000_0000) >> 14
    sequence_number = hdr_p2 & 0b0011_1111_1111_1111

    length = hdr_len + 1

    return (
        is_request, has_secondary_header, apid, sequence_flags, sequence_number,
        length,
    )


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


def decode_secondary_header(buf):
    assert buf[0] == 0x1c
    return CCSDS_EPOCH + timedelta(seconds=int.from_bytes(buf[1:5], 'big'))


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
        default=4223,
        help="Port to connect to (defaults to 4223)",
    )
    parser.add_argument(
        "-b", "--bundle-version",
        default="6",
        choices="67",
        help="Version of the bundle protocol to use (defaults to 6): "
        "6 == RFC 5050, 7 == BPv7-bis"
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

        config_msg1 = serialize_upcn_config_message(
            DEFAULT_OUTGOING_EID,
            "127.0.0.1:42421",
            contacts=[
                make_contact(2, 4, 500),
            ],
        )
        config_msg2 = serialize_upcn_config_message(
            DEFAULT_INCOMING_EID,
            DEFAULT_CONTACT_CLA_ADDRESS,
            contacts=[
                make_contact(5, 10, 500),
            ],
        )
        print("Configuring contact:", config_msg1)
        send_debug(s, wrap_spp(
            serialize_bundle(
                DEFAULT_OUTGOING_EID,
                "dtn://ops-sat.dtn/config",
                config_msg1,
            )
        ))
        print("Configuring contact:", config_msg2)
        send_debug(s, wrap_spp(
            serialize_bundle(
                DEFAULT_OUTGOING_EID,
                "dtn://ops-sat.dtn/config",
                config_msg2,
            )
        ))

        send_debug(s, wrap_spp(
            serialize_bundle(
                DEFAULT_OUTGOING_EID,
                DEFAULT_INCOMING_EID,  # bundle to the configured EID
                b"42" * 23,
            )
        ))

        while True:
            hdr = parse_spp_header(recv_debug(s, 6))
            print(hdr)
            length = hdr[-1]
            has_secondary_header = hdr[1]
            if has_secondary_header:
                secondary_header = recv_debug(s, 5)
                print(decode_secondary_header(secondary_header))
                length -= 5
            payload = recv_debug(s, length)
            print(payload)


if __name__ == "__main__":
    main()
