#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import time
import glob
import signal
import logging
import argparse
import subprocess

# An ordered array of the tools to be spawned for the RRND tests
# Three toolchains can be selected: posix, stm32 and local ("headless" RRND)
RRND_TOOLCHAIN = [
    {
        "name": "uPCN (POSIX)",
        "path": "build/posix_local/upcn",
        "only": ["posix"],
        "args": [
            "{upcnport}",
        ],
    },
    {
        "name": "upcn_netconnect",
        "path": "tools/build/upcn_netconnect/upcn_netconnect",
        "only": ["posix"],
        "args": [
            "tcp://127.0.0.1:{upcnport}",
            "tcp://127.0.0.1:{connectpubport}",
            "tcp://127.0.0.1:{connectrepport}",
        ],
    },
    {
        "name": "upcn_connect",
        "path": "tools/build/upcn_connect/upcn_connect",
        "only": ["stm32"],
        "max_instances": 1,
        "args": [
            "{upcndevice}",
            "tcp://127.0.0.1:{connectpubport}",
            "tcp://127.0.0.1:{connectrepport}",
        ],
    },
    {
        "name": "rrnd_proxy (uPCN)",
        "path": "tools/build/rrnd_proxy/rrnd_proxy",
        "only": ["posix", "stm32"],
        "args": [
            "{proxyverbose}",
            "tcp://127.0.0.1:{proxyport}",
            "tcp://127.0.0.1:{connectpubport}",
            "tcp://127.0.0.1:{connectrepport}",
        ],
        "delay_before": 0.1,
    },
    {
        "name": "rrnd_proxy (LOCAL)",
        "path": "tools/build/rrnd_proxy/rrnd_proxy",
        "only": ["local"],
        "args": [
            "{proxyverbose}",
            "tcp://127.0.0.1:{proxyport}",
        ],
    },
]

# Period (in seconds) for checking if all processes are still running
PROCESS_CHECK_TIMEOUT = 0.1
# Timeout for terminating processes via SIGTERM (afterwards, SIGKILL is used)
PROCESS_STOP_TIMEOUT = 1


def _spawn_processes(logger, instances, args_param, count, toolchain, quieter):
    """
    Spawns the provided count of processes for the specified toolchain.

    * toolchain can be one of posix, local, stm32
    * the instances are added to the provided data structure
    * for stm32, only one chain of processes can be spawned
    """
    max_inst = 100
    for chain_index in range(count):
        if chain_index >= max_inst:
            logger.warning(
                "Cannot spawn more than {} instance(s) for {}".format(
                    max_inst,
                    toolchain,
                )
            )
            break
        for prog in instances:
            if "only" in prog and toolchain not in prog["only"]:
                continue
            if "delay_before" in prog:
                time.sleep(prog["delay_before"])
            prog_args = [arg.format(**args_param) for arg in prog["args"]]
            prog_args = [arg for arg in prog_args if arg != ""]
            logger.info("Spawning {} with args: {}".format(
                prog["name"],
                prog_args,
            ))
            if "processes" not in prog:
                prog["processes"] = []
            new_process = subprocess.Popen(
                [prog["path"]] + prog_args,
                stdout=(sys.stdout if quieter < 1 else subprocess.DEVNULL),
                stderr=(sys.stderr if quieter < 2 else subprocess.DEVNULL),
            )
            prog["processes"].append(new_process)
            logger.debug("New process for {} ({})".format(
                prog["name"],
                new_process.pid,
            ))
            if "max_instances" in prog:
                max_inst = min(max_inst, prog["max_instances"])
        for param_key in args_param.keys():
            if isinstance(args_param[param_key], int):
                args_param[param_key] += 1


def _wait_for_processes(logger, instances):
    """
    Perodically checks if all processes are still running.
    If any process has terminated, a SystemExit exception is raised.
    """
    while True:
        for prog in instances:
            if "processes" not in prog:
                continue
            for pnum, process in enumerate(prog["processes"]):
                if process.poll() is not None:
                    logger.critical(
                        "Process {}, instance {} crashed; code={}".format(
                            prog["name"],
                            pnum,
                            process.returncode,
                        )
                    )
                    raise SystemExit(1)
        time.sleep(PROCESS_CHECK_TIMEOUT)


def _terminate_processes(logger, instances):
    """
    Terminates all spawned processes cleanly using SIGTERM and after a timeout
    using SIGKILL.
    """
    logger.info("Terminating...")
    for prog in reversed(instances):
        if "processes" not in prog:
            continue
        for pnum, process in enumerate(prog["processes"]):
            process.terminate()
            try:
                process.wait(PROCESS_STOP_TIMEOUT)
                logger.debug("Process {} ({}) terminated; code={}".format(
                    prog["name"],
                    process.pid,
                    process.returncode,
                ))
            except subprocess.TimeoutExpired:
                logger.warning("Timeout - killing process {} ({})".format(
                    prog["name"],
                    process.pid,
                ))
                process.kill()


def main(args):
    logger = _initialize_logger(args.verbose)
    # Add handlers for SIGTERM and SIGHUP to terminate child processes
    try:
        signal.signal(signal.SIGTERM, _exit_handler)
        signal.signal(signal.SIGHUP, _exit_handler)
    except ValueError:
        pass
    # Put all replaceable args in a dictionary, used as kwargs for format()
    args_param = {
        "upcnport": args.upcnport,
        "connectpubport": args.connectpubport,
        "connectrepport": args.connectrepport,
        "proxyport": args.proxyport,
        "proxyverbose": "-v" if args.verbose > 1 else "",
        "upcndevice": next(iter(glob.glob(args.upcndevice)), None)
    }
    instances = RRND_TOOLCHAIN
    try:
        _spawn_processes(
            logger,
            instances,
            args_param,
            args.count,
            args.toolchain,
            args.quieter,
        )
        _wait_for_processes(logger, instances)
    except KeyboardInterrupt:
        # Exit cleanly on SIGINT
        pass
    finally:
        _terminate_processes(logger, instances)


def _exit_handler(signum, frame):
    raise SystemExit(0)


def _initialize_logger(verbosity):
    logging.basicConfig(
        level={
            0: logging.WARN,
            1: logging.INFO,
        }.get(verbosity, logging.DEBUG),
        format="%(asctime)-23s %(levelname)s: %(message)s",
    )
    return logging.getLogger(sys.argv[0])


def _get_argument_parser():
    parser = argparse.ArgumentParser(
        description="Tool for spawning uPCN instances for the discovery tests"
    )
    parser.add_argument(
        "-t", "--toolchain",
        default="posix",
        choices=["posix", "stm32", "local"],
        help="type of toolchain to be spawned",
    )
    parser.add_argument(
        "-c", "--count",
        default=1,
        type=int,
        help="count of processes to be spawned",
    )
    parser.add_argument(
        "-d", "--dir",
        default=os.path.join(
            os.path.dirname(os.path.realpath(__file__)), "../.."
        ),
        help="directory of the uPCN distribution",
    )
    parser.add_argument(
        "--upcnport",
        default=42100,
        type=int,
        help="first port to be used for uPCN",
    )
    parser.add_argument(
        "--connectpubport",
        default=42200,
        type=int,
        help="first port to be used for the upcn_connect publisher",
    )
    parser.add_argument(
        "--connectrepport",
        default=42300,
        type=int,
        help="first port to be used for the upcn_connect reply socket",
    )
    parser.add_argument(
        "--proxyport",
        default=7763,
        type=int,
        help="first port to be used for rrnd_proxy",
    )
    parser.add_argument(
        "--upcndevice",
        default="/dev/serial/by-id/*STM32_Virtual*",
        help="device file of the uC uPCN runs on",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="count",
        default=0,
        help="increase output verbosity",
    )
    parser.add_argument(
        "-q", "--quieter",
        action="count",
        default=0,
        help="do not show STDOUT (one) / STDERR (two) of spawned processes",
    )
    return parser


if __name__ == "__main__":
    main(_get_argument_parser().parse_args())
