#include "pms_parser.h"
#include <cstring> // memcpy
#include <unity.h>

// A real frame captured from the sensor. Checksum verified by hand: the sum of
// bytes [0..29] == 368 == 0x0170, which is what's stored at [30..31]. Kept
// const; every test runs on a fresh copy (see setUp) so mutations from one
// case never leak into the next.
static const uint8_t good_frame[32] = {
    0x42, 0x4D, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x07, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x97, 0x00, 0x01, 0x70,
};

static uint8_t buf[32];

void setUp(void) { memcpy(buf, good_frame, sizeof buf); } // fresh copy per test
void tearDown(void) {}

// --- make_word: byte pair -> 16-bit big-endian word ------------------------
// Decimal literals where the numeric magnitude is the point; hex literals
// where the bit pattern is the point.

void test_make_word_low_byte(void) {
  TEST_ASSERT_EQUAL_HEX16(28, make_word(0x00, 0x1C));
}
void test_make_word_high_byte_promotion(void) {
  // catches a uint8_t return type or a truncated intermediate — the
  // promotion bug your original comment flagged
  TEST_ASSERT_EQUAL_HEX16(256, make_word(0x01, 0x00));
}
void test_make_word_low_byte_not_sign_extended(void) {
  // if the low byte were sign-extended, 0x80 would smear into 0xFF80
  TEST_ASSERT_EQUAL_HEX16(0x0080, make_word(0x00, 0x80));
}
void test_make_word_full_range(void) {
  // catches clipping or a signed-return truncation at the top of the range
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, make_word(0xFF, 0xFF));
}
void test_make_word_ordering(void) {
  // two distinct bytes catch a swapped high/low
  TEST_ASSERT_EQUAL_HEX16(0x1234, make_word(0x12, 0x34));
}

// --- parse_message: extract fields (assumed NOT to validate the checksum) --

void test_parse_start_chars(void) {
  pms_frame frame = parse_message(buf);
  TEST_ASSERT_EQUAL_HEX8(0x42, frame.start_char1);
  TEST_ASSERT_EQUAL_HEX8(0x4D, frame.start_char2);
}
void test_parse_zero_field(void) {
  pms_frame frame = parse_message(buf);
  TEST_ASSERT_EQUAL_HEX16(0x0000, frame.pm_sp_1_0);
}
void test_parse_reads_big_endian(void) {
  // Put a distinctive value in pm_sp_1_0 (bytes [4..5]) and repair the
  // checksum: +0x12 (+18) and +0x34 (+52) take the sum 368 -> 438 == 0x01B6,
  // so only the stored low byte changes (0x70 -> 0xB6). A little-endian read
  // would return 0x3412 here, so this pins the byte order down.
  buf[4] = 0x12;
  buf[5] = 0x34;
  buf[31] = 0xB6;
  pms_frame frame = parse_message(buf);
  TEST_ASSERT_EQUAL_HEX16(0x1234, frame.pm_sp_1_0);
  TEST_ASSERT_TRUE(checksum_ok(buf)); // fixture stays self-consistent
}

// --- checksum_ok: sum of bytes [0..29] must equal the stored word [30..31] --

void test_checksum_accepts_good_frame(void) {
  TEST_ASSERT_TRUE(checksum_ok(buf));
}
void test_checksum_rejects_corrupted_data(void) {
  buf[17] ^= 0xFF; // change a data byte; stored sum stale
  TEST_ASSERT_FALSE(checksum_ok(buf));
}
void test_checksum_rejects_corrupted_checksum(void) {
  buf[31] ^= 0x01; // change stored sum; data untouched
  TEST_ASSERT_FALSE(checksum_ok(buf));
}
void test_parse_independent_of_checksum(void) {
  // Encodes the contract that parse extracts regardless and checksum_ok is
  // the gate. If your parse_message validates internally, drop or adjust this.
  buf[31] ^= 0xFF;
  pms_frame frame = parse_message(buf);
  TEST_ASSERT_EQUAL_HEX8(0x42, frame.start_char1);
  TEST_ASSERT_FALSE(checksum_ok(buf));
}

int main(int, char **) {
  UNITY_BEGIN();

  RUN_TEST(test_make_word_low_byte);
  RUN_TEST(test_make_word_high_byte_promotion);
  RUN_TEST(test_make_word_low_byte_not_sign_extended);
  RUN_TEST(test_make_word_full_range);
  RUN_TEST(test_make_word_ordering);

  RUN_TEST(test_parse_start_chars);
  RUN_TEST(test_parse_zero_field);
  RUN_TEST(test_parse_reads_big_endian);

  RUN_TEST(test_checksum_accepts_good_frame);
  RUN_TEST(test_checksum_rejects_corrupted_data);
  RUN_TEST(test_checksum_rejects_corrupted_checksum);
  RUN_TEST(test_parse_independent_of_checksum);

  return UNITY_END();
}
