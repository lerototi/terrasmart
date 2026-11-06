#include <unity.h>
#include <string.h>
#include "terrasmart_discovery_packet.h"

using namespace terras_smart_discovery;

void test_wire_format_and_sequence() {
  const Packet packet = {PROBE, 1, 0x12345678, 0x89abcdef};
  const uint8_t expected[] = {'T', 'S', 2, 2, 1, 0x78, 0x56, 0x34, 0x12,
                              0xef, 0xcd, 0xab, 0x89};
  uint8_t wire[WIRE_SIZE] = {};
  encode(packet, wire);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, wire, sizeof(expected));
  Packet decoded = {};
  TEST_ASSERT_TRUE(decode(wire, sizeof(wire), 1, 0x12345678, decoded) == DecodeResult::OK);
  TEST_ASSERT_EQUAL_UINT32(0x89abcdef, decoded.sequence);
  TEST_ASSERT_EQUAL_UINT8(PROBE, decoded.type);
}

void test_rejects_malformed_and_other_network() {
  const Packet packet = {BEACON, 1, 0x12345678, 42};
  uint8_t wire[WIRE_SIZE + 1] = {};
  encode(packet, wire);
  Packet decoded = {};
  TEST_ASSERT_TRUE(decode(nullptr, WIRE_SIZE, 1, packet.networkId, decoded) == DecodeResult::INVALID);
  for (size_t length = 0; length < WIRE_SIZE; ++length)
    TEST_ASSERT_TRUE(decode(wire, length, 1, packet.networkId, decoded) == DecodeResult::INVALID);
  TEST_ASSERT_TRUE(decode(wire, sizeof(wire), 1, packet.networkId, decoded) == DecodeResult::INVALID);
  TEST_ASSERT_TRUE(decode(wire, WIRE_SIZE, 1, 0x12345679, decoded) == DecodeResult::WRONG_NETWORK);
  TEST_ASSERT_TRUE(decode(wire, WIRE_SIZE, 6, packet.networkId, decoded) == DecodeResult::INVALID);
  wire[2] = 1;
  TEST_ASSERT_TRUE(decode(wire, WIRE_SIZE, 1, packet.networkId, decoded) == DecodeResult::INVALID);
  wire[2] = VERSION; wire[3] = 0xff;
  TEST_ASSERT_TRUE(decode(wire, WIRE_SIZE, 1, packet.networkId, decoded) == DecodeResult::INVALID);
  wire[3] = BEACON; wire[0] = 'X';
  TEST_ASSERT_TRUE(decode(wire, WIRE_SIZE, 1, packet.networkId, decoded) == DecodeResult::INVALID);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_wire_format_and_sequence);
  RUN_TEST(test_rejects_malformed_and_other_network);
  return UNITY_END();
}
