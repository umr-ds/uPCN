from .nd.test_scenario import TestScenario
from .nd.ground_station import GroundStation
from .sample_data import SampleData


class ProbabilityInferenceScenario(TestScenario):

    RUN_ITEMS = [1.0, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2]
    RUN_LABEL = "reliability"

    def _get_scenario(self, run):
        rels = ProbabilityInferenceScenario.RUN_ITEMS
        rel = rels[run % len(rels)]
        return {
            "t0": SampleData.DEFAULT_START,
            "duration": 14,
            "satellites": [SampleData.get_sat()],
            "ground_stations": [
                GroundStation(*SampleData.get_gs(gsnum), reliability=rel)
                for gsnum in range(SampleData.get_gs_count())
            ]
        }

    def _get_default_runs(self):
        return len(ProbabilityInferenceScenario.RUN_ITEMS)
