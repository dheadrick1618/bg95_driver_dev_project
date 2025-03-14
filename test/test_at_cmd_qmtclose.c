#include "at_cmd_parser.h"
#include "at_cmd_qmtclose.h"

#include <esp_err.h>
#include <string.h>
#include <unity.h>

static const char* TAG = "TEST_QMTCLOSE_AT_CMD";

// Test response examples
static const char* VALID_QMTCLOSE_TEST_RESPONSE     = "\r\n+QMTCLOSE: (0-5)\r\nOK\r\n";
static const char* VALID_QMTCLOSE_WRITE_OK_RESPONSE = "\r\nOK\r\n"; // Initial OK response
static const char* VALID_QMTCLOSE_WRITE_URC_SUCCESS =
    "\r\nOK\r\n+QMTCLOSE: 3,0\r\n"; // URC with success result
static const char* VALID_QMTCLOSE_WRITE_URC_FAILURE =
    "\r\nOK\r\n+QMTCLOSE: 3,-1\r\n"; // URC with failure result
static const char* INVALID_QMTCLOSE_TEST_RESPONSE =
    "\r\n+QMTCLOSE: (99-100)\r\nOK\r\n"; // Invalid values
static const char* MALFORMED_QMTCLOSE_TEST_RESPONSE =
    "\r\n+QMTCLOSE: \r\nOK\r\n"; // No data after prefix

// ===== Test Enum String Mapping =====

static void test_qmtclose_result_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING(
      "Failed to close network",
      enum_to_str(QMTCLOSE_RESULT_FAILED_TO_CLOSE, QMTCLOSE_RESULT_MAP, QMTCLOSE_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Network closed successfully",
      enum_to_str(QMTCLOSE_RESULT_CLOSE_SUCCESS, QMTCLOSE_RESULT_MAP, QMTCLOSE_RESULT_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN",
                           enum_to_str(99, QMTCLOSE_RESULT_MAP, QMTCLOSE_RESULT_MAP_SIZE));
}

// ===== Test Test Command Parser =====

static void test_qmtclose_test_parser_valid_response(void)
{
  qmtclose_test_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_TEST].parser(VALID_QMTCLOSE_TEST_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(0, response.client_idx_min);
  TEST_ASSERT_EQUAL(5, response.client_idx_max);
}

static void test_qmtclose_test_parser_invalid_response(void)
{
  qmtclose_test_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_TEST].parser(INVALID_QMTCLOSE_TEST_RESPONSE, &response);

  // Should successfully parse even with out-of-range values
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(99, response.client_idx_min);
  TEST_ASSERT_EQUAL(100, response.client_idx_max);
}

static void test_qmtclose_test_parser_malformed_response(void)
{
  qmtclose_test_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_TEST].parser(
      MALFORMED_QMTCLOSE_TEST_RESPONSE, &response);

  // Should fail with invalid response
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
}

static void test_qmtclose_test_parser_null_args(void)
{
  qmtclose_test_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_TEST].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_TEST].parser(VALID_QMTCLOSE_TEST_RESPONSE, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Write Command Formatter =====

static void test_qmtclose_write_formatter_valid_params(void)
{
  char                    buffer[16] = {0};
  qmtclose_write_params_t params     = {.client_idx = 3};

  esp_err_t err =
      AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=3", buffer);
}

static void test_qmtclose_write_formatter_edge_case_params(void)
{
  char buffer[16] = {0};

  // Test with minimum valid value
  qmtclose_write_params_t params_min = {.client_idx = 0};

  esp_err_t err =
      AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].formatter(&params_min, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=0", buffer);

  // Test with maximum valid value
  qmtclose_write_params_t params_max = {.client_idx = QMTCLOSE_CLIENT_IDX_MAX};

  memset(buffer, 0, sizeof(buffer));
  err = AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].formatter(&params_max, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=5", buffer);
}

static void test_qmtclose_write_formatter_invalid_client_idx(void)
{
  char                    buffer[16] = {0};
  qmtclose_write_params_t params     = {
          .client_idx = QMTCLOSE_CLIENT_IDX_MAX + 1 // Invalid: too high
  };

  esp_err_t err =
      AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtclose_write_formatter_buffer_too_small(void)
{
  char                    buffer[1] = {0}; // Too small to hold the result
  qmtclose_write_params_t params    = {.client_idx = 3};

  esp_err_t err =
      AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
}

static void test_qmtclose_write_formatter_null_args(void)
{
  char                    buffer[16] = {0};
  qmtclose_write_params_t params     = {.client_idx = 3};

  esp_err_t err =
      AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].formatter(NULL, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].formatter(&params, NULL, sizeof(buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, 0);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Write Command Parser =====

static void test_qmtclose_write_parser_ok_only_response(void)
{
  qmtclose_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].parser(
      VALID_QMTCLOSE_WRITE_OK_RESPONSE, &response);

  // Should not fail, but also not populate any response data yet
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx);
  TEST_ASSERT_FALSE(response.present.has_result);
}

static void test_qmtclose_write_parser_success_response(void)
{
  qmtclose_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].parser(
      VALID_QMTCLOSE_WRITE_URC_SUCCESS, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(3, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTCLOSE_RESULT_CLOSE_SUCCESS, response.result);
}

static void test_qmtclose_write_parser_failure_response(void)
{
  qmtclose_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].parser(
      VALID_QMTCLOSE_WRITE_URC_FAILURE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(3, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTCLOSE_RESULT_FAILED_TO_CLOSE, response.result);
}

static void test_qmtclose_write_parser_null_args(void)
{
  qmtclose_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTCLOSE_WRITE_URC_SUCCESS, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Command Definition =====

static void test_qmtclose_command_definition(void)
{
  TEST_ASSERT_EQUAL_STRING("QMTCLOSE", AT_CMD_QMTCLOSE.name);
  TEST_ASSERT_EQUAL_STRING("Close a Network Connection for MQTT Client",
                           AT_CMD_QMTCLOSE.description);

  // Test command should have parser but no formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_TEST].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_TEST].formatter);

  // Read command should not exist
  TEST_ASSERT_NULL(AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_READ].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_READ].formatter);

  // Write command should have both parser and formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].parser);
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_WRITE].formatter);

  // Execute command should not exist
  TEST_ASSERT_NULL(AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_EXECUTE].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTCLOSE.type_info[AT_CMD_TYPE_EXECUTE].formatter);

  // Timeout should be 300ms as per specification
  TEST_ASSERT_EQUAL(300, AT_CMD_QMTCLOSE.timeout_ms);
}

// ===== Integration with AT command parser =====

static void test_qmtclose_at_parser_integration(void)
{
  at_parsed_response_t base_response = {0};

  // Parse base response for test command
  esp_err_t err = at_cmd_parse_response(VALID_QMTCLOSE_TEST_RESPONSE, &base_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(base_response.has_basic_response);
  TEST_ASSERT_TRUE(base_response.basic_response_is_ok);
  TEST_ASSERT_TRUE(base_response.has_data_response);

  // Parse base response for write command with URC
  memset(&base_response, 0, sizeof(base_response));
  err = at_cmd_parse_response(VALID_QMTCLOSE_WRITE_URC_SUCCESS, &base_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(base_response.has_basic_response);
  TEST_ASSERT_TRUE(base_response.basic_response_is_ok);
  TEST_ASSERT_TRUE(base_response.has_data_response);
}

// ===== Run All Tests =====

void run_test_at_cmd_qmtclose_all(void)
{
  UNITY_BEGIN();

  // Enum mapping tests
  RUN_TEST(test_qmtclose_result_to_str);

  // Test command parser tests
  RUN_TEST(test_qmtclose_test_parser_valid_response);
  RUN_TEST(test_qmtclose_test_parser_invalid_response);
  RUN_TEST(test_qmtclose_test_parser_malformed_response);
  RUN_TEST(test_qmtclose_test_parser_null_args);

  // Write command formatter tests
  RUN_TEST(test_qmtclose_write_formatter_valid_params);
  RUN_TEST(test_qmtclose_write_formatter_edge_case_params);
  RUN_TEST(test_qmtclose_write_formatter_invalid_client_idx);
  RUN_TEST(test_qmtclose_write_formatter_buffer_too_small);
  RUN_TEST(test_qmtclose_write_formatter_null_args);

  // Write command parser tests
  RUN_TEST(test_qmtclose_write_parser_ok_only_response);
  RUN_TEST(test_qmtclose_write_parser_success_response);
  RUN_TEST(test_qmtclose_write_parser_failure_response);
  RUN_TEST(test_qmtclose_write_parser_null_args);

  // Command definition tests
  RUN_TEST(test_qmtclose_command_definition);

  // Integration tests
  RUN_TEST(test_qmtclose_at_parser_integration);

  UNITY_END();
}
