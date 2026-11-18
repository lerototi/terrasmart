#include <unity.h>
#include "terrasmart_pilot_ota.h"

void test_topic_isolation() {
  TEST_ASSERT_TRUE(terrasSmartPilotOta::acceptsTopic("terrasmart/pilot/ota"));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsTopic("terrasmart/ota"));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsTopic("terrasmart/pilot/ota/other"));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsTopic(nullptr));
}

void test_local_target_and_identity() {
  TEST_ASSERT_TRUE(terrasSmartPilotOta::acceptsTarget(0));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsTarget(1));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsTarget(17));
  TEST_ASSERT_TRUE(terrasSmartPilotOta::acceptsGateway("gw-383E51D3E374", "gw-383E51D3E374"));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsGateway("gw-OTHER", "gw-383E51D3E374"));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsGateway(nullptr, "gw-383E51D3E374"));
  TEST_ASSERT_FALSE(terrasSmartPilotOta::acceptsGateway("", ""));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_topic_isolation);
  RUN_TEST(test_local_target_and_identity);
  return UNITY_END();
}
