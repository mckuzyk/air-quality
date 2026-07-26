#include "pms_parser.h"

uint16_t sum_bytes(const uint8_t *buffer, size_t n) {
  // Sum first n bytes of buffer
  uint16_t sum = 0;
  for (size_t i = 0; i < n; i++) {
    sum += buffer[i];
  };
  return sum;
}

bool checksum_ok(const uint8_t *buffer, size_t n) {
  uint16_t checksum = make_word(buffer[n - 2], buffer[n - 1]);
  uint16_t sum = sum_bytes(buffer, n - 2);
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

size_t build_command(PmsCommand cmd, uint8_t *out) {
  out[0] = 0x42;
  out[1] = 0x4D;
  switch (cmd) {
  case PmsCommand::Read:
    out[2] = 0xE2;
    out[3] = 0x00;
    out[4] = 0x00;
    break;
  case PmsCommand::PassiveMode:
    out[2] = 0xE1;
    out[3] = 0x00;
    out[4] = 0x00;
    break;
  case PmsCommand::ActiveMode:
    out[2] = 0xE1;
    out[3] = 0x00;
    out[4] = 0x01;
    break;
  case PmsCommand::Sleep:
    out[2] = 0xE4;
    out[3] = 0x00;
    out[4] = 0x00;
    break;
  case PmsCommand::Wake:
    out[2] = 0xE4;
    out[3] = 0x00;
    out[4] = 0x01;
    break;
  default:
    return 0;
  }

  uint16_t checksum = sum_bytes(out, 5);
  out[5] = checksum >> 8;
  out[6] = 0xFF & checksum;

  return COMMAND_FRAME_SIZE;
}
