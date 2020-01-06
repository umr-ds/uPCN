#!/usr/bin/env python3
# encoding: utf-8

import argparse
import socket
import uuid

from pyupcn.aap import AAPMessage, AAPMessageType, InsufficientAAPDataError


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


def run_aap_test(host, port, dest_eid, payload, eid_suffix):
    addr = (host, port)
    eid_suffix = eid_suffix or str(uuid.uuid4())

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect(addr)
        print("Connected to uPCN, awaiting WELCOME message...")

        msg_welcome = recv_aap(sock)
        assert msg_welcome.msg_type == AAPMessageType.WELCOME
        print("WELCOME message received! ~ EID = {}".format(msg_welcome.eid))

        print("Sending REGISTER message...")
        sock.send(AAPMessage(AAPMessageType.REGISTER, eid_suffix).serialize())
        msg_ack = recv_aap(sock)
        assert msg_ack.msg_type == AAPMessageType.ACK
        print("ACK message received!")

        print("Sending bundle...")
        sock.send(AAPMessage(AAPMessageType.SENDBUNDLE,
                             dest_eid,
                             payload).serialize())
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
        description="send a bundle via uPCN's AAP interface",
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
        "-a", "--agentid",
        default=None,
        help="the agent id to register with uPCN (default: random UUID)",
    )
    parser.add_argument(
        "dest_eid",
        help="the destination EID of the created bundle",
    )
    parser.add_argument(
        "payload",
        nargs="?",
        default=None,
        help="the payload of the created bundle, (default: read from STDIN)",
    )

    args = parser.parse_args()

    if not args.payload:
        with sys.stdin.buffer.raw as f:
            args.payload = f.read()
    else:
        args.payload = args.payload.encode("utf-8")

    run_aap_test(
        args.host,
        args.port,
        args.dest_eid,
        args.payload,
        args.agentid,
    )
