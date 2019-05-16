from .nd.test_scenario import TestScenario
from .nd.ground_station import GroundStation, AvailabilityPattern
from .sample_data import SampleData


class IntermittentAvailabilityScenario(TestScenario):

    PATTERNS = [
        [0, 43200, 86400]
    ]
    RUN_ITEMS = ["P1"]
    RUN_LABEL = "pattern"

    def _get_scenario(self, run):
        patt = IntermittentAvailabilityScenario.PATTERNS[
            run % len(IntermittentAvailabilityScenario.PATTERNS)]
        return {
            "t0": SampleData.DEFAULT_START,
            "duration": 14,
            "satellites": [SampleData.get_sat()],
            "ground_stations": [
                GroundStation(*SampleData.get_gs(gsnum),
                              availability_pattern=AvailabilityPattern(*patt))
                for gsnum in range(SampleData.get_gs_count())
            ]
        }

    def _get_default_runs(self):
        return len(IntermittentAvailabilityScenario.PATTERNS)
