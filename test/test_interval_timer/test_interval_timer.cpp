#include "interval_timer.h"
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

// --- basic firing ---

void test_not_due_before_period(void) {
  IntervalTimer t(3000);
  t.reset(1000);
  TEST_ASSERT_FALSE(t.due(3999));
}

void test_due_exactly_on_boundary(void) {
  IntervalTimer t(3000);
  t.reset(1000);
  TEST_ASSERT_TRUE(t.due(4000));
}

// Calling due() twice at the same instant must not fire twice -- loop() calls
// it every iteration, so this is the common case, not an edge case.
void test_due_is_edge_triggered_not_level(void) {
  IntervalTimer t(3000);
  t.reset(1000);
  TEST_ASSERT_TRUE(t.due(4000));
  TEST_ASSERT_FALSE(t.due(4000));
}

// --- phase locking (the reason this class exists) ---

// Noticed 123ms late; the NEXT deadline is still on the original grid at 7000,
// not 7123. This is what distinguishes `last_ += periods * period_` from
// `last_ = now` -- the latter would let per-cycle latency accumulate.
void test_late_detection_does_not_shift_grid(void) {
  IntervalTimer t(3000);
  t.reset(1000);
  TEST_ASSERT_TRUE(t.due(4123));
  TEST_ASSERT_FALSE(t.due(6999));
  TEST_ASSERT_TRUE(t.due(7000));
}

void test_grid_holds_over_many_late_cycles(void) {
  IntervalTimer t(3000);
  t.reset(0);
  for (uint32_t n = 1; n <= 100; n++) {
    TEST_ASSERT_FALSE(t.due(n * 3000 - 1));
    TEST_ASSERT_TRUE(t.due(n * 3000 + 50)); // always 50ms late
  }
  TEST_ASSERT_EQUAL_UINT32(0, t.overruns()); // late != overrun
}

// --- overruns ---

// A stall spanning ten periods yields ONE fire, not a burst of ten.
void test_overrun_fires_once(void) {
  IntervalTimer t(3000);
  t.reset(0);
  TEST_ASSERT_TRUE(t.due(30000));
  TEST_ASSERT_FALSE(t.due(30001));
}

void test_overrun_counts_skipped_periods(void) {
  IntervalTimer t(3000);
  t.reset(0);
  t.due(30000); // 10 periods elapsed => 1 fired, 9 skipped
  TEST_ASSERT_EQUAL_UINT32(9, t.overruns());
}

void test_overruns_accumulate(void) {
  IntervalTimer t(3000);
  t.reset(0);
  t.due(9000);  // 3 periods => 2 skipped
  t.due(15000); // 2 periods => 1 skipped
  TEST_ASSERT_EQUAL_UINT32(3, t.overruns());
}

void test_grid_intact_after_overrun(void) {
  IntervalTimer t(3000);
  t.reset(0);
  TEST_ASSERT_TRUE(t.due(30000));
  TEST_ASSERT_FALSE(t.due(32999));
  TEST_ASSERT_TRUE(t.due(33000));
}

// --- millis() rollover ---

// millis() wraps every ~49.7 days. Only unsigned differences are ever
// compared, so no special-casing is needed -- this test is what proves it.
// Untestable on hardware without waiting 49.7 days; trivial here.
void test_survives_rollover(void) {
  const uint32_t before_wrap = 0xFFFFFF00u; // 256ms before wrap
  IntervalTimer t(3000);
  t.reset(before_wrap);
  TEST_ASSERT_FALSE(t.due(0x900u)); // elapsed 2560
  TEST_ASSERT_TRUE(t.due(0xB00u));  // elapsed 3072
  TEST_ASSERT_FALSE(t.due(0xB00u));
}

void test_no_overrun_reported_across_rollover(void) {
  IntervalTimer t(3000);
  t.reset(0xFFFFFF00u);
  t.due(0xB00u);
  TEST_ASSERT_EQUAL_UINT32(0, t.overruns());
}

// --- reset ---

void test_reset_rephases_grid(void) {
  IntervalTimer t(3000);
  t.reset(1000);
  TEST_ASSERT_TRUE(t.due(4000));
  t.reset(5000);
  TEST_ASSERT_FALSE(t.due(7999));
  TEST_ASSERT_TRUE(t.due(8000));
}

// Arming one period in the past is the idiom for "fire on the first check" --
// needed for long-period timers (a 6h publish interval shouldn't wait 6h for
// its first datapoint).
void test_reset_in_past_fires_immediately(void) {
  IntervalTimer t(3000);
  t.reset(1000 - 3000); // wraps; still correct
  TEST_ASSERT_TRUE(t.due(1000));
}

int main(int, char **) {
  UNITY_BEGIN();

  RUN_TEST(test_not_due_before_period);
  RUN_TEST(test_due_exactly_on_boundary);
  RUN_TEST(test_due_is_edge_triggered_not_level);

  RUN_TEST(test_late_detection_does_not_shift_grid);
  RUN_TEST(test_grid_holds_over_many_late_cycles);

  RUN_TEST(test_overrun_fires_once);
  RUN_TEST(test_overrun_counts_skipped_periods);
  RUN_TEST(test_overruns_accumulate);
  RUN_TEST(test_grid_intact_after_overrun);

  RUN_TEST(test_survives_rollover);
  RUN_TEST(test_no_overrun_reported_across_rollover);

  RUN_TEST(test_reset_rephases_grid);
  RUN_TEST(test_reset_in_past_fires_immediately);

  return UNITY_END();
}
