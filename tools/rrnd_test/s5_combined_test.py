from .nd.test_scenario import TestScenario
from .nd.ground_station import GroundStation, AvailabilityPattern
from .sample_data import SampleData


class CombinedTestScenario(TestScenario):

    RUN_ITEMS = [
        (satname, (2016, 4, 1, 0, 0)) for satname in SampleData.SATELLITES
    ]
    RUN_LABEL = "sat, start_time"

    def _get_scenario(self, run):
        satellite, start_time = CombinedTestScenario.RUN_ITEMS[
            run % len(CombinedTestScenario.RUN_ITEMS)]
        period = 10
        return {
            "t0": start_time,
            "duration": 14,  # SampleData.DEFAULT_DURATION,
            "satellites": [SampleData.get_sat(satellite)],
            "ground_stations": [self._get_gs(i, beacon_period=period)
                                for i in range(SampleData.get_gs_count())]
        }

    def _get_default_runs(self):
        return len(CombinedTestScenario.RUN_ITEMS)

    def _get_gs(self, i, **kwargs):
        brs = [1200, 9600, 100000]
        rels = [0.8, 0.5, 0.2]
        patterns = [
            # [10800, 7200, 10800 + 7200],
            # [200, 2400, 3600],
            # 24h (day/night) pattern
            [0, 43200, 86400]
        ]
        # GS-type
        gst = (i % 3) + 1
        gs_data = SampleData.get_gs(i)
        # prefix EID for plot, etc.
        eid = "GST{0}_{1}".format(gst, gs_data[0])
        # variable bitrate
        kwargs["rx_bitrate"] = brs[(i // 3) % len(brs)]
        # GST2 has lower reliability, GST3 has avail. pattern
        if gst == 2:
            kwargs["reliability"] = rels[(i // 3) % len(rels)]
        elif gst == 3:
            patt = patterns[(i // 3) % len(patterns)]
            kwargs["availability_pattern"] = AvailabilityPattern(*patt)
        return GroundStation(eid, gs_data[1], **kwargs)
