from .status import RRNDStatus


class Satellite(object):

    def __init__(self, client, eid, tle, log_beacons=False):
        self.client = client
        self.eid = eid
        self.tle = tle
        self.log_beacons = log_beacons

    def initialize_instance(self, start_timestamp):
        if not self.client.send_reset():
            raise RuntimeError("Could not reset RRND")
        if not self.client.send_tle(self.tle, start_timestamp):
            raise RuntimeError("Could not initialize RRND")

    def send_beacon(self, beacon, unix_timestamp):
        success, code = beacon.send_via(self.client, unix_timestamp)
        # if not success:
        #     logger.warning("Failed sending beacon: {}".format(hex(code)))
        if self.log_beacons:
            if not success:
                print("!", end="")
            elif code == RRNDStatus.UPDATED:
                print("_", end="")
            elif (code & RRNDStatus.DELETE) != 0:
                print("~", end="")
            elif (code & RRNDStatus.UPDATE_LOCATOR_DATA) != 0:
                print("#", end="")
            elif (code & RRNDStatus.UPDATE_AVAILABILITY) != 0:
                print(":", end="")
            elif (code & RRNDStatus.UPDATE_CONTACTS) != 0:
                print("+", end="")
            else:
                print("?", end="")
        return success
