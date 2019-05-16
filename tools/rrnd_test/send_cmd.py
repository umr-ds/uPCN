#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import logging
import json
import sys
import zmq

try:
    JSONDecodeError = json.decoder.JSONDecodeError
except AttributeError:
    JSONDecodeError = ValueError


def main(args):
    logger = _initialize_logger(args.verbose)
    zmq_ctx = zmq.Context.instance()
    mq = zmq_ctx.socket(zmq.REQ)
    mq.linger = 0
    mq.SNDTIMEO = 100
    mq.RCVTIMEO = 500
    mq.connect(args.address)
    logger.debug("Connecting to ZMQ REP socket '{}'".format(args.address))
    try:
        mq.send_string(args.COMMAND)
        response = mq.recv_string()
        print(response)
        ret = json.loads(response)
        if "result" not in ret:
            logger.warn("No 'result' key in rcvd object: {}".format(ret))
        elif (ret["result"] & 0x08) != 0:
            logger.warn("ND failure: ret = {}, fc = {}".format(
                ret, hex(ret["result"])
            ))
        else:
            logger.info("OK, status code: {}".format(hex(ret["result"])))
    except zmq.ZMQError as e:
        logger.warn("Error processing ZMQ request!")
        raise
    except JSONDecodeError as e:
        logger.warn("Error decoding JSON!")
        raise
    finally:
        mq.close()
        zmq_ctx.destroy()


def _initialize_logger(verbosity):
    logging.basicConfig(
        level={
            0: logging.INFO,
        }.get(verbosity, logging.DEBUG),
        format="%(asctime)-23s %(levelname)s: %(message)s",
    )
    return logging.getLogger(sys.argv[0])


def _get_argument_parser():
    parser = argparse.ArgumentParser(
        description="tool to send a request via ZMQ and print the response",
    )
    parser.add_argument(
        "COMMAND",
        help="the command to be sent, should be valid JSON",
    )
    parser.add_argument(
        "-a", "--address",
        default="tcp://127.0.0.1:7763",
        help="the ZMQ address to connect to, default: tcp://127.0.0.1:7763",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="count",
        default=0,
        help="increase output verbosity",
    )
    return parser


if __name__ == "__main__":
    main(_get_argument_parser().parse_args())
