import abc
import sys
import traceback
import random
import json
import math
from datetime import datetime, timezone
from logging import getLogger
from .status import RRNDStatus
from .satellite import Satellite
from .contact_plan_generator import ContactPlanGenerator

logger = getLogger(__name__)


class TestScenario(object, metaclass=abc.ABCMeta):

    def __init__(self, clients, library, ephemeris="de421.bsp",
                 param=None, simulate_metrics_exchange=False,
                 log_beacons=False):
        self.clients = clients
        self.cplan_generator = ContactPlanGenerator(library, ephemeris)
        self.log_beacons = log_beacons
        self.param = param
        self._simulate_metrics_exchange = simulate_metrics_exchange

    @abc.abstractmethod
    def _get_scenario(self, run):
        """Returns data which concerns the scenario to be executed."""
        pass

    @abc.abstractmethod
    def _get_default_runs(self):
        """Returns the count of runs executed by default."""
        pass

    def run(self, runs=0):
        # Get the count of runs
        if runs is None or runs <= 0:
            runs = self._get_default_runs()
        # Check connection and reset performance data
        logger.info(
            "Performing {} run(s) of '{}', {} connections(s) available".format(
                runs,
                type(self).__name__,
                len(self.clients),
            )
        )
        if not all(c.is_connected() for c in self.clients):
            raise Exception("Could not connect to at least one instance")
        if not all(c.send_resetperf() for c in self.clients):
            raise Exception(
                "Could not reset perf data of at least one instance"
            )
        data = []
        # Perform simulation runs
        for run in range(runs):
            logger.info("Performing run: {}".format(run))
            data.append(self._single_run(self._get_scenario, run))
        # Output results
        for run_number, run_data in enumerate(data):
            logger.info("Run {} results: {}".format(run_number, json.dumps(
                {k: v for k, v in run_data.items()
                 if k != "ground_station_data"},
                indent=4
            )))
        return data

    def _single_run(self, get_scenario_fn, number):
        """
        Executes a single test scenario run as specified in the scenario
        description returned by get_scenario_fn.
        """
        sd = get_scenario_fn(number)
        unixtime = datetime(1970, 1, 1, tzinfo=timezone.utc)
        start_time = int(round((datetime(*sd["t0"], tzinfo=timezone.utc) -
                                unixtime).total_seconds(), 0))
        duration = int(86400 * sd["duration"])
        # 1) generate contact list or get it from scenario data
        sat_list = sd["satellites"]
        gs_list = [gs.get_basic_info() for gs in sd["ground_stations"]]
        logger.info("Scenario defines {} sat(s), {} gs(s), t = {} days".format(
            len(sat_list),
            len(gs_list),
            sd["duration"],
        ))
        contact_list = self.cplan_generator.cplan_from_scenario(
            sat_list,
            gs_list,
            start_time,
            duration,
            io=True,
        )
        # 2) reset and init RRND instances
        logger.info("Initializing {} instance(s)...".format(len(sat_list)))
        sats = {}
        cur_client = 0
        for sat_info in sat_list:
            sat = Satellite(
                self.clients[cur_client],
                sat_info["id"],
                sat_info["tle"],
                log_beacons=self.log_beacons,
            )
            sat.initialize_instance(start_time)
            sats[sat_info["id"]] = sat
            cur_client += 1
        gss = {gs.eid: gs for gs in sd["ground_stations"]}
        # 3) replay contact list (containing correct classes)
        contact_list = [
            (
                gss[gsid],
                sats[satid],
                start,
                end,
                prob,
            )
            for gsid, satid, start, end, prob in contact_list
        ]
        logger.info("Replaying {} contacts in order...".format(
            len(contact_list)),
        )
        run_results = self._replay_contact_list(
            sats,
            gss,
            contact_list,
            start_time,
        )
        # 4) TODO verify all gs again
        # for gs in sd["ground_stations"]:
        #     self.verify_gs(gs)
        # 5) store performance data in upcn/rrnd_proxy
        for sat_eid, sat in sats.items():
            if not sat.client.send_storeperf():
                logger.warn("Could not store perf data")
        # 6) return results
        run_results.update({
            "ground_station_data": {
                gs.eid: gs.data
                for gs in sd["ground_stations"]
            },
            "start_time": start_time,
        })
        return run_results

    def _replay_contact_list(self, sat_list, gs_list,
                             contact_list, scenario_start):
        # contact_list: [(gsid, satid, start, end, gsprob), ...]
        OFFSET = 300
        sat_dict_init = {s: 0 for s in sat_list.keys()}
        gs_dict_init = {gs: 0 for gs in gs_list.keys()}
        stats = {
            "sent_beacons_by_sat": sat_dict_init.copy(),
            "failed_beacons_by_sat": sat_dict_init.copy(),
            "sent_beacons_by_gs": gs_dict_init.copy(),
            "failed_beacons_by_gs": gs_dict_init.copy(),
            "requested_predictions_by_sat": sat_dict_init.copy(),
            "failed_predictions_by_sat": sat_dict_init.copy(),
            "requested_predictions_by_gs": gs_dict_init.copy(),
            "failed_predictions_by_gs": gs_dict_init.copy(),
            "failed_verifications_by_sat": sat_dict_init.copy(),
            "failed_verifications_by_gs": gs_dict_init.copy(),
            "missed_contacts_by_sat": sat_dict_init.copy(),
            "missed_contacts_by_gs": gs_dict_init.copy(),
        }
        # First, order the list by start-time
        contact_list = sorted(contact_list, key=lambda c: c[2])
        # Now traverse the list and simulate contacts
        for gs, sat, start, end, prob in contact_list:
            # First, request a prediction and store the response
            if not gs.is_last_prediction_in_the_future(sat.eid):
                # Query prediction for the following contact (with offset)
                stats["requested_predictions_by_sat"][sat.eid] += 1
                stats["requested_predictions_by_gs"][gs.eid] += 1
                # IMPORTANT: We always need to have the prediction available
                # before the contact!
                ret = sat.client.request_prediction(gs.eid, start - OFFSET)
                if (("result" not in ret or
                        (ret["result"] & RRNDStatus.FAILED) != 0) and
                        gs.is_prediction_succeeded_once(sat.eid)):
                    logger.info(
                        "Prediction failed again for {} on {}, fc = {}".format(
                            gs.eid,
                            sat.eid,
                            hex(ret["result"]),
                        )
                    )
                    stats["failed_predictions_by_sat"][sat.eid] += 1
                    stats["failed_predictions_by_gs"][gs.eid] += 1
            success = self._query_and_check_gs_info(
                gs,
                sat,
                next_contact=(start, end),
            )
            if not success:
                stats["failed_verifications_by_sat"][sat.eid] += 1
                stats["failed_verifications_by_gs"][gs.eid] += 1
            gs.add_contact(sat.eid, (start, end))
            # Second, check whether the contact will happen
            if random.random() > prob:
                logger.debug("Contact with {} will be missed".format(gs.eid))
                stats["missed_contacts_by_sat"][sat.eid] += 1
                stats["missed_contacts_by_gs"][gs.eid] += 1
                continue
            # Third, simulate a metric exchange if requested
            # NOTE: to avoid having to evaluate trust issues,
            # only trusted stations may be used to exchange data
            # reliability == 1 is defined as trusted in this scenario
            if (self._simulate_metrics_exchange and
                    not callable(gs.reliability) and
                    round(gs.reliability) == 1):
                self._exchange_metrics(gs, sat, gs_list)
            # Fourth, simulate the contact
            beac_sent, beac_fail = self._replay_contact(
                gs,
                sat,
                start,
                end,
                scenario_start,
            )
            # Fifth, update statistics
            stats["sent_beacons_by_sat"][sat.eid] += beac_sent
            stats["sent_beacons_by_gs"][gs.eid] += beac_sent
            stats["failed_beacons_by_sat"][sat.eid] += beac_fail
            stats["failed_beacons_by_gs"][gs.eid] += beac_fail
            # TODO [Optimization, Improvement]:
            # Allow concurrent runs of all contacts in list until
            # two whith the same sat OR gs would be executed concurrently.
            # If possible, time should never run backwards for the satellites.
        return stats

    def _replay_contact(self, gs, sat, start, end, scenario_start):
        first_beacon = (start - ((start - scenario_start) % gs.beacon_period) +
                        gs.beacon_period)
        cur_time = first_beacon
        sent_count = 0
        fail_count = 0
        while cur_time <= end:
            sent_count += 1
            success = sat.send_beacon(gs.get_next_beacon(sat.eid), cur_time)
            if not success:
                fail_count += 1
            cur_time += gs.beacon_period
        return sent_count, fail_count

    def _query_and_check_gs_info(self, gs, sat, next_contact):
        try:
            gsdata = sat.client.query_gs(gs.eid)
            gs.verify_and_store(
                sat.eid,
                gsdata,
                next_contact,
            )
        except (AssertionError, KeyError):
            if gs.is_verified_once(sat.eid):
                logger.warning(
                    "GS verification failed again for {} (failed: {})".format(
                        gs.eid,
                        traceback.extract_tb(sys.exc_info()[2])[-1][3],
                    )
                )
                return False
        return True

    def _exchange_metrics(self, gs, sat, gs_list):
        try:
            old_metrics = gs.cached_metrics
        except AttributeError:
            old_metrics = gs.cached_metrics = {}
        new_metrics = {}
        # NOTE: maybe store all received stats from sats (after filtering)
        # * derive standard deviation of values stored and decide whether to
        #   distribute them
        # --- PARAMETERS
        # maximum standard deviation reported by sat to use P value
        EXCHANGE_MAX_STD_DEV = 0.04  # Ratio: 0.015
        EXCHANGE_MIN_STD_DEV = 0.04  # Ratio: 0.015
        # minimum count of updates of P value before distribution
        EXCHANGE_MIN_UPDATES = 3
        # minimum count of observations sat must have made to accept P value
        EXCHANGE_MIN_CONTACTS = 15
        # ---
        for eid, entry in gs_list.items():
            gsdata = sat.client.query_gs(entry.eid)
            if (entry.eid == gs.eid or (gsdata["result"] & 128) or
                    not gsdata["gs"]["success"]):
                continue
            gsdata = gsdata["gs"]
            stddev = math.sqrt(gsdata["variance"])
            if (entry.eid in old_metrics and
                    old_metrics[entry.eid]["update_count"] >=
                    EXCHANGE_MIN_UPDATES and
                    stddev >= EXCHANGE_MIN_STD_DEV):
                logger.debug("{}: Updating P for {} on {}: {}".format(
                    gs.eid,
                    entry.eid,
                    sat.eid,
                    old_metrics[entry.eid]["reliability"],
                ))
                res = sat.client.send_update_metrics(
                    gs.eid,
                    entry.eid,
                    old_metrics[entry.eid]["reliability"],
                )
                if res["result"] & 128:
                    logger.info("P update failed for {} on {}, fc = {}".format(
                        entry.eid,
                        sat.eid,
                        hex(res["result"]),
                    ))
            if (gsdata["history_elements"] >= EXCHANGE_MIN_CONTACTS and
                    stddev < EXCHANGE_MAX_STD_DEV):
                try:
                    new_metrics[entry.eid] = {
                        "reliability": (
                            old_metrics[entry.eid]["reliability"] +
                            gsdata["reliability"]
                        ) / 2,
                        "update_count": (
                            old_metrics[entry.eid]["update_count"] + 1
                        ),
                    }
                except KeyError as e:
                    logger.debug(
                        "{}: Got first P for {} from {}: {}, std={}".format(
                            gs.eid,
                            entry.eid,
                            sat.eid,
                            gsdata["reliability"],
                            stddev,
                        )
                    )
                    new_metrics[entry.eid] = {
                        "reliability": gsdata["reliability"],
                        "update_count": 1,
                    }
        gs.cached_metrics.update(new_metrics)

    def get_run_items(self):
        try:
            return self.RUN_ITEMS
        except AttributeError:
            return list(range(self._get_default_runs()))

    def get_run_label(self):
        try:
            return self.RUN_LABEL
        except AttributeError:
            return "run"
