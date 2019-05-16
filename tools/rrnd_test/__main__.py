import os
import json
import argparse
import logging
import sys
from .nd.client import NDClient
from .s1_static_scenario import StaticDiscoveryScenario
from .s2_intermittent_scenario import IntermittentAvailabilityScenario
from .s3_probability_test import ProbabilityInferenceScenario
from .s4_altitude_test import CriticalAltitudeTestScenario
from .s5_combined_test import CombinedTestScenario
from .s6_scalability_test import ScalabilityTestScenario
from .s7_short_test import ShortTestScenario
from .s8_capacity_test import CapacityTestScenario
from .s9_var_altitude_test import VariableAltitudeTestScenario
from .s10_multi_sat_test import MultipleSatelliteTestScenario
from .s11_short_test_multi_sat import MultiSatelliteShortTestScenario

SCENARIOS = {
    "1": StaticDiscoveryScenario,
    "2": IntermittentAvailabilityScenario,
    "3": ProbabilityInferenceScenario,
    "4": CriticalAltitudeTestScenario,
    "5": CombinedTestScenario,
    "6": ScalabilityTestScenario,
    "7": ShortTestScenario,
    "8": CapacityTestScenario,
    "9": VariableAltitudeTestScenario,
    "10": MultipleSatelliteTestScenario,
    "11": MultiSatelliteShortTestScenario,
}

LIBRARIES = ["pyephem", "skyfield"]
DEFAULT_LIBRARY = "pyephem"

env_library = os.environ.get("RRND_LIBRARY", DEFAULT_LIBRARY)
if env_library not in LIBRARIES:
    env_library = DEFAULT_LIBRARY

VERBOSE_LEVELS = {"0": 0, "1": 1, "2": 2, "3": 2, "error": 0, "warn": 0,
                  "warning": 0, "info": 1, "debug": 2,
                  "v": 1, "vv": 2, "vvv": 2, "": 1}
DEFAULT_VERBOSE_LEVEL = 1

env_verbose_level = VERBOSE_LEVELS.get(
    os.environ.get("RRND_LOGLEVEL", str(DEFAULT_VERBOSE_LEVEL))
    .strip().lower(), DEFAULT_VERBOSE_LEVEL)

env_runs = os.environ.get("RRND_RUNS", "0")
if not env_runs.isdigit():
    env_runs = 0
else:
    env_runs = int(env_runs)

parser = argparse.ArgumentParser(description="Run a test scenario.")
parser.add_argument(
    "-t", "--scenario", choices=sorted(SCENARIOS.keys()), required=True,
    help="the selected test scenario")
parser.add_argument(
    "-l", "--library", choices=LIBRARIES, default=env_library,
    help="the astronomic library to be used")
parser.add_argument(
    "-z", "--zmqport",
    type=int,
    default=int(os.environ.get("RRND_PROXY_PORT", "7763")),
    help="the first ZMQ port (of rrnd_proxy) to connect to")
parser.add_argument(
    "-n", "--numclients",
    type=int,
    default=1,
    help="the count of rrnd_proxy instances to connect to")
parser.add_argument(
    "-f", "--file", type=argparse.FileType("w"), default=None,
    help="the file for storing collected data")
parser.add_argument(
    "-r", "--runs", type=int, default=env_runs,
    help="the run count, defaults to a scenario-specific value")
parser.add_argument(
    "-p", "--param", type=int, default=None,
    help="a scenario parameter, if the selected scenario supports it")
parser.add_argument(
    "-e", "--metricsexchange", action="store_true", default=False,
    help="enable the probability metric exchange mechanism for RRND")
parser.add_argument(
    "--zmqhost",
    default=os.environ.get("RRND_PROXY_HOST", "127.0.0.1"),
    help="the host on which rrnd_proxy runs")
parser.add_argument(
    "--skiprestart", action="store_true", default=False,
    help="trust the state of uPCN and skip the restart")
parser.add_argument(
    "-v", "--verbose", action="count", default=env_verbose_level,
    help="increase the output log level")
args = parser.parse_args()

# alt: format="%(asctime)-23s %(levelname)s - %(name)s: %(message)s")
logging.basicConfig(level={
                           0: logging.WARNING,
                           1: logging.INFO,
                           2: logging.DEBUG
                          }.get(args.verbose, logging.DEBUG),
                    format="%(levelname)s - %(message)s")
logger = logging.getLogger(__name__)

clients = [
    NDClient(
        "tcp://{}:{}".format(args.zmqhost, port),
        no_reset=args.skiprestart,
    )
    for port in range(args.zmqport, args.zmqport + args.numclients)
]
ts = SCENARIOS[args.scenario](
    clients,
    args.library,
    param=args.param,
    simulate_metrics_exchange=args.metricsexchange,
    log_beacons=(args.verbose >= 3),
)
if args.skiprestart and (
        (args.runs == 0 and ts._get_default_runs() != 1) or
        args.runs not in [0, 1]):
    logger.critical("Cannot skip restarts in multi-run scenarios!")
    sys.exit(1)
try:
    data = ts.run(args.runs)
    if args.file is not None:
        json.dump({
            "run_items": ts.get_run_items(),
            "run_label": ts.get_run_label(),
            "run_data": data,
        }, args.file)
        logger.info("Results stored to: {}".format(args.file.name))
except KeyboardInterrupt:
    pass
finally:
    if args.file is not None:
        args.file.close()
