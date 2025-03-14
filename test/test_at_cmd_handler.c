#include "at_cmd_handler.h"

#include <esp_err.h>
#include <string.h>
#include <unity.h>

/***
 * What do we want to test for the AT CMD handler?
 * - Test helper fxns extensively
 * - Test input validation - valid and invalid inputs provided
 * - Test UART read response timeout
 *
 */

// Test responses for various scenarios
static const char* TEST_OK_RESPONSE        = "\r\nOK\r\n";
static const char* TEST_ERROR_RESPONSE     = "\r\nERROR\r\n";
static const char* TEST_CME_ERROR_RESPONSE = "\r\n+CME ERROR: 123\r\n";
static const char* TEST_DATA_RESPONSE      = "\r\n+CSQ: 24,99\r\nOK\r\n";

static esp_err_t dummy_parser(const char* response, void* parsed_data)
{
  return ESP_OK;
}

// Test command definitions with response types
static const at_cmd_t TEST_CMD_SIMPLE_ONLY = {
    .name        = "TEST",
    .description = "Test command",
    .type_info   = {[AT_CMD_TYPE_READ] = {.parser        = NULL,
                                          .formatter     = NULL,
                                          .response_type = AT_CMD_RESPONSE_TYPE_SIMPLE_ONLY}}};

static const at_cmd_t TEST_CMD_DATA_REQUIRED = {
    .name        = "TEST",
    .description = "Test command with required data",
    .type_info   = {[AT_CMD_TYPE_READ] = {.parser        = dummy_parser,
                                          .formatter     = NULL,
                                          .response_type = AT_CMD_RESPONSE_TYPE_DATA_REQUIRED}}};

static const at_cmd_t TEST_CMD_DATA_OPTIONAL = {
    .name        = "TEST",
    .description = "Test command with optional data",
    .type_info   = {[AT_CMD_TYPE_READ] = {.parser        = dummy_parser,
                                          .formatter     = NULL,
                                          .response_type = AT_CMD_RESPONSE_TYPE_DATA_OPTIONAL}}};

static const at_cmd_t TEST_QMTOPEN_CMD = {
    .name        = "QMTOPEN",
    .description = "Open MQTT Connection",
    .type_info   = {[AT_CMD_TYPE_READ] = {.parser        = dummy_parser,
                                          .formatter     = NULL,
                                          .response_type = AT_CMD_RESPONSE_TYPE_DATA_OPTIONAL}}};

// ----------- TEST the HAS COMMAND TERMINATED helper fxn -------------------

// Basic Response Tests - with SIMPLE_ONLY
static void test_has_cmd_terminated_simple_only_ok(void)
{
  const char* response = "\r\nOK\r\n";
  TEST_ASSERT_TRUE(has_command_terminated(response, &TEST_CMD_SIMPLE_ONLY, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_simple_only_error(void)
{
  const char* response = "\r\nERROR\r\n";
  TEST_ASSERT_TRUE(has_command_terminated(response, &TEST_CMD_SIMPLE_ONLY, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_simple_only_cme_error(void)
{
  const char* response = "\r\n+CME ERROR: 123\r\n";
  TEST_ASSERT_TRUE(has_command_terminated(response, &TEST_CMD_SIMPLE_ONLY, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_simple_only_incomplete(void)
{
  const char* response = "\r\nOK"; // Missing \r\n
  TEST_ASSERT_FALSE(has_command_terminated(response, &TEST_CMD_SIMPLE_ONLY, AT_CMD_TYPE_READ));
}

// Test with required data
static void test_has_cmd_terminated_data_required_with_data(void)
{
  const char* response = "\r\n+TEST: 24,99\r\nOK\r\n";
  TEST_ASSERT_TRUE(has_command_terminated(response, &TEST_CMD_DATA_REQUIRED, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_data_required_no_data(void)
{
  const char* response = "\r\nOK\r\n";
  TEST_ASSERT_FALSE(has_command_terminated(response, &TEST_CMD_DATA_REQUIRED, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_data_required_incomplete_data(void)
{
  const char* response = "\r\n+TEST: 24,99\r\nOK";
  TEST_ASSERT_FALSE(has_command_terminated(response, &TEST_CMD_DATA_REQUIRED, AT_CMD_TYPE_READ));
}

// Test with optional data
static void test_has_cmd_terminated_data_optional_with_data(void)
{
  const char* response = "\r\n+TEST: 24,99\r\nOK\r\n";
  TEST_ASSERT_TRUE(has_command_terminated(response, &TEST_CMD_DATA_OPTIONAL, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_data_optional_no_data(void)
{
  const char* response = "\r\nOK\r\n";
  TEST_ASSERT_TRUE(has_command_terminated(response, &TEST_CMD_DATA_OPTIONAL, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_data_optional_incomplete(void)
{
  const char* response = "\r\n+TEST: 24,99\r\nOK";
  TEST_ASSERT_FALSE(has_command_terminated(response, &TEST_CMD_DATA_OPTIONAL, AT_CMD_TYPE_READ));
}

// QMTOPEN specific tests (data optional)
static void test_has_cmd_terminated_qmtopen_with_data(void)
{
  const char* response = "\r\n+QMTOPEN: 0,\"iot-as-mqtt.cn-shanghai.aliyuncs.com\",1883\r\nOK\r\n";
  TEST_ASSERT_TRUE(has_command_terminated(response, &TEST_QMTOPEN_CMD, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_qmtopen_no_data(void)
{
  const char* response = "\r\nOK\r\n";
  TEST_ASSERT_TRUE(has_command_terminated(response, &TEST_QMTOPEN_CMD, AT_CMD_TYPE_READ));
}

// Edge Cases and Invalid Input Tests
static void test_has_cmd_terminated_null_response(void)
{
  TEST_ASSERT_FALSE(has_command_terminated(NULL, &TEST_CMD_SIMPLE_ONLY, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_null_command(void)
{
  const char* response = "\r\nOK\r\n";
  TEST_ASSERT_FALSE(has_command_terminated(response, NULL, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_empty_response(void)
{
  const char* response = "";
  TEST_ASSERT_FALSE(has_command_terminated(response, &TEST_CMD_SIMPLE_ONLY, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_invalid_command_type(void)
{
  const char* response = "\r\nOK\r\n";
  TEST_ASSERT_FALSE(has_command_terminated(response, &TEST_CMD_SIMPLE_ONLY, AT_CMD_TYPE_MAX));
}

// Test Runner for has_command_terminated function
void run_has_command_terminated_tests(void)
{
  UNITY_BEGIN();

  printf("\n== HAS COMMAND TERMINATED Helper Tests ==\n");

  // Simple only response tests
  RUN_TEST(test_has_cmd_terminated_simple_only_ok);
  RUN_TEST(test_has_cmd_terminated_simple_only_error);
  RUN_TEST(test_has_cmd_terminated_simple_only_cme_error);
  RUN_TEST(test_has_cmd_terminated_simple_only_incomplete);

  // Required data response tests
  RUN_TEST(test_has_cmd_terminated_data_required_with_data);
  RUN_TEST(test_has_cmd_terminated_data_required_no_data);
  RUN_TEST(test_has_cmd_terminated_data_required_incomplete_data);

  // Optional data response tests
  RUN_TEST(test_has_cmd_terminated_data_optional_with_data);
  RUN_TEST(test_has_cmd_terminated_data_optional_no_data);
  RUN_TEST(test_has_cmd_terminated_data_optional_incomplete);

  // QMTOPEN specific tests
  RUN_TEST(test_has_cmd_terminated_qmtopen_with_data);
  RUN_TEST(test_has_cmd_terminated_qmtopen_no_data);

  // Edge cases and invalid input
  RUN_TEST(test_has_cmd_terminated_null_response);
  RUN_TEST(test_has_cmd_terminated_null_command);
  RUN_TEST(test_has_cmd_terminated_empty_response);
  RUN_TEST(test_has_cmd_terminated_invalid_command_type);

  UNITY_END();
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

static const at_cmd_t TEST_CMD_SIMPLE_WITH_PARSER = {
    .name        = "TEST",
    .description = "Test command simple only with parser",
    .type_info   = {[AT_CMD_TYPE_READ] = {.parser        = mock_parser,
                                          .formatter     = NULL,
                                          .response_type = AT_CMD_RESPONSE_TYPE_SIMPLE_ONLY}}};

static const at_cmd_t TEST_CMD_DATA_REQUIRED_WITH_PARSER = {
    .name        = "TEST",
    .description = "Test command data required with parser",
    .type_info   = {[AT_CMD_TYPE_READ] = {.parser        = mock_parser,
                                          .formatter     = NULL,
                                          .response_type = AT_CMD_RESPONSE_TYPE_DATA_REQUIRED}}};

static const at_cmd_t TEST_CMD_DATA_OPTIONAL_WITH_PARSER = {
    .name        = "TEST",
    .description = "Test command data optional with parser",
    .type_info   = {[AT_CMD_TYPE_READ] = {.parser        = mock_parser,
                                          .formatter     = NULL,
                                          .response_type = AT_CMD_RESPONSE_TYPE_DATA_OPTIONAL}}};

// Test parsing for simple only response type
static void test_parse_specific_data_simple_only(void)
{
  at_parsed_response_t parsed_base = {
      .has_basic_response = true, .basic_response_is_ok = true, .has_data_response = false};
  char parsed_data = 0;

  esp_err_t err = parse_at_cmd_specific_data_response(
      &TEST_CMD_SIMPLE_WITH_PARSER, AT_CMD_TYPE_READ, "OK", &parsed_base, &parsed_data);

  TEST_ASSERT_EQUAL(ESP_OK, err);
}

// Test parsing for data required - with data
static void test_parse_specific_data_required_with_data(void)
{
  at_parsed_response_t parsed_base = {.has_basic_response   = true,
                                      .basic_response_is_ok = true,
                                      .has_data_response    = true,
                                      .data_response        = "+TEST: 1",
                                      .data_response_len    = 8};
  char                 parsed_data = 0;

  esp_err_t err = parse_at_cmd_specific_data_response(&TEST_CMD_DATA_REQUIRED_WITH_PARSER,
                                                      AT_CMD_TYPE_READ,
                                                      "+TEST: 1\r\nOK\r\n",
                                                      &parsed_base,
                                                      &parsed_data);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL('+', parsed_data);
}

// Test parsing for data required - without data
static void test_parse_specific_data_required_no_data(void)
{
  at_parsed_response_t parsed_base = {
      .has_basic_response = true, .basic_response_is_ok = true, .has_data_response = false};
  char parsed_data = 0;

  esp_err_t err = parse_at_cmd_specific_data_response(
      &TEST_CMD_DATA_REQUIRED_WITH_PARSER, AT_CMD_TYPE_READ, "OK\r\n", &parsed_base, &parsed_data);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
}

// Test parsing for data optional - with data
static void test_parse_specific_data_optional_with_data(void)
{
  at_parsed_response_t parsed_base = {.has_basic_response   = true,
                                      .basic_response_is_ok = true,
                                      .has_data_response    = true,
                                      .data_response        = "+TEST: 1",
                                      .data_response_len    = 8};
  char                 parsed_data = 0;

  esp_err_t err = parse_at_cmd_specific_data_response(&TEST_CMD_DATA_OPTIONAL_WITH_PARSER,
                                                      AT_CMD_TYPE_READ,
                                                      "+TEST: 1\r\nOK\r\n",
                                                      &parsed_base,
                                                      &parsed_data);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL('+', parsed_data);
}

// Test parsing for data optional - without data
static void test_parse_specific_data_optional_no_data(void)
{
  at_parsed_response_t parsed_base = {
      .has_basic_response = true, .basic_response_is_ok = true, .has_data_response = false};
  char parsed_data = 0;

  esp_err_t err = parse_at_cmd_specific_data_response(
      &TEST_CMD_DATA_OPTIONAL_WITH_PARSER, AT_CMD_TYPE_READ, "OK\r\n", &parsed_base, &parsed_data);

  TEST_ASSERT_EQUAL(ESP_OK, err);
}

// Test with invalid response type
static void test_parse_specific_data_invalid_response_type(void)
{
  at_parsed_response_t parsed_base = {
      .has_basic_response = true, .basic_response_is_ok = true, .has_data_response = true};
  char parsed_data = 0;

  // Create a command with invalid response type
  const at_cmd_t test_cmd_invalid_type = {
      .name      = "TEST",
      .type_info = {[AT_CMD_TYPE_READ] = {
                        .parser        = mock_parser,
                        .formatter     = NULL,
                        .response_type = 99 // Invalid response type
                    }}};

  esp_err_t err = parse_at_cmd_specific_data_response(
      &test_cmd_invalid_type, AT_CMD_TYPE_READ, "+TEST: 1\r\nOK\r\n", &parsed_base, &parsed_data);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, err);
}

void run_parse_specific_data_response_tests(void)
{
  UNITY_BEGIN();

  printf("\n== PARSE SPECIFIC DATA RESPONSE Tests ==\n");
  RUN_TEST(test_parse_specific_data_simple_only);
  RUN_TEST(test_parse_specific_data_required_with_data);
  RUN_TEST(test_parse_specific_data_required_no_data);
  RUN_TEST(test_parse_specific_data_optional_with_data);
  RUN_TEST(test_parse_specific_data_optional_no_data);
  RUN_TEST(test_parse_specific_data_invalid_response_type);

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

// Main test runner
void run_test_at_cmd_handler_all(void)
{
  run_has_command_terminated_tests();
  run_validate_basic_response_tests();
  run_parse_specific_data_response_tests();
}
