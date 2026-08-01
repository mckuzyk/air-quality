#include "HardwareSerial.h"
#include "config.h"
#include "esp32-hal.h"
#include "interval_timer.h"
#include "pms_parser.h"
#include <Arduino.h>
#include <cstddef>
#include <cstdint>

static frame_collector frame_buf;
static IntervalTimer interval_timer = IntervalTimer(SAMPLE_PERIOD);

void print_frame(const pms_frame &f);

void setup() {
  Serial.begin(115200);
  delay(500);
  // (baud, config, RX)
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("==== boot ====");

  uint8_t cmd_buffer[COMMAND_FRAME_SIZE];
  size_t n = build_command(PmsCommand::PassiveMode, cmd_buffer);
  delay(2000);
  Serial2.write(cmd_buffer, n);
  delay(2000);
  // Drain buffer
  while (Serial2.available()) {
    Serial2.read();
  }

  interval_timer.reset(millis() - SAMPLE_PERIOD); // armed to fire at loop start
}

void loop() {

  if (interval_timer.due(millis())) {
    uint8_t cmd_buffer[COMMAND_FRAME_SIZE];
    size_t n_cmd = build_command(PmsCommand::Read, cmd_buffer);
    Serial2.write(cmd_buffer, n_cmd);
  }

  while (Serial2.available()) {
    uint8_t b = Serial2.read();
    size_t n = frame_buf.feed(b);
    if (n == 0)
      continue;

    if (!checksum_ok(frame_buf.buffer, n)) {
      Serial.println("Checksum failed, dropping buffer");
      continue;
    }

    if (n == FRAME_SIZE) {
      pms_frame frame = parse_message(frame_buf.buffer);
      print_frame(frame);
    } else {
      Serial.printf("Received %u-byte control frame\n", (unsigned)n);
    }
  }
}

void print_frame(const pms_frame &f) {
  Serial.println("---- PMS frame ----");
  Serial.println("Standard (CF=1), ug/m3:");
  Serial.printf("  PM1.0: %-5u  PM2.5: %-5u  PM10: %-5u\n", f.pm_sp_1_0,
                f.pm_sp_2_5, f.pm_sp_10_0);
  Serial.println("Atmospheric (environmental), ug/m3:");
  Serial.printf("  PM1.0: %-5u  PM2.5: %-5u  PM10: %-5u\n", f.pm_se_1_0,
                f.pm_se_2_5, f.pm_se_10_0);
  Serial.println("Particle counts per 0.1L air, by size:");
  Serial.printf("  >0.3um: %-5u  >0.5um: %-5u  >1.0um: %-5u\n", f.p_cnt_0_3,
                f.p_cnt_0_5, f.p_cnt_1_0);
  Serial.printf("  >2.5um: %-5u  >5.0um: %-5u  >10um:  %-5u\n", f.p_cnt_2_5,
                f.p_cnt_5_0, f.p_cnt_10_0);
  Serial.println();
}
