from .nd.test_scenario import TestScenario
from .nd.ground_station import GroundStation
from .sample_data import SampleData
import math
from datetime import datetime, timezone


class MultipleSatelliteTestScenario(TestScenario):

    RUN_ITEMS = [0.2, 0.3, 0.4, 0.5]
    RUN_LABEL = "start_reliability"

    def _get_scenario(self, run):

        start_reliability = MultipleSatelliteTestScenario.RUN_ITEMS[
            run % len(MultipleSatelliteTestScenario.RUN_ITEMS)
        ]

        start_dt = (2016, 1, (run % (31 - 9)) + 9, run % 12, 0)
        t0 = int(round((datetime(
            *start_dt, tzinfo=timezone.utc
        ) - datetime(1970, 1, 1, tzinfo=timezone.utc)).total_seconds(), 0))

        def _get_reliability(unixtime):
            if (unixtime - t0) < 86400 * 10:
                return start_reliability
            else:
                return start_reliability + 0.4

        reliability = _get_reliability

        # by inclination:
        # low (1): GALASSIA, VELOX 2
        # medium (2): BEVO 2, AGGIESAT 4, (SOMP)
        # high (3): TIGRISAT, GRIFEX, EXOCUBE, QB50P1, FIREBIRD 4, UKUBE-1,
        #           LEMUR-1, POLYITAN-1, BRITE-PL 2
        sat_list = [SampleData.get_sat(s) for s in [
            # two with low inclination
            "GALASSIA",
            "VELOX 2",
            # rest with high inclination
            "GRIFEX",
            "EXOCUBE",
            "QB50P1",
            "FIREBIRD 4",
            # "TIGRISAT",
            # "UKUBE-1",
            # "BEVO 2",
            # "LEMUR-1",
            # "POLYITAN-1",
            # "BRITE-PL 2",
            # "AGGIESAT 4",
        ]]
        sat_count = min(self.param or 1, len(sat_list))

        # minimum elevation of 10 deg in rad
        minelev = 10 / 180 * math.pi

        gs_list = [
            # two unreliable ones near the equator
            GroundStation(
                *SampleData.get_gs(2),
                reliability=reliability,
                min_elevation=minelev
            ),
            GroundStation(
                *SampleData.get_gs(6),
                reliability=reliability,
                min_elevation=minelev
            ),
            # reliable ones to exchange data
            GroundStation(
                *SampleData.get_gs(20),
                reliability=1.0,
                min_elevation=minelev
            ),
            GroundStation(
                *SampleData.get_gs(4),
                reliability=1.0,
                min_elevation=minelev
            ),
            GroundStation(
                *SampleData.get_gs(21),
                reliability=1.0,
                min_elevation=minelev
            ),
            GroundStation(
                *SampleData.get_gs(12),
                reliability=1.0,
                min_elevation=minelev
            ),
            GroundStation(
                *SampleData.get_gs(10),
                reliability=1.0,
                min_elevation=minelev
            ),
            GroundStation(
                *SampleData.get_gs(11),
                reliability=1.0,
                min_elevation=minelev
            ),
        ]

        return {
            "t0": start_dt,
            "duration": 42.0,
            "satellites": sat_list[0:sat_count],
            "ground_stations": gs_list,
            # [
            #     GroundStation(*SampleData.get_gs(gsnum),
            #                   reliability=reliability)
            #     for gsnum in range(SampleData.get_gs_count())
            # ]
        }

    def _get_default_runs(self):
        return len(MultipleSatelliteTestScenario.RUN_ITEMS)
