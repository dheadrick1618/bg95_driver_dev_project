#include "at_cmd_parser.h"
#include "at_cmd_qmtpub.h"

#include <esp_err.h>
#include <string.h>
#include <unity.h>

static const char* TAG = "TEST_QMTPUB_AT_CMD";

// Test response examples
static const char* VALID_QMTPUB_WRITE_OK_RESPONSE = "\r\nOK\r\n"; // Initial OK response
static const char* VALID_QMTPUB_WRITE_URC_SUCCESS =
    "\r\nOK\r\n+QMTPUB: 0,1,0\r\n"; // Success result
static const char* VALID_QMTPUB_WRITE_URC_RETRANSMISSION =
    "\r\nOK\r\n+QMTPUB: 2,10,1,3\r\n"; // Retransmission (3 attempts)
static const char* VALID_QMTPUB_WRITE_URC_FAILURE =
    "\r\nOK\r\n+QMTPUB: 5,100,2\r\n"; // Failed to send
static const char* INVALID_QMTPUB_WRITE_RESPONSE =
    "\r\nOK\r\n+QMTPUB: 99,99999,99\r\n"; // Invalid values
static const char* MALFORMED_QMTPUB_WRITE_RESPONSE =
    "\r\nOK\r\n+QMTPUB: \r\n"; // No data after prefix

// ===== Test Enum String Mapping =====

static void test_qmtpub_result_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING(
      "Packet sent successfully and ACK received",
      enum_to_str(QMTPUB_RESULT_SUCCESS, QMTPUB_RESULT_MAP, QMTPUB_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Packet retransmission",
      enum_to_str(QMTPUB_RESULT_RETRANSMISSION, QMTPUB_RESULT_MAP, QMTPUB_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Failed to send a packet",
      enum_to_str(QMTPUB_RESULT_FAILED_TO_SEND, QMTPUB_RESULT_MAP, QMTPUB_RESULT_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(99, QMTPUB_RESULT_MAP, QMTPUB_RESULT_MAP_SIZE));
}

static void test_qmtpub_qos_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING(
      "At most once", enum_to_str(QMTPUB_QOS_AT_MOST_ONCE, QMTPUB_QOS_MAP, QMTPUB_QOS_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "At least once", enum_to_str(QMTPUB_QOS_AT_LEAST_ONCE, QMTPUB_QOS_MAP, QMTPUB_QOS_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Exactly once", enum_to_str(QMTPUB_QOS_EXACTLY_ONCE, QMTPUB_QOS_MAP, QMTPUB_QOS_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(99, QMTPUB_QOS_MAP, QMTPUB_QOS_MAP_SIZE));
}

static void test_qmtpub_retain_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING(
      "Do not retain",
      enum_to_str(QMTPUB_RETAIN_DISABLED, QMTPUB_RETAIN_MAP, QMTPUB_RETAIN_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Retain message",
      enum_to_str(QMTPUB_RETAIN_ENABLED, QMTPUB_RETAIN_MAP, QMTPUB_RETAIN_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(99, QMTPUB_RETAIN_MAP, QMTPUB_RETAIN_MAP_SIZE));
}

// ===== Test Write Command Formatter =====

static void test_qmtpub_write_formatter_valid_params(void)
{
  char                  buffer[64] = {0};
  qmtpub_write_params_t params     = {.client_idx = 0,
                                      .msgid      = 1,
                                      .qos        = QMTPUB_QOS_AT_MOST_ONCE,
                                      .retain     = QMTPUB_RETAIN_DISABLED,
                                      .msglen     = 10};
  strcpy(params.topic, "test/topic");

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=0,1,0,0,\"test/topic\",10", buffer);
}

static void test_qmtpub_write_formatter_with_qos1_retained(void)
{
  char                  buffer[64] = {0};
  qmtpub_write_params_t params     = {.client_idx = 2,
                                      .msgid      = 42,
                                      .qos        = QMTPUB_QOS_AT_LEAST_ONCE,
                                      .retain     = QMTPUB_RETAIN_ENABLED,
                                      .msglen     = 15};
  strcpy(params.topic, "device/status");

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=2,42,1,1,\"device/status\",15", buffer);
}

static void test_qmtpub_write_formatter_with_qos2(void)
{
  char                  buffer[64] = {0};
  qmtpub_write_params_t params     = {.client_idx = 5,
                                      .msgid      = 65535, // Maximum message ID
                                      .qos        = QMTPUB_QOS_EXACTLY_ONCE,
                                      .retain     = QMTPUB_RETAIN_DISABLED,
                                      .msglen     = 100};
  strcpy(params.topic, "important/data");

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=5,65535,2,0,\"important/data\",100", buffer);
}

static void test_qmtpub_write_formatter_edge_case_params(void)
{
  char buffer[64] = {0};

  // Test with minimum valid values
  qmtpub_write_params_t params_min = {
      .client_idx = QMTPUB_CLIENT_IDX_MIN,
      .msgid      = QMTPUB_MSGID_MIN,
      .qos        = QMTPUB_QOS_AT_MOST_ONCE,
      .retain     = QMTPUB_RETAIN_DISABLED,
      .msglen     = 1 // Minimum length
  };
  strcpy(params_min.topic, "a"); // Minimum topic

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params_min, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=0,0,0,0,\"a\",1", buffer);

  // Test with maximum valid client_idx
  qmtpub_write_params_t params_max_client = {.client_idx = QMTPUB_CLIENT_IDX_MAX,
                                             .msgid      = 1,
                                             .qos        = QMTPUB_QOS_AT_MOST_ONCE,
                                             .retain     = QMTPUB_RETAIN_DISABLED,
                                             .msglen     = 10};
  strcpy(params_max_client.topic, "test/topic");

  memset(buffer, 0, sizeof(buffer));
  err = AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(
      &params_max_client, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=5,1,0,0,\"test/topic\",10", buffer);
}

static void test_qmtpub_write_formatter_invalid_client_idx(void)
{
  char                  buffer[64] = {0};
  qmtpub_write_params_t params     = {.client_idx = QMTPUB_CLIENT_IDX_MAX + 1, // Invalid: too high
                                      .msgid      = 1,
                                      .qos        = QMTPUB_QOS_AT_MOST_ONCE,
                                      .retain     = QMTPUB_RETAIN_DISABLED,
                                      .msglen     = 10};
  strcpy(params.topic, "test/topic");

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtpub_write_formatter_invalid_qos(void)
{
  char                  buffer[64] = {0};
  qmtpub_write_params_t params     = {.client_idx = 0,
                                      .msgid      = 1,
                                      .qos        = QMTPUB_QOS_EXACTLY_ONCE + 1, // Invalid QoS
                                      .retain     = QMTPUB_RETAIN_DISABLED,
                                      .msglen     = 10};
  strcpy(params.topic, "test/topic");

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtpub_write_formatter_invalid_retain(void)
{
  char                  buffer[64] = {0};
  qmtpub_write_params_t params     = {.client_idx = 0,
                                      .msgid      = 1,
                                      .qos        = QMTPUB_QOS_AT_MOST_ONCE,
                                      .retain     = QMTPUB_RETAIN_ENABLED + 1, // Invalid retain flag
                                      .msglen     = 10};
  strcpy(params.topic, "test/topic");

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtpub_write_formatter_empty_topic(void)
{
  char                  buffer[64] = {0};
  qmtpub_write_params_t params     = {.client_idx = 0,
                                      .msgid      = 1,
                                      .qos        = QMTPUB_QOS_AT_MOST_ONCE,
                                      .retain     = QMTPUB_RETAIN_DISABLED,
                                      .topic      = "", // Empty topic
                                      .msglen     = 10};

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtpub_write_formatter_invalid_msglen(void)
{
  char                  buffer[64] = {0};
  qmtpub_write_params_t params     = {
          .client_idx = 0,
          .msgid      = 1,
          .qos        = QMTPUB_QOS_AT_MOST_ONCE,
          .retain     = QMTPUB_RETAIN_DISABLED,
          .msglen     = 0 // Invalid: too small
  };
  strcpy(params.topic, "test/topic");

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  // Test with msglen too large
  params.msglen = QMTPUB_MSG_MAX_LEN + 1; // Invalid: too large
  err = AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtpub_write_formatter_buffer_too_small(void)
{
  char                  buffer[5] = {0}; // Too small to hold the result
  qmtpub_write_params_t params    = {.client_idx = 0,
                                     .msgid      = 1,
                                     .qos        = QMTPUB_QOS_AT_MOST_ONCE,
                                     .retain     = QMTPUB_RETAIN_DISABLED,
                                     .msglen     = 10};
  strcpy(params.topic, "test/topic");

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
  TEST_ASSERT_EQUAL_STRING("", buffer); // Buffer should be empty
}

static void test_qmtpub_write_formatter_null_args(void)
{
  char                  buffer[64] = {0};
  qmtpub_write_params_t params     = {.client_idx = 0,
                                      .msgid      = 1,
                                      .qos        = QMTPUB_QOS_AT_MOST_ONCE,
                                      .retain     = QMTPUB_RETAIN_DISABLED,
                                      .msglen     = 10};
  strcpy(params.topic, "test/topic");

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(NULL, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, NULL, sizeof(buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, 0);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Write Command Parser =====

static void test_qmtpub_write_parser_ok_only_response(void)
{
  qmtpub_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTPUB_WRITE_OK_RESPONSE, &response);

  // Should not fail, but also not populate any response data yet
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx);
  TEST_ASSERT_FALSE(response.present.has_msgid);
  TEST_ASSERT_FALSE(response.present.has_result);
  TEST_ASSERT_FALSE(response.present.has_value);
}

static void test_qmtpub_write_parser_success_response(void)
{
  qmtpub_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTPUB_WRITE_URC_SUCCESS, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(0, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_msgid);
  TEST_ASSERT_EQUAL(1, response.msgid);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTPUB_RESULT_SUCCESS, response.result);
  TEST_ASSERT_FALSE(response.present.has_value);
}

static void test_qmtpub_write_parser_retransmission_response(void)
{
  qmtpub_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].parser(
      VALID_QMTPUB_WRITE_URC_RETRANSMISSION, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(2, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_msgid);
  TEST_ASSERT_EQUAL(10, response.msgid);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTPUB_RESULT_RETRANSMISSION, response.result);
  TEST_ASSERT_TRUE(response.present.has_value);
  TEST_ASSERT_EQUAL(3, response.value); // 3 retransmissions
}

static void test_qmtpub_write_parser_failure_response(void)
{
  qmtpub_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTPUB_WRITE_URC_FAILURE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(5, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_msgid);
  TEST_ASSERT_EQUAL(100, response.msgid);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTPUB_RESULT_FAILED_TO_SEND, response.result);
  TEST_ASSERT_FALSE(response.present.has_value);
}

static void test_qmtpub_write_parser_invalid_response(void)
{
  qmtpub_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].parser(INVALID_QMTPUB_WRITE_RESPONSE, &response);

  // Should not populate fields with invalid values
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx);
  TEST_ASSERT_FALSE(response.present.has_msgid);
  TEST_ASSERT_FALSE(response.present.has_result);
  TEST_ASSERT_FALSE(response.present.has_value);
}

static void test_qmtpub_write_parser_malformed_response(void)
{
  qmtpub_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].parser(MALFORMED_QMTPUB_WRITE_RESPONSE, &response);

  // Should return invalid response error
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
}

static void test_qmtpub_write_parser_null_args(void)
{
  qmtpub_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTPUB_WRITE_URC_SUCCESS, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Command Definition =====

static void test_qmtpub_command_definition(void)
{
  TEST_ASSERT_EQUAL_STRING("QMTPUB", AT_CMD_QMTPUB.name);
  TEST_ASSERT_EQUAL_STRING("Publish Messages to MQTT Server", AT_CMD_QMTPUB.description);

  // Test command should be not implemented
  TEST_ASSERT_NULL(AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_TEST].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_TEST].formatter);

  // Read command should not exist
  TEST_ASSERT_NULL(AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_READ].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_READ].formatter);

  // Write command should have both parser and formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].parser);
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].formatter);
  TEST_ASSERT_EQUAL(AT_CMD_RESPONSE_TYPE_DATA_REQUIRED,
                    AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].response_type);

  // Execute command should not exist
  TEST_ASSERT_NULL(AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_EXECUTE].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_EXECUTE].formatter);

  // Timeout should be 15000ms as per specification
  TEST_ASSERT_EQUAL(15000, AT_CMD_QMTPUB.timeout_ms);
}

// ===== Integration with AT command parser =====

static void test_qmtpub_at_parser_integration(void)
{
  at_parsed_response_t    base_response   = {0};
  qmtpub_write_response_t qmtpub_response = {0};

  // Parse base response
  esp_err_t err = at_cmd_parse_response(VALID_QMTPUB_WRITE_URC_SUCCESS, &base_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(base_response.has_basic_response);
  TEST_ASSERT_TRUE(base_response.basic_response_is_ok);
  TEST_ASSERT_TRUE(base_response.has_data_response);

  // Parse command-specific data
  err = AT_CMD_QMTPUB.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTPUB_WRITE_URC_SUCCESS,
                                                          &qmtpub_response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(qmtpub_response.present.has_client_idx);
  TEST_ASSERT_EQUAL(0, qmtpub_response.client_idx);
  TEST_ASSERT_TRUE(qmtpub_response.present.has_msgid);
  TEST_ASSERT_EQUAL(1, qmtpub_response.msgid);
  TEST_ASSERT_TRUE(qmtpub_response.present.has_result);
  TEST_ASSERT_EQUAL(QMTPUB_RESULT_SUCCESS, qmtpub_response.result);
}

// ===== Run All Tests =====

void run_test_at_cmd_qmtpub_all(void)
{
  UNITY_BEGIN();

  // Enum mapping tests
  RUN_TEST(test_qmtpub_result_to_str);
  RUN_TEST(test_qmtpub_qos_to_str);
  RUN_TEST(test_qmtpub_retain_to_str);

  // Write command formatter tests
  RUN_TEST(test_qmtpub_write_formatter_valid_params);
  RUN_TEST(test_qmtpub_write_formatter_with_qos1_retained);
  RUN_TEST(test_qmtpub_write_formatter_with_qos2);
  RUN_TEST(test_qmtpub_write_formatter_edge_case_params);
  RUN_TEST(test_qmtpub_write_formatter_invalid_client_idx);
  RUN_TEST(test_qmtpub_write_formatter_invalid_qos);
  RUN_TEST(test_qmtpub_write_formatter_invalid_retain);
  RUN_TEST(test_qmtpub_write_formatter_empty_topic);
  RUN_TEST(test_qmtpub_write_formatter_invalid_msglen);
  RUN_TEST(test_qmtpub_write_formatter_buffer_too_small);
  RUN_TEST(test_qmtpub_write_formatter_null_args);

  // Write command parser tests
  RUN_TEST(test_qmtpub_write_parser_ok_only_response);
  RUN_TEST(test_qmtpub_write_parser_success_response);
  RUN_TEST(test_qmtpub_write_parser_retransmission_response);
  RUN_TEST(test_qmtpub_write_parser_failure_response);
  RUN_TEST(test_qmtpub_write_parser_invalid_response);
  RUN_TEST(test_qmtpub_write_parser_malformed_response);
  RUN_TEST(test_qmtpub_write_parser_null_args);

  // Command definition tests
  RUN_TEST(test_qmtpub_command_definition);

  // Integration tests
  RUN_TEST(test_qmtpub_at_parser_integration);

  UNITY_END();
}
