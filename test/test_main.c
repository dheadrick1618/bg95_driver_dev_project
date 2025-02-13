// This is where all other defined tests are called an run from

#include <stdio.h> //For printf
#include <unity.h>

void run_test_dummy(void);

void run_state_machine_tests_all(void);

void app_main(void)
{
  // UNITY_BEGIN();
  printf("\n======== RUNNING ALL TESTS ========\n");

  printf("\n=== Dummy Tests ===\n");
  run_test_dummy();

  // TODO: implement other tests
  // printf("\n=== Other Tests ===\n");

  // UNITY_END();
}
