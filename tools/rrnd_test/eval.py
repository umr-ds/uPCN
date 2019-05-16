#!/usr/bin/env python

import argparse
import json
import logging
import math
import numpy
import matplotlib
import matplotlib.pyplot as plt
from evaluation.prediction_comparator import PredictionComparator
from evaluation.location_comparator import LocationComparator
from evaluation.pattern_comparator import PatternComparator
from evaluation.reliability_comparator import ReliabilityComparator
matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

STATS = {
    "prediction": {
        "comparator": PredictionComparator,
        "reduce": [0, 1],
        "names": ["Contact Start", "Contact End"],
        "ylabel": "time difference (seconds)"
    },
    "reliability": {
        "comparator": ReliabilityComparator,
        "reduce": [0],
        "names": ["Contact Probability"],
        "ylabel": "difference inferred - configured"
    },
    "stddev": {
        "comparator": ReliabilityComparator,
        "reduce": [1],
        "names": ["Standard Deviation"],
        "ylabel": "standard deviation"
    },
    "confreliability": {
        "comparator": ReliabilityComparator,
        "reduce": [2],
        "names": ["Configured Reliability"],
        "ylabel": "probability"
    },
    "capacity": {
        "comparator": PredictionComparator,
        "reduce": [3, 4],
        "names": ["Bit Rate Mean", "Bit Rate Standard Deviation"],
        "ylabel": "bit rate difference (bytes)"
    },
    "location": {
        "comparator": LocationComparator,
        "reduce": None,
        "names": ["Latitude", "Longitude"],
        "ylabel": "difference (degrees)"
    },
    "pattern": {
        "comparator": PatternComparator,
        "reduce": None,
        "names": ["Offset", "On-Time", "Duration"],
        "ylabel": "time difference (seconds)"
    },
    "ontime": {
        "comparator": PatternComparator,
        "reduce": [1],
        "names": ["On-Time"],
        "ylabel": "time difference (seconds)"
    }
}

# FIXME: Remove global variable!
rects = []


def run():
    parser = argparse.ArgumentParser(description="Evaluate test results.")
    parser.add_argument("FILE", nargs="+",
                        help="the file(s) to be read")
    parser.add_argument("-s", "--stat",
                        choices=STATS.keys(), default="prediction",
                        help="the statistic to be generated")
    parser.add_argument("-o", "--over", default="runs",
                        choices=["runs", "gs", "sats", "contacts", "time",
                                 "all"],
                        help="the entity to generate a stat over")
    parser.add_argument("-p", "--plot", action="store_true",
                        help="generate a plot of the results")
    parser.add_argument("-r", "--relative", action="store_true",
                        help="use relative differences")
    parser.add_argument("-a", "--absolute", action="store_true",
                        help="show absolute values")
    parser.add_argument("-n", "--nan", action="store_true",
                        help="leave NaNs")
    parser.add_argument("-e", "--errorbars", action="store_true",
                        help="draw error bars instead of boxplots")
    parser.add_argument("-z", "--zeroline", action="store_true",
                        help="draw a zero-line")
    parser.add_argument("--timebucketwidth", type=int, default=(12 * 3600),
                        help="the width of the time buckets, in seconds")
    parser.add_argument("--filtersats", nargs="+", default=[],
                        help="filter for specific satellites")
    parser.add_argument("--filtergs", nargs="+", default=[],
                        help="filter for specific ground stations")
    parser.add_argument("--filterruns", nargs="+", default=[],
                        help="filter for specific runs")
    parser.add_argument("--backend", default="Qt5Agg",
                        choices=matplotlib.rcsetup.interactive_bk,
                        help="the matplotlib backend to be used")
    parser.add_argument("-v", "--verbose", action="count", default=0,
                        help="increase the output log level")
    args = parser.parse_args()
    logging.basicConfig(level={0: logging.INFO}.get(
        args.verbose, logging.DEBUG),
        format="%(asctime)-23s %(levelname)s - %(name)s: %(message)s")
    logger = logging.getLogger(__name__)
    if args.plot:
        matplotlib.use(args.backend)
        axes = None
        ci = 0
        boxcolor = [
            "black",
            tuple(v / 255 for v in (31, 119, 180, 255)),
            tuple(v / 255 for v in (255, 127, 14, 255)),
            "green",
        ]
        linecolor = [
            tuple(v / 255 for v in (255, 127, 14, 255)),
            "green",
            "red",
            "turquoise",
        ]
        linestyle = [
            "--",
            "-",
            "-.",
            ":",
        ]
        formattypes = [
            ".",
            "x",
            "s",
            "p",
        ]
    for f in args.FILE:
        stats_n, xitems, xlabel, stat = process_file(logger, f, args)
        if args.plot:
            axes = do_plot(
                stats_n, xitems,
                xlabel, stat["ylabel"],
                stat["names"], axes,
                boxcolor[ci], linecolor[ci],
                linestyle[ci], formattypes[ci],
                "errorbars" if args.errorbars else "boxplot",
                args.zeroline,
            )
            ci += 1
    if args.plot:
        plt.tight_layout(w_pad=2)
        plt.legend(rects, ["without distribution", "with distribution"],
                   loc="lower right", ncol=1)
        plt.show()


def process_file(logger, file_, args):
    with open(file_, "r") as f:
        data = json.load(f)
    datafilter = DataFilter(args.filtersats, args.filtergs, args.filterruns)
    print_contact_stats(logger, data, datafilter)
    stat = STATS[args.stat]
    comparator = stat["comparator"](
        data["run_data"],
        datafilter,
    )
    # TODO: stat over time: reduce to tuple with start time
    diff_algo = ("unit" if args.absolute
                 else ("relative" if args.relative else "normal"))
    stats = comparator.compare(
        over=args.over,
        reduce_to=stat["reduce"],
        difference=diff_algo,
        time_bucket_width=args.timebucketwidth,
    )
    logger.debug("Stats: \n{}".format(
        json.dumps(stats, indent=4, sort_keys=True)))
    if not args.nan:
        if args.over == "time":
            stats = [(time, strip_nan(l)) for time, l in stats]
        else:
            stats = [strip_nan(l) for l in stats]
    xitems, xlabel = xitems_and_label(args.over, data, stats, comparator)
    stats = [s for (xi, s) in sorted(zip(xitems, stats))]
    xitems = sorted(xitems)
    stats_n = reshape_stats(stats, args.over == "time")
    if args.over == "time":
        stats = [l for _, l in stats]
    np_result = [{
            "mean": numpy.mean(l, axis=0).tolist(),
            "std": numpy.std(l, axis=0).tolist()
        } for l in stats]
    for i in range(len(np_result)):
        try:
            np_result[i]["item"] = xitems[i]
        except Exception as e:
            logger.warning("Cannot assign xitem to result {}: {}".format(i, e))
    logger.info("Result: \n{}".format(
        json.dumps(np_result, indent=4, sort_keys=True)))
    return stats_n, xitems, xlabel, stat


class DataFilter:

    def __init__(self, satfilter, gsfilter, runfilter):
        self.satfilter = satfilter
        self.gsfilter = gsfilter
        self.runfilter = [int(r) for r in runfilter]

    def matches(self, sat, gs, run):
        satmatch = (sat in self.satfilter or len(self.satfilter) == 0)
        gsmatch = (gs in self.gsfilter or len(self.gsfilter) == 0)
        runmatch = (run in self.runfilter or len(self.runfilter) == 0)
        return satmatch and gsmatch and runmatch


def print_contact_stats(logger, data, datafilter):
    contacts = []
    for runindex, run in enumerate(data["run_data"]):
        for gsname, gs in run["ground_station_data"].items():
            for satellite in gs["contacts"]:
                for start, end in gs["contacts"][satellite]:
                    if datafilter.matches(satellite, gsname, runindex):
                        contacts.append((end - start))
    logger.info("Found {} contacts".format(len(contacts)))
    logger.info("Durations: mean={}s, std={}s, min={}s, max={}s".format(
        round(numpy.mean(contacts), 4), round(numpy.std(contacts), 4),
        numpy.min(contacts), numpy.max(contacts)))


def has_no_nan(e):
    for i in e:
        if math.isnan(i):
            return False
    return True


def strip_nan(data):
    return [e for e in data if has_no_nan(e)]


# This splits the innermost results, e.g. for location into lat and lon results
def reshape_stats(stats, contains_time):
    stats_n = []
    for line_index in range(len(stats)):
        line = stats[line_index][1] if contains_time else stats[line_index]
        for sample in line:
            for column_index in range(len(sample)):
                while column_index >= len(stats_n):
                    stats_n.append([])
                while line_index >= len(stats_n[column_index]):
                    stats_n[column_index].append([])
                stats_n[column_index][line_index].append(sample[column_index])
    return stats_n


def do_plot(stats_n, x_labels, x_name, y_name, subplot_titles, axis,
            boxcolor, linecolor, linestyle, formattype, plottype, zeroline):
    global rects
    cols = len(stats_n)
    if axis is None:
        _, axis = plt.subplots(ncols=cols)
        if cols == 1:
            axis = [axis]
    for i in range(len(stats_n)):
        a = axis[i]
        if zeroline:
            a.axhline(0.0, color=(0.0, 0.0, 0.0, 0.1))
        if plottype == "boxplot":
            a.boxplot(
                stats_n[i],
                whis=[5, 95],
                showmeans=True,
                meanline=True,
                boxprops={"color": boxcolor},
                whiskerprops={"color": boxcolor},
                medianprops={"color": linecolor},
                meanprops={"markerfacecolor": linecolor,
                           "markeredgecolor": linecolor,
                           "color": boxcolor,
                           "linestyle": ":"},
                flierprops={"markeredgecolor": boxcolor},
            )
            if len(str(x_labels[0])) > 3:
                a.set_xticklabels(x_labels, rotation=45,
                                  horizontalalignment="right")
            elif len(x_labels) > 15:
                import matplotlib.ticker as ticker
                a.xaxis.set_major_locator(ticker.MultipleLocator(5))
                a.set_xticklabels(x_labels[0::5])
            else:
                a.set_xticklabels(x_labels)
        else:
            eb = a.errorbar(
                x_labels,
                [numpy.mean(e) for e in stats_n[i]],
                [numpy.std(e) for e in stats_n[i]],
                color=boxcolor,
                fmt=formattype,
                capsize=3,
            )
            eb[-1][0].set_linestyle(linestyle)
            rects.append(eb)
        a.set_xlabel(x_name)
        a.set_ylabel(y_name)
        a.set_title(subplot_titles[i])
    return axis


def xitems_and_label(over, data, stats, comparator):
    stat_count = len(stats)
    xitems = range(stat_count)
    xlabel = ""
    if over == "gs":
        xitems = comparator.gs_eids
    elif over == "sats":
        xitems = comparator.sat_eids
    elif over == "runs":
        xitems = data["run_items"]
        xlabel = data["run_label"]
    elif over == "contacts":
        xlabel = "contact"
    elif over == "time":
        xitems = [time for time, _ in stats]
        xlabel = "simulation time (hours)"
    return xitems, xlabel


if __name__ == "__main__":
    run()
