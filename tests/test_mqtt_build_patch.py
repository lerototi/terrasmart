import json
import sys
import unittest
from pathlib import Path


SCRIPTS_DIR = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS_DIR))
from fdrs_build_patches import (
    patch_mqtt_callback,
    patch_mqtt_command_subscription,
    patch_mqtt_connect_identity,
)


class MqttConnectPatchTests(unittest.TestCase):
    def test_patches_legacy_connect_without_last_will(self):
        source = 'if (client.connect("FDRS_GATEWAY", mqtt_user, mqtt_pass)) {}'

        patched = patch_mqtt_connect_identity(source)

        self.assertIn("terrasSmartMqttClientId()", patched)
        self.assertIn("terrasSmartMqttStatusTopic()", patched)
        self.assertIn("terrasSmartMqttOfflinePayload()", patched)
        self.assertNotIn('"FDRS_GATEWAY"', patched)

    def test_patches_legacy_connect_with_static_last_will(self):
        source = (
            'if (client.connect("FDRS_GATEWAY", mqtt_user, mqtt_pass, '
            'TOPIC_STATUS, 0, true, "{\\"event\\":\\"offline\\"}")) {}'
        )

        patched = patch_mqtt_connect_identity(source)

        self.assertIn("terrasSmartMqttStatusTopic()", patched)
        self.assertIn("terrasSmartMqttOfflinePayload()", patched)
        self.assertNotIn("TOPIC_STATUS", patched)

    def test_patch_is_idempotent(self):
        source = (
            "if (client.connect(terrasSmartMqttClientId(), mqtt_user, "
            "mqtt_pass, terrasSmartMqttStatusTopic(), 0, true, "
            "terrasSmartMqttOfflinePayload())) {}"
        )

        self.assertEqual(
            source, patch_mqtt_connect_identity(source)
        )

    def test_ota_release_builder_requires_and_adds_gateway_identity(self):
        script = Path(__file__).resolve().parents[1] / "scripts" / "ota_release.py"
        source = script.read_text(encoding="utf-8")
        self.assertIn('config.get("ota_gateway_id")', source)
        self.assertIn('command["gateway_id"] = gateway_id', source)

    def test_gateway_ota_task_passes_gateway_identity_without_config(self):
        project_dir = Path(__file__).resolve().parents[1]
        tasks = json.loads(
            (project_dir / ".vscode" / "tasks.json").read_text(encoding="utf-8")
        )
        gateway_task = tasks["tasks"][0]
        args = gateway_task["args"]
        gateway_id_index = args.index("--gateway-id")

        self.assertEqual(args[gateway_id_index + 1], "${input:otaGatewayId}")
        docs = (project_dir / "docs" / "ota.md").read_text(encoding="utf-8")
        self.assertIn("--gateway-id gw-246F2801ABCD", docs)

    def test_runtime_config_save_checks_before_reporting_success(self):
        source = (
            Path(__file__).resolve().parents[1] / "src" / "mqtt_gateway.cpp"
        ).read_text(encoding="utf-8")
        handler = source[
            source.index("void handleSave()") : source.index("void handleReset()")
        ]

        self.assertLess(
            handler.index("copyField(validation"),
            handler.index("wifiSsid = requestedWifiSsid"),
        )
        self.assertIn("if (!saveConfiguration(true))", handler)
        self.assertLess(
            handler.index('server.send(500, "text/plain", "Falha ao salvar'),
            handler.index('server.send(200, "text/html"'),
        )

    def test_gateway_command_subscription_uses_individual_topic_only_at_runtime(self):
        source = '''void reconnect_mqtt(short int attempts, bool silent)
{
    if (!silent) DBG("Connecting MQTT...");
    for (short int i = 1; i <= attempts; ++i) {
        if (client.connect("FDRS_GATEWAY", mqtt_user, mqtt_pass)) {
            // Subscribe
            client.subscribe(TOPIC_COMMAND);
            client.subscribe(TERRASMART_OTA_TOPIC);
            return;
        }
    }
}
void reconnect_mqtt(int attempts)
'''

        patched = patch_mqtt_command_subscription(source)

        self.assertIn("client.subscribe(terrasSmartMqttCommandTopic())", patched)
        self.assertIn("client.subscribe(TOPIC_COMMAND)", patched)
        self.assertNotIn("client.subscribe(TOPIC_COMMAND)\n#endif", patched)
        self.assertEqual(patched, patch_mqtt_command_subscription(patched))

    def test_gateway_callback_patch_is_idempotent(self):
        source = '''void mqtt_callback(char *topic, byte *message, unsigned int length)
{
    String incomingString;
}
void begin_mqtt()
{
}
'''
        patched = patch_mqtt_callback(source)
        self.assertIn("terrasSmartMqttCommandTopicMatches(topic)", patched)
        self.assertEqual(patched, patch_mqtt_callback(patched))

    def test_gateway_command_identity_is_checked_before_payload_validation(self):
        header = (
            Path(__file__).resolve().parents[1]
            / "include"
            / "terrasmart_ota.h"
        ).read_text(encoding="utf-8")
        identity_guard = header.index('doc["gateway_id"]')
        payload_validation = header.index("parseUpdateCommand(doc", identity_guard)
        self.assertLess(identity_guard, payload_validation)


if __name__ == "__main__":
    unittest.main()
