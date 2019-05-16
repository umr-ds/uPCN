from .nd.test_scenario import TestScenario
from .nd.ground_station import GroundStation, AvailabilityPattern
from .sample_data import SampleData


class ShortTestScenario(TestScenario):

    def _get_scenario(self, run):
        patt = [0, 7200, 10800 + 7200]
        return {
            "t0": SampleData.DEFAULT_START,
            "duration": SampleData.DEFAULT_DURATION,
            "satellites": [SampleData.get_sat()],
            "ground_stations": [
                GroundStation(*SampleData.get_gs(0)),
                GroundStation(*SampleData.get_gs(1), reliability=0.5),
                GroundStation(*SampleData.get_gs(2),
                              availability_pattern=AvailabilityPattern(*patt))
            ]
        }

    def _get_default_runs(self):
        return 1
