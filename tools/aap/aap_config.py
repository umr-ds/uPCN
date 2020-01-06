#!/usr/bin/env python3
# encoding: utf-8

import argparse
import socket
import uuid

from pyupcn.aap import AAPMessage, AAPMessageType, InsufficientAAPDataError
from pyupcn.agents import ConfigMessage, make_contact


def recv_aap(sock):
    buf = bytearray()
    msg = None
    while msg is None:
        buf += sock.recv(1)
        try:
            msg = AAPMessage.parse(buf)
        except InsufficientAAPDataError:
            continue
    return msg


def send_bundle_via_aap(host, port, config_message):
    addr = (host, port)
    eid_suffix = str(uuid.uuid4())

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect(addr)
        print("Connected to uPCN, awaiting WELCOME message...")

        msg_welcome = recv_aap(sock)
        assert msg_welcome.msg_type == AAPMessageType.WELCOME
        print("WELCOME message received! ~ EID = {}".format(msg_welcome.eid))
        base_eid = msg_welcome.eid

        print("Sending REGISTER message...")
        sock.send(AAPMessage(AAPMessageType.REGISTER, eid_suffix).serialize())
        msg_ack = recv_aap(sock)
        assert msg_ack.msg_type == AAPMessageType.ACK
        print("ACK message received!")

        print("Sending configure bundle...")
        sock.send(AAPMessage(AAPMessageType.SENDBUNDLE,
                             base_eid + "/config",
                             config_message).serialize())
        msg_sendconfirm = recv_aap(sock)
        assert msg_sendconfirm.msg_type == AAPMessageType.SENDCONFIRM
        print("SENDCONFIRM message received! ~ ID = {}".format(
            msg_sendconfirm.bundle_id
        ))

        print("Terminating connection...")
        sock.shutdown(socket.SHUT_RDWR)


if __name__ == "__main__":
    import sys

    parser = argparse.ArgumentParser(
        description="create or update a node in uPCN",
    )
    parser.add_argument(
        "--host",
        type=str,
        default="localhost",
        help="the AAP host to connect to (default: localhost)",
    )
    parser.add_argument(
        "-p", "--port",
        type=int,
        default=4242,
        help="the AAP port to connect to (default: 4242)",
    )
    parser.add_argument(
        "eid",
        help="the EID of the node",
    )
    parser.add_argument(
        "cla_address",
        help="the CLA address of the node",
    )
    parser.add_argument(
        "-s", "--schedule",
        nargs=3,
        type=int,
        metavar=("start_offset", "duration", "bitrate"),
        action="append",
        default=[],
        help="schedule a contact relative to the current time",
    )
    parser.add_argument(
        "-r", "--reaches",
        type=str,
        action="append",
        default=[],
        help="specify an EID reachable via the node",
    )

    args = parser.parse_args()

    if not args.schedule:
        print("at least one -s/--schedule argument must be given",
              file=sys.stderr)
        sys.exit(1)

    msg = bytes(ConfigMessage(
        args.eid,
        args.cla_address,
        contacts=[
            make_contact(*contact)
            for contact in args.schedule
        ],
        reachable_eids=args.reaches,
    ))

    print(msg)

    send_bundle_via_aap(args.host, args.port, msg)
