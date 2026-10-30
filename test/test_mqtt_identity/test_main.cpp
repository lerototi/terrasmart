#include <unity.h>
#include <string.h>

#include "terrasmart_mqtt_identity.h"

void test_formats_stable_identifiers_and_status_topic() {
  const uint8_t mac[] = {0x24, 0x6F, 0x28, 0x01, 0xAB, 0xCD};
  char clientId[16] = {};
  char gatewayId[16] = {};
  char statusTopic[64] = {};
  char offlinePayload[56] = {};

  TEST_ASSERT_TRUE(
      terrasSmart::formatMqttClientId(mac, clientId, sizeof(clientId)));
  TEST_ASSERT_TRUE(
      terrasSmart::formatMqttGatewayId(mac, gatewayId, sizeof(gatewayId)));
  TEST_ASSERT_TRUE(terrasSmart::formatMqttStatusTopic(
      mac, statusTopic, sizeof(statusTopic)));
  TEST_ASSERT_TRUE(terrasSmart::formatMqttOfflinePayload(
      gatewayId, offlinePayload, sizeof(offlinePayload)));

  TEST_ASSERT_EQUAL_STRING("ts-246F2801ABCD", clientId);
  TEST_ASSERT_EQUAL_STRING("gw-246F2801ABCD", gatewayId);
  TEST_ASSERT_EQUAL_STRING("terrasmart/gateways/246F2801ABCD/status",
                           statusTopic);
  TEST_ASSERT_EQUAL_STRING(
      "{\"event\":\"offline\",\"gateway_id\":\"gw-246F2801ABCD\"}",
      offlinePayload);
}

void test_different_macs_produce_distinct_mqtt_namespaces() {
  const uint8_t firstMac[] = {0x24, 0x6F, 0x28, 0x01, 0xAB, 0xCD};
  const uint8_t secondMac[] = {0x24, 0x6F, 0x28, 0x01, 0xAB, 0xCE};
  char firstClientId[16] = {};
  char secondClientId[16] = {};
  char firstGatewayId[16] = {};
  char secondGatewayId[16] = {};
  char firstStatusTopic[64] = {};
  char secondStatusTopic[64] = {};

  TEST_ASSERT_TRUE(terrasSmart::formatMqttClientId(
      firstMac, firstClientId, sizeof(firstClientId)));
  TEST_ASSERT_TRUE(terrasSmart::formatMqttClientId(
      secondMac, secondClientId, sizeof(secondClientId)));
  TEST_ASSERT_TRUE(terrasSmart::formatMqttGatewayId(
      firstMac, firstGatewayId, sizeof(firstGatewayId)));
  TEST_ASSERT_TRUE(terrasSmart::formatMqttGatewayId(
      secondMac, secondGatewayId, sizeof(secondGatewayId)));
  TEST_ASSERT_TRUE(terrasSmart::formatMqttStatusTopic(
      firstMac, firstStatusTopic, sizeof(firstStatusTopic)));
  TEST_ASSERT_TRUE(terrasSmart::formatMqttStatusTopic(
      secondMac, secondStatusTopic, sizeof(secondStatusTopic)));

  TEST_ASSERT_TRUE(strcmp(firstClientId, secondClientId) != 0);
  TEST_ASSERT_TRUE(strcmp(firstGatewayId, secondGatewayId) != 0);
  TEST_ASSERT_TRUE(strcmp(firstStatusTopic, secondStatusTopic) != 0);
}

void test_rejects_null_and_insufficient_buffers() {
  const uint8_t mac[] = {0x24, 0x6F, 0x28, 0x01, 0xAB, 0xCD};
  char tooSmall[15] = {};
  char statusTopic[38] = {};
  char tooSmallPayload[8] = {};

  TEST_ASSERT_FALSE(
      terrasSmart::formatMqttClientId(mac, tooSmall, sizeof(tooSmall)));
  TEST_ASSERT_FALSE(
      terrasSmart::formatMqttGatewayId(mac, tooSmall, sizeof(tooSmall)));
  TEST_ASSERT_FALSE(terrasSmart::formatMqttStatusTopic(
      mac, statusTopic, sizeof(statusTopic)));
  TEST_ASSERT_FALSE(terrasSmart::formatMqttClientId(nullptr, tooSmall,
                                                    sizeof(tooSmall)));
  TEST_ASSERT_FALSE(terrasSmart::formatMqttStatusTopic(
      mac, nullptr, sizeof(statusTopic)));
  TEST_ASSERT_FALSE(terrasSmart::formatMqttOfflinePayload(
      "gw-test", tooSmallPayload, sizeof(tooSmallPayload)));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_formats_stable_identifiers_and_status_topic);
  RUN_TEST(test_different_macs_produce_distinct_mqtt_namespaces);
  RUN_TEST(test_rejects_null_and_insufficient_buffers);
  return UNITY_END();
}
