import logging
import math
from .stat_comparator import StatisticalComparator

logger = logging.getLogger(__name__)


class LocationComparator(StatisticalComparator):

    def _gsrun_data_to_tuple(self, data):
        conf = []
        meas = []
        cloc = data["coords"]
        RADTODEG = 180.0 / math.pi
        for obs in data["observations"]:
            conf.append(cloc)
            if obs is None or obs == {}:
                meas.append((float('nan'), float('nan')))
                continue
            lat, lon = obs["coords"]
            if not math.isnan(lat) and not math.isnan(lon):
                meas.append((lat * RADTODEG, lon * RADTODEG))
            else:
                meas.append((float('nan'), float('nan')))
        logger.debug("Loc result: c={}, m={}".format(conf, meas))
        return conf, meas
