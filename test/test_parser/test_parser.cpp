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

// A real 8-byte ack, captured from the sensor in response to a PassiveMode
// (0xE1 / DATAL 0x00) command. The datasheet's Appendix II documents an answer
// only for 0xE2, but 0xE1 does in fact ack, and the ack IS length-prefixed
// (length field 0x0004 -> 4 + 4 header == 8 bytes) so it flows through
// frame_collector unmodified. Checksum: 0x42+0x4D+0x00+0x04+0xE1+0x00 ==
// 0x0174.
static const uint8_t ack_passive[8] = {
    0x42, 0x4D, 0x00, 0x04, 0xE1, 0x00, 0x01, 0x74,
};

// The same ack for ActiveMode (DATAL 0x01). Confirms empirically that the ack
// echoes DATAL rather than always sending 0x00 -- i.e. an ack tells you WHICH
// mode the sensor entered, not merely that a command was received.
// Checksum: 0x42+0x4D+0x00+0x04+0xE1+0x01 == 0x0175.
static const uint8_t ack_active[8] = {
    0x42, 0x4D, 0x00, 0x04, 0xE1, 0x01, 0x01, 0x75,
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

// --- sum_bytes: plain 16-bit accumulation over the first n bytes ------------
// Extracted from checksum_ok so the verify path and the build path share one
// implementation. These tests pin the two properties that matter: it must
// accumulate in 16 bits (not truncate to 8), and it must respect n exactly.

void test_sum_bytes_empty(void) {
  // n == 0 must not read buffer[0]; the loop should simply not execute
  const uint8_t data[1] = {0xFF};
  TEST_ASSERT_EQUAL_HEX16(0x0000, sum_bytes(data, 0));
}
void test_sum_bytes_simple(void) {
  const uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
  TEST_ASSERT_EQUAL_HEX16(10, sum_bytes(data, 4));
}
void test_sum_bytes_respects_n(void) {
  // stops at n and ignores the tail — the property checksum_ok depends on when
  // it passes n - 2 to exclude the stored checksum word
  const uint8_t data[4] = {0x01, 0x02, 0xFF, 0xFF};
  TEST_ASSERT_EQUAL_HEX16(3, sum_bytes(data, 2));
}
void test_sum_bytes_exceeds_one_byte(void) {
  // 5 * 0xFF == 1275 == 0x04FB. An 8-bit accumulator would wrap to 0xFB and a
  // signed char accumulator would go negative; both show up here.
  const uint8_t data[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  TEST_ASSERT_EQUAL_HEX16(0x04FB, sum_bytes(data, 5));
}
void test_sum_bytes_accepts_non_const_buffer(void) {
  // Compile-time property, not a runtime one: build_command passes its
  // non-const `out` here, and uint8_t* -> const uint8_t* must convert
  // implicitly. If sum_bytes' parameter ever loses its const this stops
  // building — which is the point of the test.
  uint8_t mutable_data[3] = {0x10, 0x20, 0x30};
  TEST_ASSERT_EQUAL_HEX16(0x60, sum_bytes(mutable_data, 3));
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
  TEST_ASSERT_TRUE(
      checksum_ok(buf, sizeof buf)); // fixture stays self-consistent
}

// --- checksum_ok: sum of bytes [0..n-3] must equal the stored word [n-2..n-1]
// --

void test_checksum_accepts_good_frame(void) {
  TEST_ASSERT_TRUE(checksum_ok(buf, sizeof buf));
}
void test_checksum_rejects_corrupted_data(void) {
  buf[17] ^= 0xFF; // change a data byte; stored sum stale
  TEST_ASSERT_FALSE(checksum_ok(buf, sizeof buf));
}
void test_checksum_rejects_corrupted_checksum(void) {
  buf[31] ^= 0x01; // change stored sum; data untouched
  TEST_ASSERT_FALSE(checksum_ok(buf, sizeof buf));
}
void test_parse_independent_of_checksum(void) {
  // Encodes the contract that parse extracts regardless and checksum_ok is
  // the gate. If your parse_message validates internally, drop or adjust this.
  buf[31] ^= 0xFF;
  pms_frame frame = parse_message(buf);
  TEST_ASSERT_EQUAL_HEX8(0x42, frame.start_char1);
  TEST_ASSERT_FALSE(checksum_ok(buf, sizeof buf));
}
void test_checksum_accepts_real_ack(void) {
  // Same verify path, 8-byte frame: exercises the n-relative indexing rather
  // than assuming 32.
  TEST_ASSERT_TRUE(checksum_ok(ack_passive, sizeof ack_passive));
  TEST_ASSERT_TRUE(checksum_ok(ack_active, sizeof ack_active));
}

// --- build_command: enum -> 7 wire bytes -----------------------------------
// The command set is closed: exactly 5 valid frames exist and their bytes never
// change, so every one is asserted against a hand-verified literal. Checksums
// computed as the sum of bytes [0..4], stored big-endian at [5..6]:
//   Read        42 4D E2 00 00 -> 369 == 0x0171
//   PassiveMode 42 4D E1 00 00 -> 368 == 0x0170
//   ActiveMode  42 4D E1 00 01 -> 369 == 0x0171
//   Sleep       42 4D E4 00 00 -> 371 == 0x0173
//   Wake        42 4D E4 00 01 -> 372 == 0x0174
// Note the host->device command frame has NO length field (unlike the device's
// responses) — it is always exactly 7 bytes.

static void assert_command(PmsCommand cmd, const uint8_t expected[7]) {
  uint8_t out[COMMAND_FRAME_SIZE];
  size_t n = build_command(cmd, out);
  TEST_ASSERT_EQUAL_UINT(COMMAND_FRAME_SIZE, n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, COMMAND_FRAME_SIZE);
  // cross-check: the frame we just built must satisfy our own verifier
  TEST_ASSERT_TRUE(checksum_ok(out, n));
}

void test_build_read(void) {
  const uint8_t expected[7] = {0x42, 0x4D, 0xE2, 0x00, 0x00, 0x01, 0x71};
  assert_command(PmsCommand::Read, expected);
}
void test_build_passive_mode(void) {
  const uint8_t expected[7] = {0x42, 0x4D, 0xE1, 0x00, 0x00, 0x01, 0x70};
  assert_command(PmsCommand::PassiveMode, expected);
}
void test_build_active_mode(void) {
  const uint8_t expected[7] = {0x42, 0x4D, 0xE1, 0x00, 0x01, 0x01, 0x71};
  assert_command(PmsCommand::ActiveMode, expected);
}
void test_build_sleep(void) {
  const uint8_t expected[7] = {0x42, 0x4D, 0xE4, 0x00, 0x00, 0x01, 0x73};
  assert_command(PmsCommand::Sleep, expected);
}
void test_build_wake(void) {
  const uint8_t expected[7] = {0x42, 0x4D, 0xE4, 0x00, 0x01, 0x01, 0x74};
  assert_command(PmsCommand::Wake, expected);
}

void test_build_commands_are_all_distinct(void) {
  // The direct regression test for the missing-break bug: with fallthrough,
  // every command produced Wake's bytes and they all compared equal here.
  // Also catches a copy-paste typo that duplicates a (CMD, DATAL) pair.
  const PmsCommand cmds[5] = {PmsCommand::Read, PmsCommand::PassiveMode,
                              PmsCommand::ActiveMode, PmsCommand::Sleep,
                              PmsCommand::Wake};
  uint8_t frames[5][COMMAND_FRAME_SIZE];
  for (size_t i = 0; i < 5; i++)
    TEST_ASSERT_EQUAL_UINT(COMMAND_FRAME_SIZE,
                           build_command(cmds[i], frames[i]));

  for (size_t i = 0; i < 5; i++)
    for (size_t j = i + 1; j < 5; j++)
      TEST_ASSERT_FALSE_MESSAGE(
          memcmp(frames[i], frames[j], COMMAND_FRAME_SIZE) == 0,
          "two commands produced identical bytes");
}

void test_build_writes_exactly_seven_bytes(void) {
  // build_command takes a raw pointer and cannot know the caller's buffer size,
  // so this pins down that it stays inside COMMAND_FRAME_SIZE. Guard bytes
  // after the frame must survive untouched.
  uint8_t padded[COMMAND_FRAME_SIZE + 4];
  memset(padded, 0xAA, sizeof padded);
  size_t n = build_command(PmsCommand::Read, padded);
  TEST_ASSERT_EQUAL_UINT(COMMAND_FRAME_SIZE, n);
  for (size_t i = COMMAND_FRAME_SIZE; i < sizeof padded; i++)
    TEST_ASSERT_EQUAL_HEX8(0xAA, padded[i]);
}

void test_build_rejects_invalid_command(void) {
  // Deliberately does the thing production code never should: casts an
  // arbitrary integer into the enum to reach the default branch. Fixing the
  // underlying type to uint8_t makes this well-defined (any 0..255 value), and
  // the guard must return 0 rather than emit a plausible-looking frame.
  uint8_t out[COMMAND_FRAME_SIZE];
  memset(out, 0xAA, sizeof out);
  TEST_ASSERT_EQUAL_UINT(0, build_command(static_cast<PmsCommand>(200), out));
}

// --- frame_collector: framing driven by the length field -------------------
// feed() returns the frame length once a whole frame is buffered, else 0.

void test_collector_full_frame_returns_length(void) {
  // Byte-by-byte, the collector reports 0 until the final byte, then the
  // length the frame's own length field implies (28 + 4 == 32).
  frame_collector fc;
  for (size_t i = 0; i < sizeof(good_frame) - 1; i++)
    TEST_ASSERT_EQUAL_UINT(0, fc.feed(good_frame[i]));
  TEST_ASSERT_EQUAL_UINT(FRAME_SIZE,
                         fc.feed(good_frame[sizeof(good_frame) - 1]));
  TEST_ASSERT_TRUE(checksum_ok(fc.buffer, FRAME_SIZE));
}

void test_collector_honors_short_length(void) {
  // Real captured ack, not a synthetic fixture: this is byte-for-byte what the
  // sensor sends in reply to 0xE1. Proves the collector stops at the length the
  // frame declares instead of always grabbing 32 — i.e. a short control frame
  // won't swallow the head of the next frame. Note 8 == MIN_FRAME_SIZE exactly,
  // so the plausibility check passes with zero margin.
  frame_collector fc;
  for (size_t i = 0; i < sizeof(ack_passive) - 1; i++)
    TEST_ASSERT_EQUAL_UINT(0, fc.feed(ack_passive[i]));
  TEST_ASSERT_EQUAL_UINT(sizeof(ack_passive),
                         fc.feed(ack_passive[sizeof(ack_passive) - 1]));
  TEST_ASSERT_TRUE(checksum_ok(fc.buffer, sizeof(ack_passive)));
  TEST_ASSERT_EQUAL_HEX8(0xE1, fc.buffer[4]); // command echoed
  TEST_ASSERT_EQUAL_HEX8(0x00, fc.buffer[5]); // DATAL echoed: passive
}

void test_collector_ack_then_data_frame(void) {
  // The active-mode interleaving case: an 8-byte ack arrives in the middle of
  // the ~1-2.3s data stream. The short frame must not consume any of the
  // following 32-byte frame, and the collector must report both correctly.
  frame_collector fc;
  size_t n = 0;
  for (size_t i = 0; i < sizeof(ack_active); i++)
    n = fc.feed(ack_active[i]);
  TEST_ASSERT_EQUAL_UINT(sizeof(ack_active), n);
  TEST_ASSERT_EQUAL_HEX8(0x01, fc.buffer[5]); // DATAL echoed: active

  for (size_t i = 0; i < sizeof(good_frame) - 1; i++)
    TEST_ASSERT_EQUAL_UINT(0, fc.feed(good_frame[i]));
  TEST_ASSERT_EQUAL_UINT(FRAME_SIZE,
                         fc.feed(good_frame[sizeof(good_frame) - 1]));
  TEST_ASSERT_TRUE(checksum_ok(fc.buffer, FRAME_SIZE));
}

void test_collector_resyncs_after_leading_garbage(void) {
  // Bytes arriving mid-frame (the state right after attaching to a running
  // sensor) must be discarded silently until a real 0x42 0x4D header appears.
  const uint8_t garbage[5] = {0x00, 0xFF, 0x4D, 0x12, 0x99};
  frame_collector fc;
  for (size_t i = 0; i < sizeof garbage; i++)
    TEST_ASSERT_EQUAL_UINT(0, fc.feed(garbage[i]));

  size_t n = 0;
  for (size_t i = 0; i < sizeof(good_frame); i++)
    n = fc.feed(good_frame[i]);
  TEST_ASSERT_EQUAL_UINT(FRAME_SIZE, n);
  TEST_ASSERT_TRUE(checksum_ok(fc.buffer, FRAME_SIZE));
}

void test_collector_handles_repeated_start_byte(void) {
  // 0x42 0x42 0x4D ... — the HUNTING_4D branch that treats a second 0x42 as a
  // fresh start rather than resetting to HUNTING_42 and losing the header.
  frame_collector fc;
  TEST_ASSERT_EQUAL_UINT(0, fc.feed(0x42));
  size_t n = 0;
  for (size_t i = 0; i < sizeof(good_frame); i++)
    n = fc.feed(good_frame[i]);
  TEST_ASSERT_EQUAL_UINT(FRAME_SIZE, n);
  TEST_ASSERT_TRUE(checksum_ok(fc.buffer, FRAME_SIZE));
}

void test_collector_rejects_oversized_length(void) {
  // A length field of 0xFFFF claims a 65539-byte frame. The collector must
  // reject it at the length check and never report a completed frame — this is
  // the guard that keeps a corrupt length from driving a buffer overrun.
  const uint8_t hdr[4] = {0x42, 0x4D, 0xFF, 0xFF};
  frame_collector fc;
  for (size_t i = 0; i < 4; i++)
    TEST_ASSERT_EQUAL_UINT(0, fc.feed(hdr[i]));
  // and it has resynced: a fresh good frame is still parsed correctly
  size_t n = 0;
  for (size_t i = 0; i < sizeof(good_frame); i++)
    n = fc.feed(good_frame[i]);
  TEST_ASSERT_EQUAL_UINT(FRAME_SIZE, n);
}

void test_collector_rejects_undersized_length(void) {
  // The other end of the plausibility window: a length field of 0x0000 claims a
  // 4-byte frame, below MIN_FRAME_SIZE. Must be dropped, then resync.
  const uint8_t hdr[4] = {0x42, 0x4D, 0x00, 0x00};
  frame_collector fc;
  for (size_t i = 0; i < 4; i++)
    TEST_ASSERT_EQUAL_UINT(0, fc.feed(hdr[i]));
  size_t n = 0;
  for (size_t i = 0; i < sizeof(good_frame); i++)
    n = fc.feed(good_frame[i]);
  TEST_ASSERT_EQUAL_UINT(FRAME_SIZE, n);
}

void test_collector_reusable_across_frames(void) {
  // Steady-state polling reuses one collector for the life of the program;
  // reset() must leave it in a state where the next frame parses cleanly.
  frame_collector fc;
  for (int rep = 0; rep < 3; rep++) {
    size_t n = 0;
    for (size_t i = 0; i < sizeof(good_frame); i++)
      n = fc.feed(good_frame[i]);
    TEST_ASSERT_EQUAL_UINT(FRAME_SIZE, n);
    TEST_ASSERT_TRUE(checksum_ok(fc.buffer, FRAME_SIZE));
  }
}

int main(int, char **) {
  UNITY_BEGIN();

  RUN_TEST(test_make_word_low_byte);
  RUN_TEST(test_make_word_high_byte_promotion);
  RUN_TEST(test_make_word_low_byte_not_sign_extended);
  RUN_TEST(test_make_word_full_range);
  RUN_TEST(test_make_word_ordering);

  RUN_TEST(test_sum_bytes_empty);
  RUN_TEST(test_sum_bytes_simple);
  RUN_TEST(test_sum_bytes_respects_n);
  RUN_TEST(test_sum_bytes_exceeds_one_byte);
  RUN_TEST(test_sum_bytes_accepts_non_const_buffer);

  RUN_TEST(test_parse_start_chars);
  RUN_TEST(test_parse_zero_field);
  RUN_TEST(test_parse_reads_big_endian);

  RUN_TEST(test_checksum_accepts_good_frame);
  RUN_TEST(test_checksum_rejects_corrupted_data);
  RUN_TEST(test_checksum_rejects_corrupted_checksum);
  RUN_TEST(test_parse_independent_of_checksum);
  RUN_TEST(test_checksum_accepts_real_ack);

  RUN_TEST(test_build_read);
  RUN_TEST(test_build_passive_mode);
  RUN_TEST(test_build_active_mode);
  RUN_TEST(test_build_sleep);
  RUN_TEST(test_build_wake);
  RUN_TEST(test_build_commands_are_all_distinct);
  RUN_TEST(test_build_writes_exactly_seven_bytes);
  RUN_TEST(test_build_rejects_invalid_command);

  RUN_TEST(test_collector_full_frame_returns_length);
  RUN_TEST(test_collector_honors_short_length);
  RUN_TEST(test_collector_ack_then_data_frame);
  RUN_TEST(test_collector_resyncs_after_leading_garbage);
  RUN_TEST(test_collector_handles_repeated_start_byte);
  RUN_TEST(test_collector_rejects_oversized_length);
  RUN_TEST(test_collector_rejects_undersized_length);
  RUN_TEST(test_collector_reusable_across_frames);

  return UNITY_END();
}
