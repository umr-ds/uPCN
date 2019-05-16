import os
import unittest

from ..nd.client import NDClient


class BasicSettingsTest(unittest.TestCase):

    def setUp(self):
        self._client = NDClient(
            os.environ.get("UPCN_ND_PROXY", "tcp://127.0.0.1:7763"))

    def tearDown(self):
        self._client.disconnect()

    def test_connectivity(self):
        self.assertEqual(self._client.is_connected(), True)
