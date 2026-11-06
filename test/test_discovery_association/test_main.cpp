#include <unity.h>
#include <string.h>
#include "terrasmart_discovery_association.h"

using namespace terras_smart_discovery::association;

void test_round_trip_record() {
  const Record source = {{0x24,0x6f,0x28,0x01,0xab,0xcd}, 1, 0x54530101};
  uint8_t wire[WIRE_SIZE] = {};
  TEST_ASSERT_TRUE(encode(source, wire, sizeof(wire)));
  Record decoded = {};
  TEST_ASSERT_TRUE(decode(wire, sizeof(wire), source.networkId, source.channel,
                          decoded) == DecodeResult::OK);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(source.mac, decoded.mac, sizeof(source.mac));
  TEST_ASSERT_EQUAL_UINT8(source.channel, decoded.channel);
  TEST_ASSERT_EQUAL_UINT32(source.networkId, decoded.networkId);
}

void test_rejects_bad_record_and_scope_mismatch() {
  const Record source = {{0x24,0x6f,0x28,0x01,0xab,0xcd}, 1, 0x54530101};
  uint8_t wire[WIRE_SIZE] = {};
  TEST_ASSERT_TRUE(encode(source, wire, sizeof(wire)));
  Record decoded = {};
  TEST_ASSERT_TRUE(decode(wire, sizeof(wire), 0x54530102, 1, decoded) == DecodeResult::WRONG_NETWORK);
  TEST_ASSERT_TRUE(decode(wire, sizeof(wire), source.networkId, 6, decoded) == DecodeResult::WRONG_CHANNEL);
  TEST_ASSERT_TRUE(decode(wire, sizeof(wire) - 1, source.networkId, 1, decoded) == DecodeResult::INVALID);
  wire[8] ^= 1;
  TEST_ASSERT_TRUE(decode(wire, sizeof(wire), source.networkId, 1, decoded) == DecodeResult::INVALID);
  TEST_ASSERT_FALSE(encode(source, wire, sizeof(wire) - 1));
  const Record multicast = {{0x25,0x6f,0x28,0x01,0xab,0xcd}, 1, 1};
  TEST_ASSERT_FALSE(encode(multicast, wire, sizeof(wire)));
}

void test_rejects_empty_and_broadcast_addresses() {
  uint8_t wire[WIRE_SIZE] = {};
  const Record empty = {{0,0,0,0,0,0}, 1, 0x54530101};
  const Record broadcast = {{0xff,0xff,0xff,0xff,0xff,0xff}, 1, 0x54530101};
  const Record invalidChannel = {{0x24,0x6f,0x28,0x01,0xab,0xcd}, 0, 0x54530101};
  TEST_ASSERT_FALSE(encode(empty, wire, sizeof(wire)));
  TEST_ASSERT_FALSE(encode(broadcast, wire, sizeof(wire)));
  TEST_ASSERT_FALSE(encode(invalidChannel, wire, sizeof(wire)));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_round_trip_record);
  RUN_TEST(test_rejects_bad_record_and_scope_mismatch);
  RUN_TEST(test_rejects_empty_and_broadcast_addresses);
  return UNITY_END();
}
