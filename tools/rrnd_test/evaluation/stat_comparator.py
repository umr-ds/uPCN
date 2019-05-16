import abc
import numpy
import logging
import functools

logger = logging.getLogger(__name__)


class StatisticalComparator(object, metaclass=abc.ABCMeta):

    def __init__(self, data, datafilter):
        self.configured = []
        self.measured = []
        self.contacts = []
        self.start_times = []
        assert len(data) > 0
        self.gs_eids = sorted(data[0]["ground_station_data"].keys())
        self.sat_eids = sorted(
            data[0]["ground_station_data"][self.gs_eids[0]]["contacts"].keys()
        )
        for runindex, run in enumerate(data):
            conf_run = []
            meas_run = []
            cont_run = []
            assert len(run["ground_station_data"].keys()) == len(self.gs_eids)
            self.start_times.append(run["start_time"])
            for gs_eid in self.gs_eids:
                gs = run["ground_station_data"][gs_eid]
                assert len(gs["contacts"].keys()) == len(self.sat_eids)
                conf_gs = []
                meas_gs = []
                cont_gs = []
                for sat_eid in self.sat_eids:
                    if not datafilter.matches(sat_eid, gs_eid, runindex):
                        continue
                    sat = {
                        k: v for k, v in gs.items()
                        if k != "contacts" and k != "observations"
                    }
                    sat.update({
                        "contacts": gs["contacts"][sat_eid],
                        "observations": gs["observations"][sat_eid],
                    })
                    assert len(sat["contacts"]) <= len(sat["observations"])
                    conf_gssat, meas_gssat = self._gsrun_data_to_tuple(sat)
                    assert len(conf_gssat) == len(meas_gssat)
                    cont_gssat = gs["contacts"][sat_eid]
                    assert len(conf_gssat) == len(cont_gssat)
                    if len(conf_gssat) != 0:
                        conf_gs.append(conf_gssat)
                        meas_gs.append(meas_gssat)
                        cont_gs.append(cont_gssat)
                assert len(conf_gs) == len(meas_gs)
                if len(conf_gs) != 0:
                    conf_run.append(conf_gs)
                    meas_run.append(meas_gs)
                    cont_run.append(cont_gs)
            assert len(conf_run) > 0
            self.configured.append(conf_run)
            self.measured.append(meas_run)
            self.contacts.append(cont_run)

        assert len(self.configured) > 0

    @abc.abstractmethod
    def _gsrun_data_to_tuple(self, data):
        pass

    def compare(self, over="runs", difference="normal", reduce_to=None,
                time_bucket_width=3600):
        if difference == "normal":
            diff = StatisticalComparator._difference
        elif difference == "relative":
            diff = StatisticalComparator._relative_difference
        elif difference == "unit":
            diff = StatisticalComparator._unit_difference
        else:
            raise Exception(
                "difference has to be one of 'normal', 'relative', 'unit'"
            )
        if over == "runs":
            stats = StatisticalComparator._stats_per_run
        elif over == "gs":
            stats = StatisticalComparator._stats_per_gs
        elif over == "sats":
            stats = StatisticalComparator._stats_per_sat
        elif over == "contacts":
            stats = StatisticalComparator._stats_per_contact
        elif over == "time":
            stats = functools.partial(
                StatisticalComparator._stats_by_time,
                t0=self.start_times,
                bucket_width=time_bucket_width,
            )
        elif over == "all":
            stats = StatisticalComparator._stats_all
        else:
            raise Exception(
                "over has to be one of 'runs', 'gs', 'sats', 'contacts', " +
                "'time', 'all'"
            )
        return stats(
            self.configured,
            self.measured,
            self.contacts,
            lambda c, m: StatisticalComparator._reduce_to(
                diff(c, m),
                reduce_to
            )
        )

    @staticmethod
    def _stats_per_run(c, m, ct, diff):
        result = []
        for i_run in range(len(c)):
            run_result = []
            for i_gs in range(len(c[i_run])):
                for i_sat in range(len(c[i_run][i_gs])):
                    for i_ct in range(len(c[i_run][i_gs][i_sat])):
                        run_result.append(diff(
                            c[i_run][i_gs][i_sat][i_ct],
                            m[i_run][i_gs][i_sat][i_ct]
                        ))
            result.append(run_result)
        return result

    @staticmethod
    def _stats_per_gs(c, m, ct, diff):
        result = []
        for i_gs in range(len(c[0])):
            gs_result = []
            for i_run in range(len(c)):
                assert len(c[i_run]) == len(c[0])
                for i_sat in range(len(c[i_run][i_gs])):
                    for i_ct in range(len(c[i_run][i_gs][i_sat])):
                        gs_result.append(diff(
                            c[i_run][i_gs][i_sat][i_ct],
                            m[i_run][i_gs][i_sat][i_ct]
                        ))
            result.append(gs_result)
        return result

    @staticmethod
    def _stats_per_sat(c, m, ct, diff):
        result = []
        for i_sat in range(len(c[0][0])):
            sat_result = []
            for i_run in range(len(c)):
                assert len(c[i_run][0]) == len(c[0][0])
                for i_gs in range(len(c[i_run])):
                    for i_ct in range(len(c[i_run][i_gs][i_sat])):
                        sat_result.append(diff(
                            c[i_run][i_gs][i_sat][i_ct],
                            m[i_run][i_gs][i_sat][i_ct]
                        ))
            result.append(sat_result)
        return result

    @staticmethod
    def _stats_per_contact(c, m, ct, diff):
        contacts = max([len(l) for l in c[0][0]])
        result = []
        for i_ct in range(contacts):
            ct_result = []
            for i_run in range(len(c)):
                for i_gs in range(len(c[i_run])):
                    for i_sat in range(len(c[i_run][i_gs])):
                        if i_ct < len(c[i_run][i_gs][i_sat]):
                            ct_result.append(diff(
                                c[i_run][i_gs][i_sat][i_ct],
                                m[i_run][i_gs][i_sat][i_ct]
                            ))
            result.append(ct_result)
        return result

    @staticmethod
    def _stats_by_time(c, m, ct, diff, t0, bucket_width):
        results = []
        for i_run in range(len(c)):
            for i_gs in range(len(c[i_run])):
                for i_sat in range(len(c[i_run][i_gs])):
                    for i_ct in range(len(c[i_run][i_gs][i_sat])):
                        results.append((
                            ct[i_run][i_gs][i_sat][i_ct][0] - t0[i_run],
                            diff(
                                c[i_run][i_gs][i_sat][i_ct],
                                m[i_run][i_gs][i_sat][i_ct]
                            )
                        ))
        tmax = bucket_width
        times = [0]
        bucks = [[]]
        c = 0
        for t, v in sorted(results, key=(lambda x: x[0])):
            assert t >= (tmax - bucket_width) or tmax - bucket_width == 0
            if t < tmax:
                bucks[c].append(v)
            else:
                bucks.append([v])
                times.append(tmax / 3600)
                c += 1
                tmax += bucket_width
        return list(zip(times, bucks))

    @staticmethod
    def _stats_all(c, m, ct, diff):
        result = []
        for i_run in range(len(c)):
            for i_gs in range(len(c[i_run])):
                for i_sat in range(len(c[i_run][i_gs])):
                    for i_ct in range(len(c[i_run][i_gs][i_sat])):
                        result.append(diff(
                            c[i_run][i_gs][i_sat][i_ct],
                            m[i_run][i_gs][i_sat][i_ct]
                        ))
        return [result]

    @staticmethod
    def _relative_difference(configured, measured):
        if measured is None:
            return [float('nan')]
        if measured[0] is None:
            return [float('nan')] * len(measured)
        try:
            return numpy.divide(
                numpy.subtract(measured, configured), configured).tolist()
        except Exception as e:
            logger.debug("Can't calculate difference of {} and {}: {}"
                         .format(configured, measured, e))
            return [float('nan')]

    @staticmethod
    def _difference(configured, measured):
        if measured is None:
            return [float('nan')]
        if measured[0] is None:
            return [float('nan')] * len(measured)
        try:
            return numpy.subtract(measured, configured).tolist()
        except Exception as e:
            logger.debug("Can't calculate difference of {} and {}: {}"
                         .format(configured, measured, e))
            return [float('nan')]

    @staticmethod
    def _unit_difference(configured, measured):
        if measured is None:
            return [float('nan')]
        if measured[0] is None:
            return [float('nan')] * len(measured)
        return measured

    @staticmethod
    def _reduce_to(tup, reduce_to=None):
        logger.debug("Reducing  {}".format(tup))
        if type(tup) is not tuple and type(tup) is not list:
            return [tup]
        if reduce_to is None:
            res = list(tup)
        else:
            res = [tup[i] for i in reduce_to]
        return res
