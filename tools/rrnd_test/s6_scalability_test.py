from .nd.test_scenario import TestScenario
from .nd.ground_station import GroundStation
from .sample_data import SampleData


class ScalabilityTestScenario(TestScenario):

    RUN_ITEMS = [1, 5, 20]
    RUN_LABEL = "gs_count"

    def _get_scenario(self, run):
        gsnum = ScalabilityTestScenario.RUN_ITEMS[
            run % len(ScalabilityTestScenario.RUN_ITEMS)]
        if self.param is not None:
            gsnum = self.param
        return {
            "t0": SampleData.DEFAULT_START,
            "duration": SampleData.DEFAULT_DURATION,
            "satellites": [SampleData.get_sat()],
            "ground_stations": [
                GroundStation(*SampleData.get_gs(i))
                for i in range(gsnum)
            ]
        }

    def _get_default_runs(self):
        return (len(ScalabilityTestScenario.RUN_ITEMS)
                if self.param is None else 1)
