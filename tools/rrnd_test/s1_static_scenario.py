from .nd.test_scenario import TestScenario
from .nd.ground_station import GroundStation
from .sample_data import SampleData


class StaticDiscoveryScenario(TestScenario):

    RUN_ITEMS = [10, 20, 5, 15, 3, 1]
    RUN_LABEL = "period/s"

    def _get_scenario(self, run):
        periods = StaticDiscoveryScenario.RUN_ITEMS
        period = periods[run % len(periods)]
        return {
            "t0": SampleData.DEFAULT_START,
            "duration": SampleData.DEFAULT_DURATION,
            "satellites": [SampleData.get_sat()],
            "ground_stations": [
                GroundStation(*SampleData.get_gs(gsnum), beacon_period=period)
                for gsnum in range(SampleData.get_gs_count())
            ]
        }

    def _get_default_runs(self):
        return len(StaticDiscoveryScenario.RUN_ITEMS)
