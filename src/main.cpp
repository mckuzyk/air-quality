#include "HardwareSerial.h"
#include "pms_parser.h"
#include <Arduino.h>
#include <cstdint>

void print_frame(const pms_frame &f);

void setup() {
  Serial.begin(115200);
  // (baud, config, RX)
  Serial2.begin(9600, SERIAL_8N1, 16);
}

void loop() {
  static frame_collector frame_buf;
  while (Serial2.available()) {
    if (frame_buf.feed(Serial2.read())) {
      if (checksum_ok(frame_buf.buffer)) {
        pms_frame frame = parse_message(frame_buf.buffer);
        print_frame(frame);
      } else {
        Serial.println("frame dropped: checksum mismatch");
      }
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
