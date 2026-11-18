#include <unity.h>
#include "terrasmart_pilot_ota.h"
#include "terrasmart_pilot_uart_lines.h"
#include "terrasmart_uart_v1.h"

void test_topic_isolation() {
  TEST_ASSERT_TRUE(terrasSmartPilotOta::acceptsTopic("terrasmart/pilot/ota"));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsTopic("terrasmart/ota"));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsTopic("terrasmart/pilot/ota/other"));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsTopic(nullptr));
}

void test_local_target_and_identity() {
  TEST_ASSERT_TRUE(terrasSmartPilotOta::acceptsTarget(0));
  TEST_ASSERT_TRUE(terrasSmartPilotOta::acceptsTarget(1));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsTarget(17));
  TEST_ASSERT_TRUE(terrasSmartPilotOta::acceptsGateway("gw-383E51D3E374", "gw-383E51D3E374"));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsGateway("gw-OTHER", "gw-383E51D3E374"));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsGateway(nullptr, "gw-383E51D3E374"));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsGateway("", ""));
}

void test_uart_interleaving_and_resynchronization() {
  using namespace terrasSmartPilotUart;
  Parser parser;
  uint8_t input[terras_smart_uart_v1::PAYLOAD_SIZE] = {};
  char encoded[terras_smart_uart_v1::LINE_SIZE + 1];
  TEST_ASSERT_TRUE(terras_smart_uart_v1::encode(input, encoded, sizeof(encoded)));
  uint32_t now = 0;
  Kind kind = Kind::NONE;
  for (const char *p = encoded; *p; ++p) kind = parser.feed(*p, now++);
  TEST_ASSERT_EQUAL(static_cast<int>(Kind::TELEMETRY), static_cast<int>(kind));
  uint8_t output[sizeof(input)];
  TEST_ASSERT_TRUE(terras_smart_uart_v1::decode(parser.line(), parser.size(), output));
  const char *ack = "!TSOTA:54530104010000010000\r\n";
  for (const char *p = ack; *p; ++p) kind = parser.feed(*p, now++);
  TEST_ASSERT_EQUAL(static_cast<int>(Kind::OTA), static_cast<int>(kind));
  const char *ready = "!TSREADY:12345678:D4E9F402E5EC\n";
  for (const char *p = ready; *p; ++p) kind = parser.feed(*p, now++);
  TEST_ASSERT_EQUAL(static_cast<int>(Kind::READY), static_cast<int>(kind));
  for (unsigned i = 0; i < MAX_LINE + 5; ++i) kind = parser.feed(i == 0 ? '!' : 'A', now++);
  for (const char *p = encoded; *p; ++p) kind = parser.feed(*p, now++);
  TEST_ASSERT_EQUAL(static_cast<int>(Kind::TELEMETRY), static_cast<int>(kind));
  parser.feed('!', now++); parser.feed('X', now++);
  parser.expire(now + TIMEOUT_MS);
  TEST_ASSERT_EQUAL(static_cast<int>(Kind::NONE), static_cast<int>(parser.feed('\n', now + TIMEOUT_MS + 1)));
}

void test_uart_error_classification() {
  using namespace terrasSmartPilotUart;
  Parser parser;
  Kind kind = Kind::NONE;
  const char *line = "!UNKNOWN:123\n";
  for (const char *p = line; *p; ++p) kind = parser.feed(*p, 1);
  TEST_ASSERT_EQUAL(static_cast<int>(Kind::INVALID), static_cast<int>(kind));
  TEST_ASSERT_EQUAL(static_cast<int>(Error::UNKNOWN_PREFIX), static_cast<int>(parser.error()));
  parser.feed('!', 2);
  for (unsigned i = 0; i < MAX_LINE; ++i) kind = parser.feed('A', 2);
  TEST_ASSERT_EQUAL(static_cast<int>(Kind::INVALID), static_cast<int>(kind));
  TEST_ASSERT_EQUAL(static_cast<int>(Error::OVERFLOW), static_cast<int>(parser.error()));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_topic_isolation);
  RUN_TEST(test_local_target_and_identity);
  RUN_TEST(test_uart_interleaving_and_resynchronization);
  RUN_TEST(test_uart_error_classification);
  return UNITY_END();
}
