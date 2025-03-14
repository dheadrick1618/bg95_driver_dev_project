// components/bg95_driver/test/test_at_cmd_qmtopen.c

#include "at_cmd_parser.h"
#include "at_cmd_qmtopen.h"

#include <esp_err.h>
#include <string.h>
#include <unity.h>

static const char* TAG = "TEST_QMTOPEN_AT_CMD";

// Test response examples
static const char* VALID_QMTOPEN_READ_RESPONSE =
    "\r\n+QMTOPEN: 0,\"mqtt.example.com\",1883\r\nOK\r\n";
static const char* VALID_QMTOPEN_READ_EMPTY_RESPONSE = "\r\nOK\r\n"; // No connection open
static const char* VALID_QMTOPEN_WRITE_OK_RESPONSE   = "\r\nOK\r\n"; // Initial OK response
static const char* VALID_QMTOPEN_WRITE_URC_SUCCESS =
    "\r\nOK\r\n+QMTOPEN: 2,0\r\n"; // URC with success result
static const char* VALID_QMTOPEN_WRITE_URC_FAILURE =
    "\r\nOK\r\n+QMTOPEN: 2,3\r\n"; // URC with failure (PDP activation error)
static const char* INVALID_QMTOPEN_READ_RESPONSE =
    "\r\n+QMTOPEN: 99,\"invalid.example.com\",99999\r\nOK\r\n"; // Invalid values
static const char* MALFORMED_QMTOPEN_READ_RESPONSE =
    "\r\n+QMTOPEN: \r\nOK\r\n"; // No data after prefix

// ===== Test Enum String Mapping =====

static void test_qmtopen_result_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING(
      "Failed to open network",
      enum_to_str(QMTOPEN_RESULT_FAILED_TO_OPEN, QMTOPEN_RESULT_MAP, QMTOPEN_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Network opened successfully",
      enum_to_str(QMTOPEN_RESULT_OPEN_SUCCESS, QMTOPEN_RESULT_MAP, QMTOPEN_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Wrong parameter",
      enum_to_str(QMTOPEN_RESULT_WRONG_PARAMETER, QMTOPEN_RESULT_MAP, QMTOPEN_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "MQTT client identifier is occupied",
      enum_to_str(QMTOPEN_RESULT_MQTT_ID_OCCUPIED, QMTOPEN_RESULT_MAP, QMTOPEN_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Failed to activate PDP",
      enum_to_str(QMTOPEN_RESULT_FAILED_ACTIVATE_PDP, QMTOPEN_RESULT_MAP, QMTOPEN_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Failed to parse domain name",
      enum_to_str(QMTOPEN_RESULT_FAILED_PARSE_DOMAIN, QMTOPEN_RESULT_MAP, QMTOPEN_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Network connection error",
      enum_to_str(QMTOPEN_RESULT_NETWORK_CONN_ERROR, QMTOPEN_RESULT_MAP, QMTOPEN_RESULT_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(99, QMTOPEN_RESULT_MAP, QMTOPEN_RESULT_MAP_SIZE));
}

// ===== Test Read Command Parser =====

static void test_qmtopen_read_parser_valid_response(void)
{
  qmtopen_read_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_READ].parser(VALID_QMTOPEN_READ_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_TRUE(response.present.has_host_name);
  TEST_ASSERT_TRUE(response.present.has_port);
  TEST_ASSERT_EQUAL(0, response.client_idx);
  TEST_ASSERT_EQUAL_STRING("mqtt.example.com", response.host_name);
  TEST_ASSERT_EQUAL(1883, response.port);
}

static void test_qmtopen_read_parser_empty_response(void)
{
  qmtopen_read_response_t response = {0};

  esp_err_t err = AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_READ].parser(
      VALID_QMTOPEN_READ_EMPTY_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx);
  TEST_ASSERT_FALSE(response.present.has_host_name);
  TEST_ASSERT_FALSE(response.present.has_port);
}

static void test_qmtopen_read_parser_invalid_response(void)
{
  qmtopen_read_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_READ].parser(INVALID_QMTOPEN_READ_RESPONSE, &response);

  // Should parse but not set invalid values
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx); // client_idx should be rejected (99)
  TEST_ASSERT_TRUE(response.present.has_host_name);
  TEST_ASSERT_EQUAL_STRING("invalid.example.com", response.host_name);
  TEST_ASSERT_FALSE(response.present.has_port); // port should be rejected (99999)
}

static void test_qmtopen_read_parser_malformed_response(void)
{
  qmtopen_read_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_READ].parser(MALFORMED_QMTOPEN_READ_RESPONSE, &response);

  // Should return OK but with no data
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx);
  TEST_ASSERT_FALSE(response.present.has_host_name);
  TEST_ASSERT_FALSE(response.present.has_port);
}

static void test_qmtopen_read_parser_null_args(void)
{
  qmtopen_read_response_t response = {0};

  esp_err_t err = AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_READ].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_READ].parser(VALID_QMTOPEN_READ_RESPONSE, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Write Command Formatter =====

static void test_qmtopen_write_formatter_valid_params(void)
{
  char                   buffer[64] = {0};
  qmtopen_write_params_t params     = {.client_idx = 2, .port = 8883};
  strncpy(params.host_name, "mqtt.example.org", sizeof(params.host_name) - 1);

  esp_err_t err =
      AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=2,\"mqtt.example.org\",8883", buffer);
}

static void test_qmtopen_write_formatter_edge_case_params(void)
{
  char buffer[64] = {0};

  // Test with minimum valid values
  qmtopen_write_params_t params_min = {.client_idx = 0, .port = 0};
  strncpy(params_min.host_name, "a", sizeof(params_min.host_name) - 1);

  esp_err_t err =
      AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].formatter(&params_min, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=0,\"a\",0", buffer);

  // Test with maximum valid values
  qmtopen_write_params_t params_max = {.client_idx = QMTOPEN_CLIENT_IDX_MAX,
                                       .port       = QMTOPEN_PORT_MAX};
  memset(params_max.host_name, 'b', sizeof(params_max.host_name) - 1);
  params_max.host_name[sizeof(params_max.host_name) - 1] = '\0';

  memset(buffer, 0, sizeof(buffer));
  err = AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].formatter(&params_max, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  // We can't test the entire string due to its length, but we can check parts
  TEST_ASSERT_EQUAL_STRING_LEN("=5,\"", buffer, 4);
  TEST_ASSERT_EQUAL('b', buffer[4]); // Should have 'b' at position 4
}

static void test_qmtopen_write_formatter_invalid_client_idx(void)
{
  char                   buffer[64] = {0};
  qmtopen_write_params_t params = {.client_idx = QMTOPEN_CLIENT_IDX_MAX + 1, // Invalid: too high
                                   .port       = 1883};
  strncpy(params.host_name, "mqtt.example.com", sizeof(params.host_name) - 1);

  esp_err_t err =
      AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtopen_write_formatter_invalid_port(void)
{
  char                   buffer[64] = {0};
  qmtopen_write_params_t params     = {
          .client_idx = 1,
          .port       = 65536 // Invalid: too high
  };
  strncpy(params.host_name, "mqtt.example.com", sizeof(params.host_name) - 1);

  esp_err_t err =
      AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtopen_write_formatter_empty_hostname(void)
{
  char                   buffer[64] = {0};
  qmtopen_write_params_t params     = {
          .client_idx = 1,
          .port       = 1883,
          .host_name  = "" // Invalid: empty
  };

  esp_err_t err =
      AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtopen_write_formatter_buffer_too_small(void)
{
  char                   buffer[5] = {0}; // Too small to hold the result
  qmtopen_write_params_t params    = {.client_idx = 1, .port = 1883};
  strncpy(params.host_name, "mqtt.example.com", sizeof(params.host_name) - 1);

  esp_err_t err =
      AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
}

static void test_qmtopen_write_formatter_null_args(void)
{
  char                   buffer[64] = {0};
  qmtopen_write_params_t params     = {.client_idx = 1, .port = 1883};
  strncpy(params.host_name, "mqtt.example.com", sizeof(params.host_name) - 1);

  esp_err_t err =
      AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].formatter(NULL, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, NULL, sizeof(buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, 0);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Write Command Parser =====

static void test_qmtopen_write_parser_ok_only_response(void)
{
  qmtopen_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].parser(
      VALID_QMTOPEN_WRITE_OK_RESPONSE, &response);

  // Should not fail, but also not populate any response data yet
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx);
  TEST_ASSERT_FALSE(response.present.has_result);
}

static void test_qmtopen_write_parser_success_response(void)
{
  qmtopen_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].parser(
      VALID_QMTOPEN_WRITE_URC_SUCCESS, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(2, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTOPEN_RESULT_OPEN_SUCCESS, response.result);
}

static void test_qmtopen_write_parser_failure_response(void)
{
  qmtopen_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].parser(
      VALID_QMTOPEN_WRITE_URC_FAILURE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(2, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTOPEN_RESULT_FAILED_ACTIVATE_PDP, response.result);
}

static void test_qmtopen_write_parser_null_args(void)
{
  qmtopen_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTOPEN_WRITE_URC_SUCCESS, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Command Definition =====

static void test_qmtopen_command_definition(void)
{
  TEST_ASSERT_EQUAL_STRING("QMTOPEN", AT_CMD_QMTOPEN.name);
  TEST_ASSERT_EQUAL_STRING("Open a Network Connection for MQTT Client", AT_CMD_QMTOPEN.description);

  // Test command should be not implemented as requested
  TEST_ASSERT_NULL(AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_TEST].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_TEST].formatter);

  // Read command should have parser but no formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_READ].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_READ].formatter);

  // Write command should have both parser and formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].parser);
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_WRITE].formatter);

  // Execute command should not exist
  TEST_ASSERT_NULL(AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_EXECUTE].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_EXECUTE].formatter);
}

// ===== Integration with AT command parser =====

static void test_qmtopen_at_parser_integration(void)
{
  at_parsed_response_t    base_response    = {0};
  qmtopen_read_response_t qmtopen_response = {0};

  // Parse base response
  esp_err_t err = at_cmd_parse_response(VALID_QMTOPEN_READ_RESPONSE, &base_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(base_response.has_basic_response);
  TEST_ASSERT_TRUE(base_response.basic_response_is_ok);
  TEST_ASSERT_TRUE(base_response.has_data_response);

  // Parse command-specific data
  err = AT_CMD_QMTOPEN.type_info[AT_CMD_TYPE_READ].parser(VALID_QMTOPEN_READ_RESPONSE,
                                                          &qmtopen_response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(qmtopen_response.present.has_client_idx);
  TEST_ASSERT_EQUAL(0, qmtopen_response.client_idx);
  TEST_ASSERT_TRUE(qmtopen_response.present.has_host_name);
  TEST_ASSERT_EQUAL_STRING("mqtt.example.com", qmtopen_response.host_name);
  TEST_ASSERT_TRUE(qmtopen_response.present.has_port);
  TEST_ASSERT_EQUAL(1883, qmtopen_response.port);
}

// ===== Run All Tests =====

void run_test_at_cmd_qmtopen_all(void)
{
  UNITY_BEGIN();

  // Enum mapping tests
  RUN_TEST(test_qmtopen_result_to_str);

  // Read command parser tests
  RUN_TEST(test_qmtopen_read_parser_valid_response);
  RUN_TEST(test_qmtopen_read_parser_empty_response);
  RUN_TEST(test_qmtopen_read_parser_invalid_response);
  RUN_TEST(test_qmtopen_read_parser_malformed_response);
  RUN_TEST(test_qmtopen_read_parser_null_args);

  // Write command formatter tests
  RUN_TEST(test_qmtopen_write_formatter_valid_params);
  RUN_TEST(test_qmtopen_write_formatter_edge_case_params);
  RUN_TEST(test_qmtopen_write_formatter_invalid_client_idx);
  RUN_TEST(test_qmtopen_write_formatter_invalid_port);
  RUN_TEST(test_qmtopen_write_formatter_empty_hostname);
  RUN_TEST(test_qmtopen_write_formatter_buffer_too_small);
  RUN_TEST(test_qmtopen_write_formatter_null_args);

  // Write command parser tests
  RUN_TEST(test_qmtopen_write_parser_ok_only_response);
  RUN_TEST(test_qmtopen_write_parser_success_response);
  RUN_TEST(test_qmtopen_write_parser_failure_response);
  RUN_TEST(test_qmtopen_write_parser_null_args);

  // Command definition tests
  RUN_TEST(test_qmtopen_command_definition);

  // Integration tests
  RUN_TEST(test_qmtopen_at_parser_integration);

  UNITY_END();
}
