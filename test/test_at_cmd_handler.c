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

// Test command definitions
static const at_cmd_t TEST_CMD_NO_PARSER = {
    .name        = "TEST",
    .description = "Test command",
    .type_info   = {[AT_CMD_TYPE_READ] = {.parser = NULL, .formatter = NULL}}};

static esp_err_t dummy_parser(const char* response, void* parsed_data)
{
  return ESP_OK;
}

static const at_cmd_t TEST_CMD_WITH_PARSER = {
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
  TEST_ASSERT_TRUE(has_command_terminated(response, &TEST_CMD_WITH_PARSER, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_fxn_ok_before_data_response(void)
{
  const char* response = "\r\nOK\r\n+QMTOPEN: 0,0\r\n";
  TEST_ASSERT_TRUE(has_command_terminated(response, &TEST_CMD_WITH_PARSER, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_fxn_data_response_no_ok(void)
{
  const char* response = "\r\n+QMTOPEN: 0,0\r\n";
  TEST_ASSERT_FALSE(has_command_terminated(response, &TEST_CMD_WITH_PARSER, AT_CMD_TYPE_READ));
}

static void test_has_cmd_terminated_fxn_incomplete_data_response(void)
{
  const char* response = "\r\nOK\r\n+QMTOPEN: 0,0";
  TEST_ASSERT_FALSE(has_command_terminated(response, &TEST_CMD_WITH_PARSER, AT_CMD_TYPE_READ));
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

void run_test_at_cmd_handler_all(void)
{

  run_has_command_terminated_fxn_tests();

  // UNITY_BEGIN();

  // Basic response tests
  // RUN_TEST(test_basic_ok_response);

  // UNITY_END();
}
