#!/usr/bin/env python

import argparse
import json
import logging
import math
import matplotlib

# Max difference in seconds for a match
MAX_CTIME_DIFF = 1800


def run():
    parser = argparse.ArgumentParser(description="Evaluate loc-pred accuracy.")
    parser.add_argument("FILE", type=argparse.FileType("r"),
                        help="the file to be read")
    parser.add_argument("-s", "--stat", choices=["latitude", "longitude"],
                        help="the selected location stat")
    parser.add_argument("--backend", default="Qt5Agg",
                        choices=matplotlib.rcsetup.interactive_bk,
                        help="the matplotlib backend to be used")
    parser.add_argument("-v", "--verbose", action="count", default=0)
    args = parser.parse_args()
    logging.basicConfig(level={0: logging.INFO}.get(
        args.verbose, logging.DEBUG),
        format="%(asctime)-23s %(levelname)s - %(name)s: %(message)s")
    logger = logging.getLogger(__name__)
    try:
        data = json.load(args.FILE)
    finally:
        args.FILE.close()
    xval = []
    yval1 = []
    yval2 = []
    loc_index = 0 if args.stat == "latitude" else 1
    RADTODEG = 180.0 / math.pi
    for run in data["run_data"]:
        for gs_eid, gs in run.items():
            obs_c = 0
            res_c = 0
            for obs in gs["observations"]:
                obs_c += 1
                if (obs == {} or math.isnan(obs["coords"][0]) or
                        obs["prediction"] is None):
                    continue
                loc_diff = (obs["coords"][loc_index] * RADTODEG -
                            gs["coords"][loc_index])
                pred = obs["prediction"]
                p1 = pred[0] / 1000.0
                p2 = (pred[0] + pred[1]) / 1000.0
                for c_intv in gs["contacts"]:
                    c1 = c_intv[0] / 1000.0
                    c2 = c_intv[1] / 1000.0
                    if (_interval_match((c1, c2), (p1, p2)) > 0 or
                            _ct_match((c1, c2), (p1, p2))):
                        xval.append(loc_diff)
                        yval1.append(p1 - c1)
                        yval2.append(p2 - c2)
                        res_c += 1
                        break
            logger.debug("GS {}: {} obs, {} cts".format(gs_eid, obs_c, res_c))
    logger.info("{} records found".format(len(xval)))
    matplotlib.use(args.backend)
    import matplotlib.pyplot as plt
    plt.plot(xval, yval1, "b+")
    plt.plot(xval, yval2, "r+")
    plt.show()


def _interval_match(a, b):
    assert a[1] >= a[0]
    assert b[1] >= b[0]
    i0 = max(a[0], b[0])
    i1 = min(a[1], b[1])
    if i0 > i1 or a[1] == a[0]:
        return 0.0
    else:
        return float(i1 - i0) / float(a[1] - a[0])


def _ct_match(a, b):
    assert a[1] >= a[0]
    assert b[1] >= b[0]
    return max(abs(a[0] - b[0]), abs(a[1] - b[1])) <= MAX_CTIME_DIFF


if __name__ == "__main__":
    run()
