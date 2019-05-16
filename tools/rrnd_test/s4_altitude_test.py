import math
from .nd.test_scenario import TestScenario
from .nd.ground_station import GroundStation
from .sample_data import SampleData


class CriticalAltitudeTestScenario(TestScenario):

    RUN_ITEMS = [0.0, 10.0, 25.0, 45.0]
    RUN_LABEL = "critical_altitude/deg"

    def _get_scenario(self, run):
        alts = CriticalAltitudeTestScenario.RUN_ITEMS
        alt = alts[run % len(alts)] / 180.0 * math.pi
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
        return len(CriticalAltitudeTestScenario.RUN_ITEMS)
