// This is for setting up testing framework and ensuring it is working

#include <unity.h>

void dummy_test(void)
{
  TEST_ASSERT_EQUAL(1, 1);
}

// Main test runner
void run_test_dummy(void)
{
  RUN_TEST(dummy_test);
}
