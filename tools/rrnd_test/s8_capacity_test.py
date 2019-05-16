from .nd.test_scenario import TestScenario
from .nd.ground_station import GroundStation
from .sample_data import SampleData


class CapacityTestScenario(TestScenario):

    RUN_ITEMS = [
        [9600, 0],
        [9600, 600],
        [9600, 2400],
        [9600, 4800]
    ]
    RUN_LABEL = "[bitrate, std.dev.]/baud"

    def _get_scenario(self, run):
        br = CapacityTestScenario.RUN_ITEMS[
            run % len(CapacityTestScenario.RUN_ITEMS)]
        return {
            "t0": SampleData.DEFAULT_START,
            "duration": SampleData.DEFAULT_DURATION,
            "satellites": [SampleData.get_sat()],
            "ground_stations": [
                GroundStation(*SampleData.get_gs(gsnum),
                              achieved_rx_mean=br[0], achieved_rx_std=br[1])
                for gsnum in range(SampleData.get_gs_count())
            ]
        }

    def _get_default_runs(self):
        return len(CapacityTestScenario.RUN_ITEMS)
