// This is where all other defined tests are called an run from

#include <stdio.h> //For printf
#include <unity.h>

void run_test_dummy(void);

void run_test_at_cmd_parser_all(void);

void run_test_mock_uart_interface_all(void);

void run_test_at_cmd_handler_all(void);

void app_main(void)
{
  // UNITY_BEGIN(); -- not needed here because each 'run_test_....' fxn handles UNITY BEGIN and END
  printf("\n======== RUNNING ALL TESTS ========\n");

  printf("\n===== Dummy Tests =====\n");
  run_test_dummy();

  printf("\n===== AT CMD PARSER Tests =====\n");
  run_test_at_cmd_parser_all();

  printf("\n===== MOCK UART Interface Tests =====\n");
  run_test_mock_uart_interface_all();

  printf("\n===== AT CMD HANDLER Tests =====\n");
  run_test_at_cmd_handler_all();

  // UNITY_END();
}
