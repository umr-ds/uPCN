#!/usr/bin/env python3
# encoding: utf-8

import socket

from tcpcl_test_common import (
    serialize_tcpcl_contact_header,
    decode_tcpcl_contact_header,
)

DEFAULT_INCOMING_EID = "dtn:1"
BIND_TO = ("127.0.0.1", 42420)

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(BIND_TO)
    print("Bound TCP socket to", BIND_TO)
    sock.listen(1)
    while True:
        conn, addr = sock.accept()
        with conn:
            print("Accepted connection from", addr)
            try:
                conn.sendall(
                    serialize_tcpcl_contact_header(DEFAULT_INCOMING_EID)
                )
                upcn_contact_header = conn.recv(1024)
                try:
                    header = decode_tcpcl_contact_header(upcn_contact_header)
                except AssertionError:
                    print("Invalid TCPCL header received, closing connection")
                    conn.sendall(b"\x50")
                    conn.shutdown(socket.SHUT_RDWR)
                    continue
                print("Got valid TCPCL header:", header)
                while True:
                    data = conn.recv(1024)
                    if len(data) == 0:
                        print("Connection closed by other side")
                        break
                    print("Received:", data.hex())
            except OSError:
                print("Error receiving from socket, closing connection")
            except KeyboardInterrupt:
                conn.sendall(b"\x50")
                conn.shutdown(socket.SHUT_RDWR)
                raise
