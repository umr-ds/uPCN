from .nd.test_scenario import TestScenario
from .nd.ground_station import GroundStation
from .sample_data import SampleData
import math


class MultiSatelliteShortTestScenario(TestScenario):

    def _get_scenario(self, run):

        # minimum elevation of 10 deg in rad
        minelev = 10 / 180 * math.pi

        return {
            "t0": SampleData.DEFAULT_START,
            "duration": 21.0,
            "satellites": SampleData.get_multiple_sats(3),
            "ground_stations": [
                GroundStation(*SampleData.get_gs(0), min_elevation=minelev),
                GroundStation(*SampleData.get_gs(1), min_elevation=minelev),
                GroundStation(*SampleData.get_gs(2), min_elevation=minelev,
                              reliability=0.5),
            ]
        }

    def _get_default_runs(self):
        return 1
