#include "pms_parser.h"

uint16_t make_word(uint8_t hi, uint8_t lo) { return (hi << 8) | lo; }

bool checksum_ok(const uint8_t *buffer) {
  uint16_t checksum = make_word(buffer[BUF_CS_HI], buffer[BUF_CS_LO]);
  uint16_t sum = 0;
  for (size_t i = 0; i < FRAME_SIZE - 2; i++) {
    sum += buffer[i];
  };
  return sum == checksum;
}

bool start_bytes_ok(const uint8_t *buffer) {
  return (buffer[BUF_STRT1] == 0x42) && (buffer[BUF_STRT2] == 0x4D);
}

pms_frame parse_message(const uint8_t *message) {
  pms_frame frame;
  frame.start_char1 = message[BUF_STRT1];
  frame.start_char2 = message[BUF_STRT2];
  frame.frame_length = make_word(message[BUF_FRM_HI], message[BUF_FRM_LO]);
  frame.pm_sp_1_0 = make_word(message[BUF_D1_HI], message[BUF_D1_LO]);
  frame.pm_sp_2_5 = make_word(message[BUF_D2_HI], message[BUF_D2_LO]);
  frame.pm_sp_10_0 = make_word(message[BUF_D3_HI], message[BUF_D3_LO]);
  frame.pm_se_1_0 = make_word(message[BUF_D4_HI], message[BUF_D4_LO]);
  frame.pm_se_2_5 = make_word(message[BUF_D5_HI], message[BUF_D5_LO]);
  frame.pm_se_10_0 = make_word(message[BUF_D6_HI], message[BUF_D6_LO]);
  frame.p_cnt_0_3 = make_word(message[BUF_D7_HI], message[BUF_D7_LO]);
  frame.p_cnt_0_5 = make_word(message[BUF_D8_HI], message[BUF_D8_LO]);
  frame.p_cnt_1_0 = make_word(message[BUF_D9_HI], message[BUF_D9_LO]);
  frame.p_cnt_2_5 = make_word(message[BUF_D10_HI], message[BUF_D10_LO]);
  frame.p_cnt_5_0 = make_word(message[BUF_D11_HI], message[BUF_D11_LO]);
  frame.p_cnt_10_0 = make_word(message[BUF_D12_HI], message[BUF_D12_LO]);
  frame.reserved = make_word(message[BUF_D13_HI], message[BUF_D13_LO]);
  frame.checksum = make_word(message[BUF_CS_HI], message[BUF_CS_LO]);

  return frame;
}
