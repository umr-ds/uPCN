import sys
import datetime
import numpy as np
from scipy import optimize

try:
    import ephem
    pyephem_ok = True
except ImportError:
    pyephem_ok = False

try:
    from skyfield.api import load, utc, Angle, Topos, EarthSatellite
except ImportError:
    if not pyephem_ok:
        raise ImportError(
            "Either pyephem or python-skyfield have to be available!"
        )


class ContactPlanGenerator(object):
    """
    An extended version of the core functionality of gencplan-rr-2.py:
    - Enhanced contact prediction approach using NumPy and SciPy
    - Can use either python-skyfield (newer) or pyephem (C library, faster)
    - Direction-dependent minimum elevation possible
    """

    def __init__(self, library="pyephem", ephemeris="de421.bsp"):
        self.library = library
        self.ephemeris = ephemeris
        if self.library == "skyfield":
            planets = load(self.ephemeris)
            self.earth = planets["earth"]
            self.ts = load.timescale()
        else:
            self.earth = None

    def cplan_from_scenario(
            self, sats, gs, start_time, duration, precision=0.1,
            half_period=2700.0, step=50.0, min_duration=0.0, io=True):
        """
        Creates a contact plan as a list of tuples of the form
        (nodeA, nodeB, start, end) for the given scenario.

        Arguments:
            - sats: [{"id": "abc", "tle": "..."}, ...]
            - gs: [{"id": "abc", "lat": <rad>, "lon": <rad>,
                    "p": [0,1], "hot": <bool>, "minelev": <rad|func(az)>}, ...]
            - start_time: UNIX timestamp
            - duration: (in seconds)
            - precision: fitting / maximization precision, in seconds
            - half_period: assumed half orbit period for fitting, in seconds
            - step: simulation step, in seconds
            - min_duration: minimum contact duration, in seconds
        """

        # TODO
        # * RRp (prob.)
        #   - allow probabilistic characteristics for individual links,
        #     not only per ground station?
        # * RRs (ISLs)
        #   - https://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection
        #   - use ECEF coordinates!

        def _init_with_hotspots(gs, duration):
            # interconnect all "hot" gs with one another for [0, duration]
            hot = [e for e in gs if e["hot"]]
            return [
                (e["id"], f["id"], 0, duration, 1.0)
                for e in hot for f in hot if e["id"] != f["id"]
            ]

        result = _init_with_hotspots(gs, duration)

        w_sats = [
            (e["id"], self._create_sat(e["id"], e["tle"]))
            for e in sats
        ]
        w_gs = [
            (e["id"], e["p"], e["minelev"],
             self._create_gs(e["lat"], e["lon"]))
            for e in gs
        ]

        # Python maximization and solving for zeros loosely taken from here:
        # https://github.com/skyfielders/astronomy-notebooks/

        if self.library == "skyfield":
            _altaz = self._altaz_skyfield
        else:
            _altaz = self._altaz_pyephem

        def _elevation_over_min(gsobj, satobj, minelev, x):
            alt, az = _altaz(gsobj, satobj, x)
            return alt - (minelev(az) if callable(minelev) else minelev)

        def _maximize_elev(gsobj, satobj, minelev, rough_time,
                           delta, precision):
            bracket_interval = [
                rough_time + delta,
                rough_time,
                rough_time - delta
            ]
            return optimize.minimize_scalar(
                lambda x: -_elevation_over_min(gsobj, satobj, minelev, x),
                bracket=bracket_interval,
                tol=precision,
            ).x

        def _get_elev_zeros(gsobj, satobj, minelev,
                            rough_time, half_period, precision):
            return (
                optimize.brentq(
                    lambda x: _elevation_over_min(gsobj, satobj, minelev, x),
                    rough_time - half_period,
                    rough_time,
                ),
                optimize.brentq(
                    lambda x: _elevation_over_min(gsobj, satobj, minelev, x),
                    rough_time + half_period,
                    rough_time,
                ),
            )

        times = np.arange(start_time, start_time + duration, step)
        # For each Sat-GS combination, this calculates the maxima of the
        # elevation function and the zeros surrounding them, which identify
        # the contact start and end times.
        for satnum, (satid, satobj) in enumerate(w_sats):
            if io:
                print(
                    "Generating CPlan for satellite: {} / {} ...".format(
                        satnum + 1,
                        len(w_sats),
                    ),
                    file=sys.stderr,
                    end="\r",
                )
            for gsid, gsprob, gsminelev, gsobj in w_gs:
                # First, find "rough maxima" where the sign of the difference
                # between consecutive array elements changes.
                rough_alt = [
                    _elevation_over_min(gsobj, satobj, gsminelev, d)
                    for d in times
                ]
                ldiff = np.ediff1d(rough_alt, to_begin=0.0)
                rdiff = np.ediff1d(rough_alt, to_end=0.0)
                rough_maxima = times[(ldiff > 0.0) & (rdiff < 0.0)]
                # Secondly, maximize the altitudes with sufficient precision.
                precise_maxima = [
                    _maximize_elev(gsobj, satobj, gsminelev, x,
                                   step, precision)
                    for x in rough_maxima
                ]
                # Thirdly, reduce the set of maxima to those which reach the
                # minimum altitude, i.e. are above zero.
                contact_maxima = [
                    x for x in precise_maxima
                    if _elevation_over_min(gsobj, satobj, gsminelev, x) > 0.0
                ]
                # Fourthly, determine the zeros surrounding the maxima and
                # store them in the list of resulting contacts.
                contacts = [
                    _get_elev_zeros(gsobj, satobj, gsminelev, x,
                                    half_period, precision)
                    for x in contact_maxima
                ]
                result += [
                    (gsid, satid, start, end,
                     gsprob(start) if callable(gsprob) else gsprob)
                    for start, end in contacts
                    if end >= start and (end - start) >= min_duration
                ]
        if io:
            print(
                "Generating CPlan for satellite: {0} / {0} [ OK ]".format(
                    len(w_sats),
                ),
                file=sys.stderr,
            )

        return result

    def _create_sat(self, id_, tle):
        tle = tle.strip().split("\n")[-2:]
        if self.library == "skyfield":
            sat = self.earth + EarthSatellite(*tle)
        else:
            sat = ephem.readtle(id_, *tle)
        return sat

    def _create_gs(self, lat, lon):
        if self.library == "skyfield":
            obs = self.earth + Topos(
                Angle(degrees=lat), Angle(degrees=lon)
            )
        else:
            obs = ephem.Observer()
            obs.lat = str(lat)
            obs.lon = str(lon)
        return obs

    def _altaz_skyfield(self, gsobj, satobj, unixtime):
        time = self.ts.utc(
            datetime.datetime.fromtimestamp(unixtime, tz=utc)
        )
        altaz = gsobj.at(time).observe(satobj).apparent().altaz()
        return altaz[0].radians, altaz[1].radians

    def _altaz_pyephem(self, gsobj, satobj, unixtime):
        gsobj.date = datetime.datetime.utcfromtimestamp(unixtime)
        satobj.compute(gsobj)
        return satobj.alt, satobj.az
