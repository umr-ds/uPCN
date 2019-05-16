import zmq
import logging
import json
from .status import RRNDStatus

logger = logging.getLogger(__name__)

try:
    JSONDecodeError = json.decoder.JSONDecodeError
except AttributeError:
    JSONDecodeError = ValueError


class NDClient(object):

    def __init__(self, zmq_address, context=None, no_reset=False,
                 sndtimeo=200, rcvtimeo=3000):
        self._zmq_ctx = context or zmq.Context.instance()
        self._mq = self._zmq_ctx.socket(zmq.REQ)
        self._mq.linger = 0
        self._mq.SNDTIMEO = sndtimeo
        self._mq.RCVTIMEO = rcvtimeo
        self._mq.connect(zmq_address)
        self._no_reset = no_reset
        logger.debug("Connecting to ZMQ socket: '{}'".format(zmq_address))

    def _request(self, req, default={}):
        response = "<unknown>"
        try:
            # logger.debug("> {}".format(req))
            self._mq.send_json(req)
            response = self._mq.recv_string()
            ret = json.loads(response)
            # logger.debug("< {}".format(ret))
            if "result" not in ret:
                logger.warn("No 'result' key in rcvd object: {}".format(ret))
            elif (ret["result"] & RRNDStatus.FAILED) != 0:
                logger.debug("Request failed: req = {}, fc = {}".format(
                    req,
                    hex(ret["result"]),
                ))
            return ret
        except zmq.ZMQError as e:
            logger.warn("Error processing ZMQ request: {}".format(e))
            return default
        except JSONDecodeError as e:
            logger.warn("Error decoding reponse: {}\n---> Response was: {}"
                        .format(e, response))
            return default

    # str, sec, sec, baud, baud, msec/bool
    def send_beacon(self, seqno, eid, unix_timestamp, period=10,
                    bitrate_up=9600, bitrate_down=9600,
                    availability=True, inet=False,
                    mobile=False, neighbors=[]):
        ts = int(round(unix_timestamp, 0))
        flags = 1 if inet else 0
        if mobile:
            flags = flags | 2
        if availability is not True:
            flags = flags | 4
        beacon_array = [
            int(seqno),
            int(period * 100.0),
            int(round(bitrate_up * 8.0, 0)),
            int(round(bitrate_down * 8.0, 0)),
            int(flags),
            int(availability * 1000.0 if availability is not True else 0),
            neighbors  # no neighbors
        ]
        cmd = {
            "command": "process",
            "time": ts,
            "eid": str(eid),
            "beacon": beacon_array
        }
        res = self._request(cmd)
        if "result" not in res:
            return False, RRNDStatus.FAILED
        elif (res["result"] & RRNDStatus.FAILED) != 0:
            return False, res["result"]
        else:
            return True, res["result"]

    def send_reset(self):
        if self._no_reset:
            logger.info("Skipping RESET as requested")
            return True, None
        return self._request({
                "command": "reset"
            }) == {"result": RRNDStatus.UNCHANGED}

    def send_resetperf(self):
        return self._request({
                "command": "reset_perf"
            }) == {"result": RRNDStatus.UNCHANGED}

    def send_storeperf(self):
        return self._request({
                "command": "store_perf"
            }) == {"result": RRNDStatus.UNCHANGED}

    def send_tle(self, tle, ts):
        tle = [x.strip() for x in tle.split("\n")]
        if len(tle) > 2:
            tle = tle[1:3]
        return self._request({
                "command": "init",
                "tle": "\n".join(tle),
                "time": int(round(ts, 0))
            }) == {"result": RRNDStatus.UNCHANGED}

    def query_gs(self, gs):
        return self._request({
                "command": "get_gs",
                "eid": gs
            })

    def request_prediction(self, gs, time):
        return self._request({
                "command": "next_contact",
                "eid": gs,
                "time": int(round(time, 0))
            })

    def send_update_metrics(self, gs_sender, gs, reliability):
        return self._request({
                "command": "update_prob",
                "eid": gs,
                "source_eid": gs_sender,
                "reliability": reliability,
            })

    def is_connected(self):
        return (self._request({"command": "test"}) ==
                {"result": RRNDStatus.UNCHANGED})

    def disconnect(self):
        self._mq.close()
        self._zmq_ctx.destroy()
