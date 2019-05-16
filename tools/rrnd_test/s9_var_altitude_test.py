from .nd.test_scenario import TestScenario
from .nd.ground_station import GroundStation
from .sample_data import SampleData
from math import sin, pi


class VariableAltitudeTestScenario(TestScenario):

    RUN_ITEMS = ["c(0 deg)", "c(15 deg)", "sin(0;15 deg)"]
    RUN_LABEL = "critical_altitude"

    def _get_scenario(self, run):
        alts = [0.0, 15.0, VariableAltitudeTestScenario._sinaz]
        alt = alts[run % len(alts)]
        return {
            "t0": SampleData.DEFAULT_START,
            "duration": SampleData.DEFAULT_DURATION,
            "satellites": [SampleData.get_sat()],
            "ground_stations": [
                GroundStation(*SampleData.get_gs(gsnum), min_elevation=alt)
                for gsnum in range(SampleData.get_gs_count())
            ]
        }

    def _get_default_runs(self):
        return len(VariableAltitudeTestScenario.RUN_ITEMS)

    @staticmethod
    def _sinaz(az):
        return 7.5 + 7.5 * sin(az / 180.0 * pi)
