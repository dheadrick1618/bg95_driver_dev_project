#include "bg95_uart_interface.h"

#include <string.h>
#include <unity.h>

// Test responses
static const mock_uart_response_t test_responses[] = {
    {.expected_cmd = "AT+CSQ", .cmd_response = "\r\n+CSQ: 24,0\r\nOK\r\n", .delay_ms = 0},
    {.expected_cmd = "AT+CREG?", .cmd_response = "\r\n+CREG: 0,1\r\nOK\r\n", .delay_ms = 10},
    {.expected_cmd = "AT+CPIN?", .cmd_response = "\r\n+CPIN: READY\r\nOK\r\n", .delay_ms = 0}};

static void test_mock_uart_init_null_interface(void)
{
  TEST_ASSERT_EQUAL(
      ESP_ERR_INVALID_ARG,
      mock_uart_init(NULL, test_responses, sizeof(test_responses) / sizeof(test_responses[0])));
}

static void test_mock_uart_init_null_responses(void)
{
  bg95_uart_interface_t uart = {0};
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, mock_uart_init(&uart, NULL, 1));
}

static void test_mock_uart_init_zero_responses(void)
{
  bg95_uart_interface_t uart = {0};
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, mock_uart_init(&uart, test_responses, 0));
}

static void test_mock_uart_init_success(void)
{
  bg95_uart_interface_t uart = {0};
  TEST_ASSERT_EQUAL(
      ESP_OK,
      mock_uart_init(&uart, test_responses, sizeof(test_responses) / sizeof(test_responses[0])));

  // Verify interface is properly initialized
  TEST_ASSERT_NOT_NULL(uart.write);
  TEST_ASSERT_NOT_NULL(uart.read);
  TEST_ASSERT_NOT_NULL(uart.context);

  mock_uart_deinit(&uart);
}

static void test_mock_uart_write_command(void)
{
  bg95_uart_interface_t uart = {0};
  TEST_ASSERT_EQUAL(
      ESP_OK,
      mock_uart_init(&uart, test_responses, sizeof(test_responses) / sizeof(test_responses[0])));

  const char* test_cmd = "AT+CSQ\r\n";
  TEST_ASSERT_EQUAL(ESP_OK, uart.write(test_cmd, strlen(test_cmd), uart.context));

  mock_uart_deinit(&uart);
}

static void test_mock_uart_read_matching_response(void)
{
  bg95_uart_interface_t uart = {0};
  TEST_ASSERT_EQUAL(
      ESP_OK,
      mock_uart_init(&uart, test_responses, sizeof(test_responses) / sizeof(test_responses[0])));

  // Write command
  const char* test_cmd = "AT+CSQ\r\n";
  TEST_ASSERT_EQUAL(ESP_OK, uart.write(test_cmd, strlen(test_cmd), uart.context));

  // Read response
  char   buffer[128] = {0};
  size_t bytes_read  = 0;
  TEST_ASSERT_EQUAL(ESP_OK, uart.read(buffer, sizeof(buffer), &bytes_read, 100, uart.context));

  // Verify response
  TEST_ASSERT_EQUAL(strlen(test_responses[0].cmd_response), bytes_read);
  TEST_ASSERT_EQUAL_STRING(test_responses[0].cmd_response, buffer);

  mock_uart_deinit(&uart);
}

static void test_mock_uart_read_no_matching_command(void)
{
  bg95_uart_interface_t uart = {0};
  TEST_ASSERT_EQUAL(
      ESP_OK,
      mock_uart_init(&uart, test_responses, sizeof(test_responses) / sizeof(test_responses[0])));

  // Write unknown command
  const char* test_cmd = "AT+UNKNOWN\r\n";
  TEST_ASSERT_EQUAL(ESP_OK, uart.write(test_cmd, strlen(test_cmd), uart.context));

  // Try to read response
  char   buffer[128] = {0};
  size_t bytes_read  = 0;
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                    uart.read(buffer, sizeof(buffer), &bytes_read, 100, uart.context));

  mock_uart_deinit(&uart);
}

// Test when response is larger than provided buffer size
#define tiny_buffer_size 5
static void test_mock_uart_read_buffer_overflow(void)
{
  bg95_uart_interface_t uart = {0};
  TEST_ASSERT_EQUAL(
      ESP_OK,
      mock_uart_init(&uart, test_responses, sizeof(test_responses) / sizeof(test_responses[0])));

  // Write command
  const char* test_cmd = "AT+CSQ\r\n";
  TEST_ASSERT_EQUAL(ESP_OK, uart.write(test_cmd, strlen(test_cmd), uart.context));

  // Try to read with tiny buffer
  char   buffer[tiny_buffer_size] = {0};
  size_t bytes_read               = 0;
  TEST_ASSERT_EQUAL(ESP_OK, uart.read(buffer, sizeof(buffer), &bytes_read, 100, uart.context));

  // Should truncate to buffer size
  TEST_ASSERT_EQUAL(tiny_buffer_size, bytes_read);
  TEST_ASSERT_EQUAL_STRING_LEN(test_responses[0].cmd_response, buffer, tiny_buffer_size);

  mock_uart_deinit(&uart);
}

void run_test_mock_uart_interface_all(void)
{
  UNITY_BEGIN();

  // Initialization tests
  RUN_TEST(test_mock_uart_init_null_interface);
  RUN_TEST(test_mock_uart_init_null_responses);
  RUN_TEST(test_mock_uart_init_zero_responses);
  RUN_TEST(test_mock_uart_init_success);

  // Basic operation tests
  RUN_TEST(test_mock_uart_write_command);
  RUN_TEST(test_mock_uart_read_matching_response);
  RUN_TEST(test_mock_uart_read_no_matching_command);

  // Advanced tests
  RUN_TEST(test_mock_uart_read_buffer_overflow);

  UNITY_END();
}
