#pragma once

#include <cstdint>

#ifndef SAMPLE_PERIOD_MS
#define SAMPLE_PERIOD_MS 3000
#endif

constexpr uint32_t SAMPLE_PERIOD = SAMPLE_PERIOD_MS;
static_assert(SAMPLE_PERIOD > 0, "sample period must be nonzero");
