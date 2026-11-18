#include <unity.h>
#include <string.h>
#include "terrasmart_uart_v1.h"
using namespace terras_smart_uart_v1;

void test_roundtrip_fragmented_and_crc() {
  uint8_t payload[PAYLOAD_SIZE], output[PAYLOAD_SIZE] = {};
  for (size_t i = 0; i < sizeof(payload); ++i) payload[i] = static_cast<uint8_t>(i);
  char line[LINE_SIZE + 1]; TEST_ASSERT_TRUE(encode(payload, line, sizeof(line)));
  Parser parser; bool received = false;
  for (size_t i = 0; i < LINE_SIZE; ++i) received = parser.feed(line[i], i * 2, output);
  TEST_ASSERT_TRUE(received); TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, output, sizeof(payload));
  line[8] = line[8] == '0' ? '1' : '0';
  TEST_ASSERT_FALSE(decode(line, LINE_SIZE, output));
  Parser corrupted; Parser::Result result = Parser::Result::NONE;
  for (size_t i = 0; i < LINE_SIZE; ++i) result = corrupted.feedResult(line[i], i, output);
  TEST_ASSERT_TRUE(result == Parser::Result::REJECTED);
}
void test_timeout_resync_and_foreign_lines() {
  uint8_t payload[PAYLOAD_SIZE] = {}, output[PAYLOAD_SIZE]; char line[LINE_SIZE + 1];
  TEST_ASSERT_TRUE(encode(payload, line, sizeof(line)));
  Parser parser;
  const char *foreign = "!TSOTA:1234\n[{\"id\":1}]\n";
  for (size_t i=0; foreign[i]; ++i) TEST_ASSERT_FALSE(parser.feed(foreign[i], i, output));
  for (size_t i=0; i<20; ++i) TEST_ASSERT_FALSE(parser.feed(line[i], 100+i, output));
  parser.expire(2000);
  for (size_t i=20; i<LINE_SIZE; ++i) TEST_ASSERT_FALSE(parser.feed(line[i], 2000+i, output));
  bool received = false;
  for (size_t i=0; i<LINE_SIZE; ++i) received = parser.feed(line[i], 2200+i, output);
  TEST_ASSERT_TRUE(received);
  TEST_ASSERT_FALSE(decode(line, LINE_SIZE-1, output));
  Parser oversized; oversized.feedResult('!', 0, output);
  Parser::Result result = Parser::Result::NONE;
  for (size_t i=0; i<LINE_SIZE; ++i) result = oversized.feedResult('A', i+1, output);
  TEST_ASSERT_TRUE(result == Parser::Result::REJECTED);
  line[3] = '2';
  TEST_ASSERT_FALSE(decode(line, LINE_SIZE, output));
}
int main(int, char **) {
  UNITY_BEGIN(); RUN_TEST(test_roundtrip_fragmented_and_crc);
  RUN_TEST(test_timeout_resync_and_foreign_lines); return UNITY_END();
}
