import logging
import math
from .stat_comparator import StatisticalComparator

logger = logging.getLogger(__name__)


class ReliabilityComparator(StatisticalComparator):

    def _gsrun_data_to_tuple(self, data):
        conf = []
        meas = []
        for obs in data["observations"]:
            if obs is None or obs == {}:
                conf.append((0.0, 0.0))
                meas.append((float('nan'), float('nan')))
                continue
            crel, rel, var = obs["reliability"]
            conf.append((crel, 0.0, 0.0))
            meas.append((rel, math.sqrt(var), crel))
        logger.debug("Rel. result: c={}, m={}".format(conf, meas))
        return conf, meas
