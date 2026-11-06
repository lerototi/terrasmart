#include <unity.h>
#include <string.h>
#include "terrasmart_data_v1.h"

using namespace terras_smart_data_v1;

static Frame sample() {
  Frame f = {0x12345678, {0x24,0x6f,0x28,1,2,3}, {0x38,0x3e,0x51,4,5,6},
             0xabcdef01, 0x12345678, 1, {0x1234, 25, -12.5f}};
  return f;
}
void test_known_wire_and_decode() {
  Frame f = sample(), decoded = {};
  const uint8_t expected[] = {0x54,0x53,1,1,0x78,0x56,0x34,0x12,
    0x24,0x6f,0x28,1,2,3,0x38,0x3e,0x51,4,5,6,
    0x01,0xef,0xcd,0xab,0x78,0x56,0x34,0x12,1,7,0x34,0x12,25,0,0,0x48,0xc1};
  uint8_t wire[FRAME_SIZE];
  TEST_ASSERT_TRUE(encode(f, wire, sizeof(wire)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, wire, FRAME_SIZE);
  TEST_ASSERT_TRUE(decode(wire, sizeof(wire), f.networkId, f.destination, decoded));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(f.source, decoded.source, 6);
  TEST_ASSERT_EQUAL_UINT32(f.bootId, decoded.bootId);
  TEST_ASSERT_EQUAL_UINT32(f.sequence, decoded.sequence);
  TEST_ASSERT_EQUAL_UINT16(f.reading.id, decoded.reading.id);
  TEST_ASSERT_EQUAL_UINT8(f.reading.type, decoded.reading.type);
  TEST_ASSERT_EQUAL_FLOAT(f.reading.data, decoded.reading.data);
}
void test_rejects_bad_envelopes() {
  Frame f = sample(), out = {};
  uint8_t wire[FRAME_SIZE + 1] = {};
  TEST_ASSERT_TRUE(encode(f, wire, sizeof(wire)));
  TEST_ASSERT_FALSE(encode(f, wire, FRAME_SIZE - 1));
  for (size_t n = 0; n < FRAME_SIZE; ++n)
    TEST_ASSERT_FALSE(decode(wire, n, f.networkId, f.destination, out));
  TEST_ASSERT_FALSE(decode(nullptr, FRAME_SIZE, f.networkId, f.destination, out));
  TEST_ASSERT_FALSE(decode(wire, sizeof(wire), f.networkId, f.destination, out));
  TEST_ASSERT_FALSE(decode(wire, FRAME_SIZE, 0, f.destination, out));
  uint8_t wrong[6]; memcpy(wrong, f.destination, 6); wrong[5]++;
  TEST_ASSERT_FALSE(decode(wire, FRAME_SIZE, f.networkId, wrong, out));
  const size_t offsets[] = {0, 1, 2, 3, 28, 29};
  for (size_t i = 0; i < sizeof(offsets)/sizeof(offsets[0]); ++i) {
    const uint8_t before = wire[offsets[i]]; wire[offsets[i]] ^= 0xff;
    TEST_ASSERT_FALSE(decode(wire, FRAME_SIZE, f.networkId, f.destination, out));
    wire[offsets[i]] = before;
  }
  f.hopLimit = 0;
  TEST_ASSERT_FALSE(encode(f, wire, sizeof(wire)));
}
int main(int, char **) {
  UNITY_BEGIN(); RUN_TEST(test_known_wire_and_decode); RUN_TEST(test_rejects_bad_envelopes);
  return UNITY_END();
}
