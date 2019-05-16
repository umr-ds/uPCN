#!/usr/bin/env python

import argparse
import json
import operator
import re
import sys
import inspect

CHECKS = [
    (
        "sent_beacons_by_sat",
        operator.eq,
        "sent_beacons_by_gs"
    ),
    (
        "failed_beacons_by_sat",
        operator.eq,
        "failed_beacons_by_gs"
    ),
    (
        "requested_predictions_by_sat",
        operator.eq,
        "requested_predictions_by_gs"
    ),
    (
        "failed_predictions_by_sat",
        operator.eq,
        "failed_predictions_by_gs"
    ),
    (
        "failed_verifications_by_sat",
        operator.eq,
        "failed_verifications_by_gs"
    ),
    (
        "failed_beacons_by_sat",
        operator.le,
        "sent_beacons_by_sat"
    ),
    (
        "failed_predictions_by_sat",
        operator.le,
        "requested_predictions_by_sat"
    ),
    (
        "failed_verifications_by_sat",
        operator.le,
        lambda data, args: args.failedgsmax
    ),
    (
        lambda data, args: (data["failed_beacons_by_sat"] * 100 /
                            data["sent_beacons_by_sat"]),
        operator.le,
        lambda data, args: args.failedbeaconmax
    ),
    (
        lambda data, args: (data["failed_predictions_by_sat"] * 100 /
                            data["requested_predictions_by_sat"]),
        operator.le,
        lambda data, args: args.failedpredictionmax
    ),
]


def run():
    parser = argparse.ArgumentParser(description="Check test output file.")
    parser.add_argument("FILE", type=argparse.FileType("r"),
                        help="the file to be read")
    parser.add_argument("--failedgsmax", type=int, default=0,
                        help="the maximum for failed gs count")
    parser.add_argument("--failedbeaconmax", type=float, default=0.1,
                        help="the maximum failed beacon ratio, in percent")
    parser.add_argument("--failedpredictionmax", type=float, default=33.33,
                        help="the maximum failed prediction ratio, in percent")
    parser.add_argument("-v", "--verbose", action="store_true", default=False,
                        help="enable verbose mode")
    args = parser.parse_args()
    try:
        data = json.load(args.FILE)
    finally:
        args.FILE.close()
    cum_data = {}
    failed_checks = 0
    for run_id, run_data in enumerate(data["run_data"]):
        cum_data = {
            key: sum(value.values())
            for key, value in run_data.items()
            if key not in ["ground_station_data", "start_time"]
        }
        for l, op, r in CHECKS:
            arg1 = l(cum_data, args) if callable(l) else cum_data[l]
            arg2 = r(cum_data, args) if callable(r) else cum_data[r]
            result = op(arg1, arg2)
            if not result or args.verbose:
                print("Run #{} {} check ({} <{}> {}) for: {}, {}".format(
                    run_id,
                    "SUCCEEDED" if result else "FAILED",
                    _strip_lambda(inspect.getsource(l)) if callable(l) else l,
                    op.__name__,
                    _strip_lambda(inspect.getsource(r)) if callable(r) else r,
                    arg1,
                    arg2,
                ), file=sys.stderr)
                if not result:
                    failed_checks += 1
    if failed_checks != 0:
        print("FAILED {} check(s)".format(failed_checks))
        sys.exit(1)
    print("Everything OK.")


def _strip_lambda(code):
    return re.sub(r"\s+", " ", code.strip())


if __name__ == "__main__":
    run()
