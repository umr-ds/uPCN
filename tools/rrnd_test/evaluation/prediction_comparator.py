import logging
from .stat_comparator import StatisticalComparator

logger = logging.getLogger(__name__)


class PredictionComparator(StatisticalComparator):

    def _gsrun_data_to_tuple(self, data):
        conf = []
        meas = []
        for i in range(len(data["contacts"])):
            contact = data["contacts"][i]
            obs = data["observations"][i]
            rx_mean = data["achieved_rx"][0]
            rx_std = data["achieved_rx"][1]
            if obs is None or obs == {} or obs["prediction"] is None:
                logger.debug("Observation for contact {} is empty".format(i))
                meas.append((float('nan'), float('nan'),
                             float('nan'), float('nan'), float('nan')))
            else:
                pred = obs["prediction"]
                p_end = pred[0] + pred[1]
                if self._interval_match(contact,
                                        (pred[0] / 1000.0,
                                         p_end / 1000.0)) <= 0:
                    logger.debug("Prediction for contact {} not found"
                                 .format(i))
                    meas.append((float('nan'), float('nan'),
                                 float('nan'), float('nan'), float('nan')))
                else:
                    meas.append((pred[0] / 1000.0, p_end / 1000.0,
                                 pred[2], pred[4] / 8 / (pred[1] // 1000),
                                 pred[5] / (pred[1] // 1000)))
                    # rx_mean *= (pred[1] // 1000)
                    # rx_std *= (pred[1] // 1000)
            # NOTE: reliability broken => separate comparator
            conf.append((contact[0], contact[1], 0.0, rx_mean, rx_std))
        logger.debug("Pred result: c={}, m={}".format(conf, meas))
        return conf, meas

    def _interval_match(self, a, b):
        assert a[1] >= a[0]
        assert b[1] >= b[0]
        i0 = max(a[0], b[0])
        i1 = min(a[1], b[1])
        if i0 > i1 or a[1] == a[0]:
            return 0.0
        else:
            return float(i1 - i0) / float(a[1] - a[0])
