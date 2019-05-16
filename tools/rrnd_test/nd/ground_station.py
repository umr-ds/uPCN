import numpy
from math import sqrt, sin, cos, atan2
from logging import getLogger
from .beacon import Beacon

logger = getLogger(__name__)


def difference(configured, measured):
    # return numpy.round(numpy.divide(
    #     numpy.subtract(measured, configured), configured), 6).tolist()
    return numpy.round(numpy.subtract(measured, configured), 6).tolist()


class AvailabilityPattern(object):

    def __init__(self, offset, on_time, period):
        assert period != 0
        assert on_time <= period
        assert offset < period
        self.duration = period
        self.offset = offset % period
        self.on_time = on_time

    def is_available(self, time):
        return self.get_available_time(time) <= self.on_time

    def get_available_time(self, time):
        return (time - self.offset) % self.duration

    def compare(self, pattern):
        assert isinstance(pattern, AvailabilityPattern)
        return (difference(pattern.offset, self.offset),
                difference(pattern.on_time, self.on_time),
                difference(pattern.duration, self.duration))


class GroundStation(object):

    def __init__(self, eid, coords, reliability=1.0, beacon_period=10,
                 min_elevation=0.0, tx_bitrate=9600, rx_bitrate=9600,
                 availability_pattern=None,
                 achieved_rx_mean=None, achieved_rx_std=0):
        assert len(eid) > 0
        assert len(coords) == 2
        assert coords[0] < 1.58 and coords[0] > -1.58
        assert coords[1] < 3.15 and coords[1] > -3.15
        assert beacon_period > 0 and beacon_period <= 60
        if availability_pattern is not None:
            assert isinstance(availability_pattern, AvailabilityPattern)
        self.eid = eid
        self.latitude = coords[0]
        self.longitude = coords[1]
        self.reliability = reliability
        self.beacon_period = beacon_period
        self.min_elevation = min_elevation
        self.tx_bitrate = tx_bitrate
        self.rx_bitrate = rx_bitrate
        self.achieved_rx = (achieved_rx_mean
                            if achieved_rx_mean is not None else rx_bitrate)
        self.achieved_rx_std = achieved_rx_std
        self.pattern = availability_pattern
        self.verified_once = {}
        self.prediction_succeeded_once = {}
        self.last_prediction_in_the_future = {}
        self.beacon_seqnum = 0
        self.data = {
            "eid": eid,
            "coords": coords,
            "beacon_period": beacon_period,
            "tx_bitrate": tx_bitrate,
            "rx_bitrate": rx_bitrate,
            "achieved_rx": [self.achieved_rx, self.achieved_rx_std],
            "pattern": ((availability_pattern.offset,
                         availability_pattern.on_time,
                         availability_pattern.duration)
                        if availability_pattern is not None else None),
            # NOTE: In fact, the following contains a list of _possible_
            # contacts, without the reliability value being honored.
            "contacts": {},
            "observations": {},
        }

    def get_basic_info(self):
        """
        Queries a dict which describes the GS for the Contact Plan Generator.

        Format:
        {"id": "abc", "lat": <rad>, "lon": <rad>,
         "p": [0,1], "hot": <bool>, "minelev": <rad|func(az)>}
        """
        return {
            "id": self.eid,
            "lat": self.latitude,
            "lon": self.longitude,
            "p": self.reliability,
            "hot": False,
            "minelev": self.min_elevation,
        }

    def verify_and_store(self, satid, response, next_contact):
        entry = {}
        try:
            entry = self.verify_response(satid, response, next_contact)
        finally:
            obs_array = self.data["observations"]
            if satid not in obs_array:
                obs_array[satid] = []
            # assert len(obs_array[satid]) == int(next_contact_number)
            obs_array[satid].append(entry)

    def verify_response(self, satid, response, next_contact):
        # TODO: do not store anything in class, return results to test_scenario
        # assert (response["result"] & 128) == 0
        gs = response["gs"]
        assert gs["success"]
        assert gs["eid"] == self.eid
        assert gs["beacon_period"] / 100 == self.beacon_period
        # assert gs["tx_bitrate"] / 8 == self.tx_bitrate
        # assert gs["rx_bitrate"] / 8 == self.rx_bitrate
        # Check location determination accuracy
        lat, lon, alt = GroundStation.ecef2lla(*gs["location"])
        alt /= 1000.0
        topo_difference = difference(
            [lat, lon, abs(alt)],
            [self.latitude, self.longitude, 0],
        )
        # Check availability pattern
        pattern_diff = None
        pattern_result = None
        if self.pattern is not None:
            try:
                pattern_meas = AvailabilityPattern(
                    *[x // 1000 for x in gs["ap"]])
                pattern_diff = self.pattern.compare(pattern_meas)
                pattern_result = (
                    pattern_meas.offset,
                    pattern_meas.on_time,
                    pattern_meas.duration
                )
            except Exception:
                pass
        # Check reported last contact times
        last_contact = self.data["contacts"][satid][-1]
        if last_contact[0] is not None and last_contact[1] is not None:
            contact_diff = difference(
                numpy.divide([
                    gs["last_contact_start_time"],
                    gs["last_contact_end_time"]
                ], 1000.0),
                list(last_contact),
            )
        else:
            contact_diff = None
        next_start, next_end = next_contact
        cur_reliability = (
            self.reliability(next_start)
            if callable(self.reliability) else self.reliability
        )
        reliability_diff = difference(
            gs["reliability"],
            cur_reliability,
        )
        # Check if and how much the next prediction matches the next contact
        if gs["next_prediction"][0] != 0 and next_start is not None:
            # TODO: Accumulate predictions and check them at the end!
            logger.debug("GS {} - Prediction: {}".format(
                self.eid, gs["next_prediction"]))
            prediction_diff = int(gs["next_prediction"][0] / 1000 - next_start)
            if prediction_diff < -1800:
                self.last_prediction_in_the_future[satid] = True
            else:
                self.last_prediction_in_the_future[satid] = False
            reliability_diff = difference(
                gs["next_prediction"][2],
                cur_reliability,
            )
            self.prediction_succeeded_once[satid] = True
        else:
            gs["next_prediction"] = None
            prediction_diff = None
        diffs = (
            reliability_diff,
            topo_difference,
            pattern_diff,
            contact_diff,
            prediction_diff,
        )
        # Report results
        logger.debug(
            "GS {} - DIFF: R={}, T={}, A={}(s), C={}(s), P={}(s)".format(
                self.eid,
                *diffs
            )
        )
        self.verified_once[satid] = True
        return {
            "coords": (lat, lon),
            "prediction": gs["next_prediction"],
            "pattern": pattern_result,
            "reliability": (
                cur_reliability,
                gs["reliability"],
                gs["variance"],
            ),
        }

    def add_contact(self, satid, contact):
        if satid not in self.data["contacts"]:
            self.data["contacts"][satid] = []
        # assert len(self.data["contacts"][satid]) == int(number)
        self.data["contacts"][satid].append(contact)

    def get_collected_data(self):
        return self.data

    def get_min_elevation(self, az):
        if callable(self.min_elevation):
            return self.min_elevation(az)
        else:
            return self.min_elevation

    def is_last_prediction_in_the_future(self, satid):
        return self.last_prediction_in_the_future.get(satid, False)

    def is_verified_once(self, satid):
        return self.verified_once.get(satid, False)

    def is_prediction_succeeded_once(self, satid):
        return self.prediction_succeeded_once.get(satid, False)

    def get_next_beacon(self, sat):
        new_beacon = Beacon(
            self.eid,
            self.beacon_seqnum,
            self.beacon_period,
            avail_p=self.pattern,
            br_tx=self.tx_bitrate,
            br_rx=self.achieved_rx,
            br_rx_std=self.achieved_rx_std,
        )
        self.beacon_seqnum += 1
        return new_beacon

    # From geodesy JS library by Chris Veness, MIT licensed
    # https://github.com/chrisveness/geodesy/blob/master/latlon-ellipsoidal.js
    @staticmethod
    def ecef2lla(x, y, z):
        try:
            # WGS84
            a = 6378.137
            b = 6356.75231425

            e2 = (a * a - b * b) / (a * a)    # 1st eccentricity squared
            eps2 = (a * a - b * b) / (b * b)  # 2nd eccentricity squared
            p = sqrt(x * x + y * y)           # distance from minor axis
            R = sqrt(p * p + z * z)           # polar radius

            # parametric latitude
            # (Bowring eqn 17, replacing tan_beta = z·a / p·b)
            tan_beta = (b * z) / (a * p) * (1 + eps2 * b / R)
            sin_beta = tan_beta / sqrt(1 + tan_beta * tan_beta)
            cos_beta = sin_beta / tan_beta

            # geodetic latitude (Bowring eqn 18)
            phi = atan2(z + eps2 * b * sin_beta * sin_beta * sin_beta,
                        p - e2 * a * cos_beta * cos_beta * cos_beta)

            # longitude
            lamb = atan2(y, x)

            # height above ellipsoid (Bowring eqn 7)
            sin_phi = sin(phi)
            cos_phi = cos(phi)
            # length of the normal terminated by the minor axis
            length = a / sqrt(1 - e2 * sin_phi * sin_phi)
            h = p * cos_phi + z * sin_phi - (a * a / length)

            return (phi, lamb, h)
        except Exception:
            return (float('nan'), float('nan'), float('nan'))
