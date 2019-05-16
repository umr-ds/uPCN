#!/usr/bin/env python

import argparse
import json


def run():
    parser = argparse.ArgumentParser(description="Join test results.")
    parser.add_argument("IN1", type=argparse.FileType("r"),
                        help="the first input file")
    parser.add_argument("IN2", type=argparse.FileType("r"),
                        help="the second input file")
    parser.add_argument("OUT", type=argparse.FileType("w"),
                        help="the output file")
    parser.add_argument("-o", "--over", default="runs",
                        choices=["runs", "gs"],
                        help="the entity to join the stats over")
    args = parser.parse_args()
    in1 = json.load(args.IN1)
    in2 = json.load(args.IN2)
    if args.over == "runs":
        in1["run_data"] = in1["run_data"] + in2["run_data"]
        in1["run_items"] = list(range(len(in1["run_data"])))
    else:
        assert len(in1["run_data"]) == len(in2["run_data"])
        for i in range(len(in1["run_data"])):
            for eid in in1["run_data"][i]:
                assert eid in in2["run_data"][i]
                in1["run_data"][i][eid]["contacts"] += (
                    in2["run_data"][i][eid]["contacts"])
                in1["run_data"][i][eid]["observations"] += (
                    in2["run_data"][i][eid]["observations"])
    json.dump(in1, args.OUT)
    for f in (args.IN1, args.IN2):
        f.close()
    args.OUT.close()


if __name__ == "__main__":
    run()
