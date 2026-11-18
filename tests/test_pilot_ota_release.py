import unittest

from scripts.ota_release import PILOT_ENVS, TARGETS, ota_topic


class PilotOtaReleaseTest(unittest.TestCase):
    def test_pilots_never_use_configured_production_topic(self):
        for env in PILOT_ENVS:
            with self.subTest(env=env):
                self.assertEqual(TARGETS[env][0], 0)
                self.assertEqual(ota_topic(env, {"mqtt_topic": "terrasmart/ota"}),
                                 "terrasmart/pilot/ota")

    def test_production_topic_remains_configurable(self):
        self.assertEqual(ota_topic("esp32dev_mqtt_gateway", {}), "terrasmart/ota")
        self.assertEqual(ota_topic("esp32dev_gateway", {"mqtt_topic": "custom/ota"}),
                         "custom/ota")


if __name__ == "__main__":
    unittest.main()
