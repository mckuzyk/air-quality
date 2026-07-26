#include <cstddef>
#include <cstdint>

#pragma once

constexpr size_t FRAME_SIZE = 32;
constexpr size_t MIN_FRAME_SIZE = 8;
constexpr size_t COMMAND_FRAME_SIZE = 7;
inline uint16_t make_word(uint8_t hi, uint8_t lo) { return (hi << 8) | lo; }

enum BufferRaw {
  BUF_STRT1 = 0,
  BUF_STRT2,
  BUF_FRM_HI,
  BUF_FRM_LO,
  BUF_D1_HI,
  BUF_D1_LO,
  BUF_D2_HI,
  BUF_D2_LO,
  BUF_D3_HI,
  BUF_D3_LO,
  BUF_D4_HI,
  BUF_D4_LO,
  BUF_D5_HI,
  BUF_D5_LO,
  BUF_D6_HI,
  BUF_D6_LO,
  BUF_D7_HI,
  BUF_D7_LO,
  BUF_D8_HI,
  BUF_D8_LO,
  BUF_D9_HI,
  BUF_D9_LO,
  BUF_D10_HI,
  BUF_D10_LO,
  BUF_D11_HI,
  BUF_D11_LO,
  BUF_D12_HI,
  BUF_D12_LO,
  BUF_D13_HI,
  BUF_D13_LO,
  BUF_CS_HI,
  BUF_CS_LO
};

enum class PmsCommand : uint8_t { Read, PassiveMode, ActiveMode, Sleep, Wake };

struct pms_frame {
  uint8_t start_char1;
  uint8_t start_char2;
  uint16_t frame_length;
  uint16_t pm_sp_1_0;
  uint16_t pm_sp_2_5;
  uint16_t pm_sp_10_0;
  uint16_t pm_se_1_0;
  uint16_t pm_se_2_5;
  uint16_t pm_se_10_0;
  uint16_t p_cnt_0_3;
  uint16_t p_cnt_0_5;
  uint16_t p_cnt_1_0;
  uint16_t p_cnt_2_5;
  uint16_t p_cnt_5_0;
  uint16_t p_cnt_10_0;
  uint16_t reserved;
  uint16_t checksum;
};

enum CollectionState { HUNTING_42, HUNTING_4D, COLLECTING };
enum class ReadState { IDLE, AWAITING_RESPONSE };

struct frame_collector {
  CollectionState state = HUNTING_42;
  size_t idx = 0;
  size_t expected = FRAME_SIZE;
  uint8_t buffer[FRAME_SIZE];

  size_t feed(uint8_t b) {
    switch (state) {
    case HUNTING_42:
      if (b == 0x42) {
        idx = 0;
        state = HUNTING_4D;
        buffer[idx] = b;
        idx++;
      }
      break;
    case HUNTING_4D:
      if (b == 0x4D) {
        state = COLLECTING;
        buffer[idx] = b;
        idx++;
      } else if (b == 0x42) {
        state = HUNTING_4D;
        buffer[0] = b;
        idx = 1;
      } else {
        reset();
      }
      break;
    case COLLECTING:
      buffer[idx++] = b;

      if (idx == 4) {
        // Frame size is remaining bytes only. Add 4 to include the header
        size_t claimed = make_word(buffer[BUF_FRM_HI], buffer[BUF_FRM_LO]) + 4;
        if (claimed < MIN_FRAME_SIZE || claimed > FRAME_SIZE) {
          reset(); // implausible length -> drop and resync
          break;
        }
        expected = claimed;
      }

      if (idx >= expected) {
        size_t n = expected;
        reset();
        return n;
      }
      break;
    default:
      reset();
      break;
    }
    return 0;
  }

  void reset() {
    idx = 0;
    state = HUNTING_42;
  }
};

bool checksum_ok(const uint8_t *buffer, size_t n);
bool start_bytes_ok(const uint8_t *buffer);
pms_frame parse_message(const uint8_t *message);
size_t build_command(PmsCommand cmd, uint8_t *out);
uint16_t sum_bytes(const uint8_t *buffer, size_t n);
