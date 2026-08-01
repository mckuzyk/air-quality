#include <cstdint>

#pragma once

class IntervalTimer {
public:
  explicit IntervalTimer(uint32_t period_ms) : period_(period_ms) {}

  void reset(uint32_t now) {
    last_ = now;
    overruns_ = 0;
  }

  bool due(uint32_t now) {
    const uint32_t elapsed = now - last_;
    if (elapsed < period_) {
      return false;
    }
    const uint32_t periods = elapsed / period_;
    last_ += periods * period_;
    if (periods > 1) {
      overruns_ += periods - 1;
    }
    return true;
  }

  uint32_t overruns() const { return overruns_; }

private:
  uint32_t period_;
  uint32_t last_ = 0;
  uint32_t overruns_ = 0;
};
