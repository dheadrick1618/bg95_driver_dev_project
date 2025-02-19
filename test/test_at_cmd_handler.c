#include "at_cmd_handler.h"

#include <esp_err.h>
#include <string.h>
#include <unity.h>

/***
 * What do we want to test for the AT CMD handler?
 * - Test helper fxns extensively
 * - Test input validation - valid and invalid  inputs  provided
 * - Test UART read response timeout
 * -
 *
 */

// Test responses for various scenarios
static const char* TEST_OK_RESPONSE        = "\r\nOK\r\n";
static const char* TEST_ERROR_RESPONSE     = "\r\nERROR\r\n";
static const char* TEST_CME_ERROR_RESPONSE = "\r\n+CME ERROR: 123\r\n";
static const char* TEST_DATA_RESPONSE      = "\r\n+CSQ: 24,99\r\nOK\r\n";

// Test command definitions
static const at_cmd_t TEST_CMD_NO_PARSER = {
    .name        = "TEST",
    .description = "Test command",
    .type_info   = {[AT_CMD_TYPE_READ] = {.parser = NULL, .formatter = NULL}}};

static esp_err_t dummy_parser(const char* response, void* parsed_data)
{
  return ESP_OK;
}

static const at_cmd_t TEST_QMTOPEN_CMD_WITH_PARSER = {
    .name        = "QMTOPEN",
    .description = "Open MQTT Connection",
    .type_info   = {[AT_CMD_TYPE_READ] = {.parser = dummy_parser, .formatter = NULL}}};

// ----------- TEST the HAS COMMAND TERMINATED helper fxn -------------------
// --------------------------------------------------------------------------

// Basic Response Tests
static void test_has_cmd_terminated_fxn_basic_ok_response(void)
{
  const char* response = "\r\nOK\r\n";
  TEST_ASSERT_TRUE(has_command_terminated(response, &TEST_CMD_NO_PARSER, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_fxn_basic_error_response(void)
{
  const char* response = "\r\nERROR\r\n";
  TEST_ASSERT_TRUE(has_command_terminated(response, &TEST_CMD_NO_PARSER, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_fxn_basic_cme_error_response(void)
{
  const char* response = "\r\n+CME ERROR: 123\r\n";
  TEST_ASSERT_TRUE(has_command_terminated(response, &TEST_CMD_NO_PARSER, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_fxn_incomplete_ok_response(void)
{
  const char* response = "\r\nOK"; // Missing \r\n
  TEST_ASSERT_FALSE(has_command_terminated(response, &TEST_CMD_NO_PARSER, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_fxn_incomplete_error_response(void)
{
  const char* response = "\r\nERROR"; // Missing \r\n
  TEST_ASSERT_FALSE(has_command_terminated(response, &TEST_CMD_NO_PARSER, AT_CMD_TYPE_READ));
}

// Data Response Tests
static void test_has_cmd_terminated_fxn_data_before_ok_response(void)
{
  const char* response = "\r\n+QMTOPEN: 0,\"iot-as-mqtt.cn-shanghai.aliyuncs.com\",1883\r\nOK\r\n";
  TEST_ASSERT_TRUE(
      has_command_terminated(response, &TEST_QMTOPEN_CMD_WITH_PARSER, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_fxn_ok_before_data_response(void)
{
  const char* response = "\r\nOK\r\n+QMTOPEN: 0,0\r\n";
  TEST_ASSERT_TRUE(
      has_command_terminated(response, &TEST_QMTOPEN_CMD_WITH_PARSER, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_fxn_data_response_no_ok(void)
{
  const char* response = "\r\n+QMTOPEN: 0,0\r\n";
  TEST_ASSERT_FALSE(
      has_command_terminated(response, &TEST_QMTOPEN_CMD_WITH_PARSER, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_fxn_incomplete_data_response(void)
{
  const char* response = "\r\nOK\r\n+QMTOPEN: 0,0";
  TEST_ASSERT_FALSE(
      has_command_terminated(response, &TEST_QMTOPEN_CMD_WITH_PARSER, AT_CMD_TYPE_READ));
}

// Edge Cases and Invalid Input Tests
static void test_has_cmd_terminated_fxn_null_response(void)
{
  TEST_ASSERT_FALSE(has_command_terminated(NULL, &TEST_CMD_NO_PARSER, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_fxn_null_command(void)
{
  const char* response = "\r\nOK\r\n";
  TEST_ASSERT_FALSE(has_command_terminated(response, NULL, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_fxn_empty_response(void)
{
  const char* response = "";
  TEST_ASSERT_FALSE(has_command_terminated(response, &TEST_CMD_NO_PARSER, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_fxn_invalid_command_type(void)
{
  const char* response = "\r\nOK\r\n";
  TEST_ASSERT_FALSE(has_command_terminated(response, &TEST_CMD_NO_PARSER, AT_CMD_TYPE_MAX));
}

// Test Runner with updated function names
void run_has_command_terminated_fxn_tests(void)
{
  UNITY_BEGIN();

  printf("\n== HAS CMD TERMINATED helper fxn Tests ==\n");
  // Basic response tests
  RUN_TEST(test_has_cmd_terminated_fxn_basic_ok_response);
  RUN_TEST(test_has_cmd_terminated_fxn_basic_error_response);
  RUN_TEST(test_has_cmd_terminated_fxn_basic_cme_error_response);
  RUN_TEST(test_has_cmd_terminated_fxn_incomplete_ok_response);
  RUN_TEST(test_has_cmd_terminated_fxn_incomplete_error_response);

  // Data response tests
  RUN_TEST(test_has_cmd_terminated_fxn_data_before_ok_response);
  RUN_TEST(test_has_cmd_terminated_fxn_ok_before_data_response);
  RUN_TEST(test_has_cmd_terminated_fxn_data_response_no_ok);
  RUN_TEST(test_has_cmd_terminated_fxn_incomplete_data_response);

  // Edge cases and invalid input
  RUN_TEST(test_has_cmd_terminated_fxn_null_response);
  RUN_TEST(test_has_cmd_terminated_fxn_null_command);
  RUN_TEST(test_has_cmd_terminated_fxn_empty_response);
  RUN_TEST(test_has_cmd_terminated_fxn_invalid_command_type);

  UNITY_END();
}

// ----------- TEST VALIDATE BASIC RESPONSE FUNCTION -------------------

static void test_validate_basic_response_ok(void)
{
  at_parsed_response_t parsed;
  esp_err_t            err = validate_basic_response(TEST_OK_RESPONSE, &parsed);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(parsed.has_basic_response);
  TEST_ASSERT_TRUE(parsed.basic_response_is_ok);
}

static void test_validate_basic_response_error(void)
{
  at_parsed_response_t parsed;
  esp_err_t            err = validate_basic_response(TEST_ERROR_RESPONSE, &parsed);
  TEST_ASSERT_EQUAL(ESP_FAIL, err);
  TEST_ASSERT_TRUE(parsed.has_basic_response);
  TEST_ASSERT_FALSE(parsed.basic_response_is_ok);
}

static void test_validate_basic_response_cme_error(void)
{
  at_parsed_response_t parsed;
  esp_err_t            err = validate_basic_response(TEST_CME_ERROR_RESPONSE, &parsed);
  TEST_ASSERT_EQUAL(ESP_FAIL, err);
  TEST_ASSERT_TRUE(parsed.has_basic_response);
  TEST_ASSERT_FALSE(parsed.basic_response_is_ok);
}

static void test_validate_basic_response_null_response(void)
{
  at_parsed_response_t parsed;
  esp_err_t            err = validate_basic_response(NULL, &parsed);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_validate_basic_response_null_parsed(void)
{
  esp_err_t err = validate_basic_response(TEST_OK_RESPONSE, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ----------- TEST PARSE AT CMD SPECIFIC DATA RESPONSE FUNCTION -------------------

// Mock parser function for testing
static esp_err_t mock_parser(const char* response, void* parsed_data)
{
  if (!response || !parsed_data)
  {
    return ESP_ERR_INVALID_ARG;
  }
  // Simple parser that just copies first char of response to parsed_data
  *(char*) parsed_data = response[0];
  return ESP_OK;
}

static const at_cmd_t TEST_CMD_WITH_PARSER = {
    .name        = "TEST",
    .description = "Test command",
    .type_info   = {[AT_CMD_TYPE_READ] = {.parser = mock_parser, .formatter = NULL}}};

static void test_parse_specific_data_with_parser(void)
{
  at_parsed_response_t parsed_base = {
      .has_data_response = true, .data_response = "+TEST: 1", .data_response_len = 8};
  char parsed_data = 0;

  esp_err_t err = parse_at_cmd_specific_data_response(
      &TEST_CMD_WITH_PARSER, AT_CMD_TYPE_READ, "+TEST: 1", &parsed_base, &parsed_data);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL('+', parsed_data);
}

static void test_parse_specific_data_no_parser(void)
{
  at_parsed_response_t parsed_base = {
      .has_data_response = true, .data_response = "+TEST: 1", .data_response_len = 8};
  char parsed_data = 0;

  const at_cmd_t cmd_no_parser = {.name      = "TEST",
                                  .type_info = {[AT_CMD_TYPE_READ] = {.parser = NULL}}};

  esp_err_t err = parse_at_cmd_specific_data_response(
      &cmd_no_parser, AT_CMD_TYPE_READ, "+TEST: 1", &parsed_base, &parsed_data);

  TEST_ASSERT_EQUAL(ESP_OK, err);
}

static void test_parse_specific_data_no_data_response(void)
{
  at_parsed_response_t parsed_base = {.has_data_response = false};
  char                 parsed_data = 0;

  esp_err_t err = parse_at_cmd_specific_data_response(
      &TEST_CMD_WITH_PARSER, AT_CMD_TYPE_READ, "OK", &parsed_base, &parsed_data);

  TEST_ASSERT_EQUAL(ESP_OK, err);
}

// ----------- TEST READ AT CMD RESPONSE FUNCTION -------------------
// TODO: Test read at cmd reponse fxn

// // Mock UART functions for testing
// static esp_err_t
// mock_uart_read(char* buffer, size_t max_len, size_t* bytes_read, uint32_t timeout_ms, void*
// context)
// {
//   static const char* test_response = "\r\nOK\r\n";
//   static size_t      pos           = 0;
//
//   if (pos >= strlen(test_response))
//   {
//     *bytes_read = 0;
//     return ESP_OK;
//   }
//
//   size_t remaining = strlen(test_response) - pos;
//   size_t to_copy   = remaining < max_len ? remaining : max_len;
//
//   memcpy(buffer, test_response + pos, to_copy);
//   *bytes_read = to_copy;
//   pos += to_copy;
//
//   return ESP_OK;
// }

// static void test_read_at_cmd_response_success(void)
// {
//   at_cmd_handler_t handler = {.uart = {.read = mock_uart_read, .context = NULL}};
//
//   char      response_buffer[32];
//   esp_err_t err = read_at_cmd_response(
//       &handler, &TEST_CMD_WITH_PARSER, AT_CMD_TYPE_READ, response_buffer,
//       sizeof(response_buffer));
//
//   TEST_ASSERT_EQUAL(ESP_OK, err);
//   TEST_ASSERT_EQUAL_STRING("\r\nOK\r\n", response_buffer);
// }
//
// static void test_read_at_cmd_response_buffer_too_small(void)
// {
//   at_cmd_handler_t handler = {.uart = {.read = mock_uart_read, .context = NULL}};
//
//   char      response_buffer[4]; // Too small for complete response
//   esp_err_t err = read_at_cmd_response(
//       &handler, &TEST_CMD_WITH_PARSER, AT_CMD_TYPE_READ, response_buffer,
//       sizeof(response_buffer));
//
//   TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
// }

// ----------------------------------------------------------------
// ----------------------------------------------------------------

void run_validate_basic_response_tests(void)
{
  UNITY_BEGIN();

  printf("\n== VALIDATE BASIC RESPONSE Tests ==\n");
  RUN_TEST(test_validate_basic_response_ok);
  RUN_TEST(test_validate_basic_response_error);
  RUN_TEST(test_validate_basic_response_cme_error);
  RUN_TEST(test_validate_basic_response_null_response);
  RUN_TEST(test_validate_basic_response_null_parsed);

  UNITY_END();
}

void run_parse_specific_data_response_tests(void)
{
  UNITY_BEGIN();

  printf("\n== PARSE SPECIFIC DATA RESPONSE Tests ==\n");
  RUN_TEST(test_parse_specific_data_with_parser);
  RUN_TEST(test_parse_specific_data_no_parser);
  RUN_TEST(test_parse_specific_data_no_data_response);

  UNITY_END();
}

// void run_read_at_cmd_response_tests(void)
// {
//   UNITY_BEGIN();
//
//   // printf("\n== READ AT CMD RESPONSE Tests ==\n");
//   // RUN_TEST(test_read_at_cmd_response_success);
//   // RUN_TEST(test_read_at_cmd_response_buffer_too_small);
//
//   UNITY_END();
// }

void run_test_at_cmd_handler_all(void)
{
  run_has_command_terminated_fxn_tests();
  run_validate_basic_response_tests();
  run_parse_specific_data_response_tests();
  // run_read_at_cmd_response_tests();
}
