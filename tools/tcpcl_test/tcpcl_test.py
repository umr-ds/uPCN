#!/usr/bin/env python3
# encoding: utf-8

import time
import socket
import argparse

from tcpcl_test_common import (
    serialize_tcpcl_contact_header,
    decode_tcpcl_contact_header,
    serialize_upcn_config_message,
    serialize_tcpcl_single_bundle_segment,
    serialize_bundle6,
    serialize_bundle7,
    make_contact,
)

DEFAULT_OUTGOING_EID = "dtn:2"
DEFAULT_INCOMING_EID = "dtn:1"
# we have to listen on this port to properly test bundle transmission
DEFAULT_CONTACT_CLA_ADDRESS = "127.0.0.1:42420"


def run_simple_forwarding_test(serialize_bundle, concurrent=False):

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect(("localhost", 4556))
        print("Connected to uPCN, sending TCPCL header...")
        sock.sendall(serialize_tcpcl_contact_header(DEFAULT_OUTGOING_EID))
        upcn_contact_header = sock.recv(1024)
        print(
            "Received TCPCL header:",
            decode_tcpcl_contact_header(upcn_contact_header),
        )

        config_msg1 = serialize_upcn_config_message(
            DEFAULT_OUTGOING_EID,
            "127.0.0.1:42421",
            contacts=[
                make_contact(2, 10 if concurrent else 4, 500),
            ],
        )
        config_msg2 = serialize_upcn_config_message(
            DEFAULT_INCOMING_EID,
            DEFAULT_CONTACT_CLA_ADDRESS,
            contacts=[
                make_contact(3 if concurrent else 5, 10, 500),
            ],
        )
        print("Configuring contact:", config_msg1)
        sock.sendall(serialize_tcpcl_single_bundle_segment(
            serialize_bundle(
                DEFAULT_OUTGOING_EID,
                "dtn://ops-sat.dtn/config",
                config_msg1,
            )
        ))
        print("Configuring contact:", config_msg2)
        sock.sendall(serialize_tcpcl_single_bundle_segment(
            serialize_bundle(
                DEFAULT_OUTGOING_EID,
                "dtn://ops-sat.dtn/config",
                config_msg2,
            )
        ))

        print("Terminating TCPCL connection...")
        sock.sendall(b"\x50")
        sock.shutdown(socket.SHUT_RDWR)

    # wait for contact to start, connect and send bundle
    time.sleep(3.2)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect(("localhost", 4556))
        print("Connected to uPCN, sending TCPCL header...")
        sock.sendall(serialize_tcpcl_contact_header(DEFAULT_OUTGOING_EID))
        upcn_contact_header = sock.recv(1024)
        print(
            "Received TCPCL header:",
            decode_tcpcl_contact_header(upcn_contact_header),
        )

        # wait until uPCN has processed the routing command and send the bundle
        sock.sendall(serialize_tcpcl_single_bundle_segment(
            serialize_bundle(
                DEFAULT_OUTGOING_EID,
                DEFAULT_INCOMING_EID,  # bundle to the configured EID
                b"\x42" * 42,
            )
        ))
        time.sleep(1.8)

        print("Terminating TCPCL connection...")
        sock.sendall(b"\x50")
        sock.shutdown(socket.SHUT_RDWR)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-p", "--protocol",
        choices=["6", "7"], default="6",
        help="the bundle protocol version to be used",
    )
    parser.add_argument(
        "--concurrent",
        action="store_true",
        help="use two concurrent contacts for sending/receiving",
    )
    args = parser.parse_args()
    serialize_bundle = {
        "6": serialize_bundle6,
        "7": serialize_bundle7,
    }.get(args.protocol)
    run_simple_forwarding_test(serialize_bundle, args.concurrent)
