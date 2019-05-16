import numpy


class Beacon(object):

    def __init__(self, eid, seq, period, avail_p=None,
                 br_tx=9600, br_rx=9600, br_rx_std=0):
        rx = (br_rx if br_rx_std == 0
              else numpy.random.normal(br_rx, br_rx_std))
        if rx < 0.2:
            rx = 0.2
        self.eid = eid
        self.seq = seq
        self.period = period
        self.br_tx = br_tx
        self.br_rx = br_rx
        self.avail_p = avail_p

    def send_via(self, client, unix_timestamp):
        avail = (True if self.avail_p is None
                 else self.avail_p.get_available_time(unix_timestamp))
        success, code = client.send_beacon(
            self.seq,
            self.eid,
            unix_timestamp,
            self.period,
            availability=avail,
            bitrate_up=self.br_tx,
            bitrate_down=self.br_rx,
        )
        return success, code
