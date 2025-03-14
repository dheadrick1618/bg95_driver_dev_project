#include "at_cmd_parser.h"
#include "at_cmd_qmtconn.h"

#include <esp_err.h>
#include <string.h>
#include <unity.h>

static const char* TAG = "TEST_QMTCONN_AT_CMD";

// Test response examples
static const char* VALID_QMTCONN_TEST_RESPONSE =
    "\r\n+QMTCONN: (0-5),<clientID>,<username>,<password>\r\nOK\r\n";
static const char* VALID_QMTCONN_READ_RESPONSE =
    "\r\n+QMTCONN: 2,3\r\nOK\r\n"; // Client 2 is connected (state 3)
static const char* VALID_QMTCONN_READ_EMPTY_RESPONSE = "\r\nOK\r\n"; // No clients connected
static const char* VALID_QMTCONN_WRITE_OK_RESPONSE   = "\r\nOK\r\n"; // Initial OK response
static const char* VALID_QMTCONN_WRITE_URC_SUCCESS =
    "\r\nOK\r\n+QMTCONN: 1,0,0\r\n"; // URC with success result and accepted
static const char* VALID_QMTCONN_WRITE_URC_FAILURE =
    "\r\nOK\r\n+QMTCONN: 1,2,4\r\n"; // URC with failure result and bad credentials
static const char* INVALID_QMTCONN_READ_RESPONSE =
    "\r\n+QMTCONN: 99,99\r\nOK\r\n"; // Invalid client_idx and state
static const char* MALFORMED_QMTCONN_READ_RESPONSE =
    "\r\n+QMTCONN: \r\nOK\r\n"; // No data after prefix

// ===== Test Enum String Mapping =====

static void test_qmtconn_result_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING(
      "Packet sent successfully and ACK received",
      enum_to_str(QMTCONN_RESULT_SUCCESS, QMTCONN_RESULT_MAP, QMTCONN_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Packet retransmission",
      enum_to_str(QMTCONN_RESULT_RETRANSMISSION, QMTCONN_RESULT_MAP, QMTCONN_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Failed to send a packet",
      enum_to_str(QMTCONN_RESULT_FAILED_TO_SEND, QMTCONN_RESULT_MAP, QMTCONN_RESULT_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(99, QMTCONN_RESULT_MAP, QMTCONN_RESULT_MAP_SIZE));
}

static void test_qmtconn_state_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING(
      "MQTT is initializing",
      enum_to_str(QMTCONN_STATE_INITIALIZING, QMTCONN_STATE_MAP, QMTCONN_STATE_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "MQTT is connecting",
      enum_to_str(QMTCONN_STATE_CONNECTING, QMTCONN_STATE_MAP, QMTCONN_STATE_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "MQTT is connected",
      enum_to_str(QMTCONN_STATE_CONNECTED, QMTCONN_STATE_MAP, QMTCONN_STATE_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "MQTT is disconnecting",
      enum_to_str(QMTCONN_STATE_DISCONNECTING, QMTCONN_STATE_MAP, QMTCONN_STATE_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(99, QMTCONN_STATE_MAP, QMTCONN_STATE_MAP_SIZE));
}

static void test_qmtconn_ret_code_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING(
      "Connection Accepted",
      enum_to_str(QMTCONN_RET_CODE_ACCEPTED, QMTCONN_RET_CODE_MAP, QMTCONN_RET_CODE_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING("Connection Refused: Unacceptable Protocol Version",
                           enum_to_str(QMTCONN_RET_CODE_UNACCEPTABLE_PROTOCOL,
                                       QMTCONN_RET_CODE_MAP,
                                       QMTCONN_RET_CODE_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING("Connection Refused: Identifier Rejected",
                           enum_to_str(QMTCONN_RET_CODE_IDENTIFIER_REJECTED,
                                       QMTCONN_RET_CODE_MAP,
                                       QMTCONN_RET_CODE_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING("Connection Refused: Server Unavailable",
                           enum_to_str(QMTCONN_RET_CODE_SERVER_UNAVAILABLE,
                                       QMTCONN_RET_CODE_MAP,
                                       QMTCONN_RET_CODE_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING("Connection Refused: Bad Username or Password",
                           enum_to_str(QMTCONN_RET_CODE_BAD_CREDENTIALS,
                                       QMTCONN_RET_CODE_MAP,
                                       QMTCONN_RET_CODE_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING("Connection Refused: Not Authorized",
                           enum_to_str(QMTCONN_RET_CODE_NOT_AUTHORIZED,
                                       QMTCONN_RET_CODE_MAP,
                                       QMTCONN_RET_CODE_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN",
                           enum_to_str(99, QMTCONN_RET_CODE_MAP, QMTCONN_RET_CODE_MAP_SIZE));
}

// ===== Test Test Command Parser =====

static void test_qmtconn_test_parser_valid_response(void)
{
  qmtconn_test_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_TEST].parser(VALID_QMTCONN_TEST_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(0, response.client_idx_min);
  TEST_ASSERT_EQUAL(5, response.client_idx_max);
}

static void test_qmtconn_test_parser_malformed_response(void)
{
  qmtconn_test_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_TEST].parser(MALFORMED_QMTCONN_READ_RESPONSE, &response);

  // Should fail with invalid response
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
}

static void test_qmtconn_test_parser_null_args(void)
{
  qmtconn_test_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_TEST].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_TEST].parser(VALID_QMTCONN_TEST_RESPONSE, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Read Command Parser =====

static void test_qmtconn_read_parser_valid_response(void)
{
  qmtconn_read_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_READ].parser(VALID_QMTCONN_READ_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(2, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_state);
  TEST_ASSERT_EQUAL(QMTCONN_STATE_CONNECTED, response.state);
}

static void test_qmtconn_read_parser_empty_response(void)
{
  qmtconn_read_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_READ].parser(
      VALID_QMTCONN_READ_EMPTY_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx);
  TEST_ASSERT_FALSE(response.present.has_state);
}

static void test_qmtconn_read_parser_invalid_response(void)
{
  qmtconn_read_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_READ].parser(INVALID_QMTCONN_READ_RESPONSE, &response);

  // Should process but not populate fields with invalid values
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx);
  TEST_ASSERT_FALSE(response.present.has_state);
}

static void test_qmtconn_read_parser_malformed_response(void)
{
  qmtconn_read_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_READ].parser(MALFORMED_QMTCONN_READ_RESPONSE, &response);

  // Should fail with invalid response
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
}

static void test_qmtconn_read_parser_null_args(void)
{
  qmtconn_read_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_READ].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_READ].parser(VALID_QMTCONN_READ_RESPONSE, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Write Command Formatter =====

static void test_qmtconn_write_formatter_client_id_only(void)
{
  char                   buffer[64] = {0};
  qmtconn_write_params_t params     = {.client_idx = 1,
                                       .present    = {.has_username = false, .has_password = false}};
  strncpy(params.client_id, "ESP32Test", sizeof(params.client_id) - 1);

  esp_err_t err =
      AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=1,\"ESP32Test\"", buffer);
}

static void test_qmtconn_write_formatter_with_username(void)
{
  char                   buffer[64] = {0};
  qmtconn_write_params_t params     = {.client_idx = 2,
                                       .present    = {.has_username = true, .has_password = false}};
  strncpy(params.client_id, "ESP32Test", sizeof(params.client_id) - 1);
  strncpy(params.username, "testuser", sizeof(params.username) - 1);

  esp_err_t err =
      AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=2,\"ESP32Test\",\"testuser\"", buffer);
}

static void test_qmtconn_write_formatter_with_username_password(void)
{
  char                   buffer[128] = {0};
  qmtconn_write_params_t params      = {.client_idx = 3,
                                        .present    = {.has_username = true, .has_password = true}};
  strncpy(params.client_id, "ESP32Test", sizeof(params.client_id) - 1);
  strncpy(params.username, "testuser", sizeof(params.username) - 1);
  strncpy(params.password, "testpass", sizeof(params.password) - 1);

  esp_err_t err =
      AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=3,\"ESP32Test\",\"testuser\",\"testpass\"", buffer);
}

static void test_qmtconn_write_formatter_invalid_client_idx(void)
{
  char                   buffer[64] = {0};
  qmtconn_write_params_t params = {.client_idx = QMTCONN_CLIENT_IDX_MAX + 1, // Invalid: too high
                                   .present    = {.has_username = false, .has_password = false}};
  strncpy(params.client_id, "ESP32Test", sizeof(params.client_id) - 1);

  esp_err_t err =
      AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtconn_write_formatter_empty_client_id(void)
{
  char                   buffer[64] = {0};
  qmtconn_write_params_t params     = {.client_idx = 1,
                                       .client_id  = "", // Invalid: empty
                                       .present    = {.has_username = false, .has_password = false}};

  esp_err_t err =
      AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtconn_write_formatter_password_without_username(void)
{
  char                   buffer[64] = {0};
  qmtconn_write_params_t params     = {.client_idx = 1,
                                       .present    = {
                                              .has_username = false,
                                              .has_password = true // Invalid: password without username
                                   }};
  strncpy(params.client_id, "ESP32Test", sizeof(params.client_id) - 1);
  strncpy(params.password, "testpass", sizeof(params.password) - 1);

  esp_err_t err =
      AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtconn_write_formatter_buffer_too_small(void)
{
  char                   buffer[5] = {0}; // Too small to hold the result
  qmtconn_write_params_t params    = {.client_idx = 1,
                                      .present    = {.has_username = false, .has_password = false}};
  strncpy(params.client_id, "ESP32Test", sizeof(params.client_id) - 1);

  esp_err_t err =
      AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
}

static void test_qmtconn_write_formatter_null_args(void)
{
  char                   buffer[64] = {0};
  qmtconn_write_params_t params     = {.client_idx = 1,
                                       .present    = {.has_username = false, .has_password = false}};
  strncpy(params.client_id, "ESP32Test", sizeof(params.client_id) - 1);

  esp_err_t err =
      AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].formatter(NULL, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, NULL, sizeof(buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, 0);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Write Command Parser =====

static void test_qmtconn_write_parser_ok_only_response(void)
{
  qmtconn_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].parser(
      VALID_QMTCONN_WRITE_OK_RESPONSE, &response);

  // Should not fail, but also not populate any response data yet
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx);
  TEST_ASSERT_FALSE(response.present.has_result);
  TEST_ASSERT_FALSE(response.present.has_ret_code);
}

static void test_qmtconn_write_parser_success_response(void)
{
  qmtconn_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].parser(
      VALID_QMTCONN_WRITE_URC_SUCCESS, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(1, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTCONN_RESULT_SUCCESS, response.result);
  TEST_ASSERT_TRUE(response.present.has_ret_code);
  TEST_ASSERT_EQUAL(QMTCONN_RET_CODE_ACCEPTED, response.ret_code);
}

static void test_qmtconn_write_parser_failure_response(void)
{
  qmtconn_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].parser(
      VALID_QMTCONN_WRITE_URC_FAILURE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(1, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTCONN_RESULT_FAILED_TO_SEND, response.result);
  TEST_ASSERT_TRUE(response.present.has_ret_code);
  TEST_ASSERT_EQUAL(QMTCONN_RET_CODE_BAD_CREDENTIALS, response.ret_code);
}

static void test_qmtconn_write_parser_null_args(void)
{
  qmtconn_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTCONN_WRITE_URC_SUCCESS, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Command Definition =====

static void test_qmtconn_command_definition(void)
{
  TEST_ASSERT_EQUAL_STRING("QMTCONN", AT_CMD_QMTCONN.name);
  TEST_ASSERT_EQUAL_STRING("Connect a Client to MQTT Server", AT_CMD_QMTCONN.description);

  // Test command should have parser but no formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_TEST].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_TEST].formatter);

  // Read command should have parser but no formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_READ].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_READ].formatter);

  // Write command should have both parser and formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].parser);
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_WRITE].formatter);

  // Execute command should not exist
  TEST_ASSERT_NULL(AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_EXECUTE].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTCONN.type_info[AT_CMD_TYPE_EXECUTE].formatter);

  // Timeout should be 5000ms as per specification
  TEST_ASSERT_EQUAL(5000, AT_CMD_QMTCONN.timeout_ms);
}

// ===== Integration with AT command parser =====

static void test_qmtconn_at_parser_integration(void)
{
  at_parsed_response_t base_response = {0};

  // Parse base response for test command
  esp_err_t err = at_cmd_parse_response(VALID_QMTCONN_TEST_RESPONSE, &base_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(base_response.has_basic_response);
  TEST_ASSERT_TRUE(base_response.basic_response_is_ok);
  TEST_ASSERT_TRUE(base_response.has_data_response);

  // Parse base response for read command
  memset(&base_response, 0, sizeof(base_response));
  err = at_cmd_parse_response(VALID_QMTCONN_READ_RESPONSE, &base_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(base_response.has_basic_response);
  TEST_ASSERT_TRUE(base_response.basic_response_is_ok);
  TEST_ASSERT_TRUE(base_response.has_data_response);

  // Parse base response for write command with URC
  memset(&base_response, 0, sizeof(base_response));
  err = at_cmd_parse_response(VALID_QMTCONN_WRITE_URC_SUCCESS, &base_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(base_response.has_basic_response);
  TEST_ASSERT_TRUE(base_response.basic_response_is_ok);
  TEST_ASSERT_TRUE(base_response.has_data_response);
}

// ===== Run All Tests =====

void run_test_at_cmd_qmtconn_all(void)
{
  UNITY_BEGIN();

  // Enum mapping tests
  RUN_TEST(test_qmtconn_result_to_str);
  RUN_TEST(test_qmtconn_state_to_str);
  RUN_TEST(test_qmtconn_ret_code_to_str);

  // Test command parser tests
  RUN_TEST(test_qmtconn_test_parser_valid_response);
  RUN_TEST(test_qmtconn_test_parser_malformed_response);
  RUN_TEST(test_qmtconn_test_parser_null_args);

  // Read command parser tests
  RUN_TEST(test_qmtconn_read_parser_valid_response);
  RUN_TEST(test_qmtconn_read_parser_empty_response);
  RUN_TEST(test_qmtconn_read_parser_invalid_response);
  RUN_TEST(test_qmtconn_read_parser_malformed_response);
  RUN_TEST(test_qmtconn_read_parser_null_args);

  // Write command formatter tests
  RUN_TEST(test_qmtconn_write_formatter_client_id_only);
  RUN_TEST(test_qmtconn_write_formatter_with_username);
  RUN_TEST(test_qmtconn_write_formatter_with_username_password);
  RUN_TEST(test_qmtconn_write_formatter_invalid_client_idx);
  RUN_TEST(test_qmtconn_write_formatter_empty_client_id);
  RUN_TEST(test_qmtconn_write_formatter_password_without_username);
  RUN_TEST(test_qmtconn_write_formatter_buffer_too_small);
  RUN_TEST(test_qmtconn_write_formatter_null_args);

  // Write command parser tests
  RUN_TEST(test_qmtconn_write_parser_ok_only_response);
  RUN_TEST(test_qmtconn_write_parser_success_response);
  RUN_TEST(test_qmtconn_write_parser_failure_response);
  RUN_TEST(test_qmtconn_write_parser_null_args);

  // Command definition tests
  RUN_TEST(test_qmtconn_command_definition);

  // Integration tests
  RUN_TEST(test_qmtconn_at_parser_integration);

  UNITY_END();
}
