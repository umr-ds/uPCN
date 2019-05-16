import logging
from .stat_comparator import StatisticalComparator

logger = logging.getLogger(__name__)


class PatternComparator(StatisticalComparator):

    def _gsrun_data_to_tuple(self, data):
        conf = []
        meas = []
        patt = data["pattern"]
        if patt is None:
            patt = (float('nan'), float('nan'), float('nan'))
        for obs in data["observations"]:
            conf.append(patt)
            if obs is None or obs == {} or obs["pattern"] is None:
                meas.append((float('nan'), float('nan'), float('nan')))
                continue
            meas.append(obs["pattern"])
        logger.debug("Pattern result: c={}, m={}".format(conf, meas))
        return conf, meas
