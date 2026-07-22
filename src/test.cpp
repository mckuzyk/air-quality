#include "pms_parser.h"
#include <cassert>

int main() {
  assert(make_word(0x00, 0x1C) == 28);
  assert(make_word(0x01, 0x00) == 256); // catches the promotion bug

  uint8_t br[32] = {
      0X42, 0X4D, 0X00, 0X1C, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
      0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X24, 0X00, 0X07, 0X00, 0X01,
      0X00, 0X01, 0X00, 0X01, 0X00, 0X00, 0X97, 0X00, 0X01, 0X70,
  };

  pms_frame frame = parse_message(br);
  assert(frame.start_char1 == 0X42);
  assert(frame.start_char2 == 0X4D);
  assert(frame.pm_sp_1_0 == 0);

  assert(checksum_ok(br));

  return 0;
}
