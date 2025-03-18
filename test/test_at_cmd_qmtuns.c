#include "at_cmd_parser.h"
#include "at_cmd_qmtuns.h"

#include <esp_err.h>
#include <string.h>
#include <unity.h>

static const char* TAG = "TEST_QMTUNS_AT_CMD";

// Test response examples
static const char* VALID_QMTUNS_TEST_RESPONSE     = "\r\n+QMTUNS: (0-5),(1-65535)\r\nOK\r\n";
static const char* VALID_QMTUNS_WRITE_OK_RESPONSE = "\r\nOK\r\n"; // Initial OK response
static const char* VALID_QMTUNS_WRITE_URC_SUCCESS =
    "\r\nOK\r\n+QMTUNS: 0,42,0\r\n"; // URC with success result
static const char* VALID_QMTUNS_WRITE_URC_RETRANSMISSION =
    "\r\nOK\r\n+QMTUNS: 2,100,1\r\n"; // URC with retransmission
static const char* VALID_QMTUNS_WRITE_URC_FAILURE =
    "\r\nOK\r\n+QMTUNS: 5,1000,2\r\n"; // URC with failed to send
static const char* INVALID_QMTUNS_WRITE_RESPONSE =
    "\r\nOK\r\n+QMTUNS: 99,99999,99\r\n"; // Invalid values
static const char* MALFORMED_QMTUNS_WRITE_RESPONSE =
    "\r\nOK\r\n+QMTUNS: \r\n"; // No data after prefix

// ===== Test Enum String Mapping =====

static void test_qmtuns_result_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING(
      "Packet sent successfully and ACK received",
      enum_to_str(QMTUNS_RESULT_SUCCESS, QMTUNS_RESULT_MAP, QMTUNS_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Packet retransmission",
      enum_to_str(QMTUNS_RESULT_RETRANSMISSION, QMTUNS_RESULT_MAP, QMTUNS_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Failed to send a packet",
      enum_to_str(QMTUNS_RESULT_FAILED_TO_SEND, QMTUNS_RESULT_MAP, QMTUNS_RESULT_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(99, QMTUNS_RESULT_MAP, QMTUNS_RESULT_MAP_SIZE));
}

// ===== Test Test Command Parser =====

static void test_qmtuns_test_parser_valid_response(void)
{
  qmtuns_test_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_TEST].parser(VALID_QMTUNS_TEST_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(0, response.client_idx_min);
  TEST_ASSERT_EQUAL(5, response.client_idx_max);
  TEST_ASSERT_EQUAL(1, response.msgid_min);
  TEST_ASSERT_EQUAL(65535, response.msgid_max);
}

static void test_qmtuns_test_parser_null_args(void)
{
  qmtuns_test_response_t response = {0};

  esp_err_t err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_TEST].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_TEST].parser(VALID_QMTUNS_TEST_RESPONSE, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtuns_test_parser_invalid_response(void)
{
  qmtuns_test_response_t response = {0};

  // Test with a response that doesn't contain QMTUNS data
  esp_err_t err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_TEST].parser("\r\nOK\r\n", &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);

  // Test with malformed QMTUNS response
  err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_TEST].parser("\r\n+QMTUNS: invalid\r\nOK\r\n", &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
}

// ===== Test Write Command Formatter =====

static void test_qmtuns_write_formatter_single_topic(void)
{
  char                  buffer[128] = {0};
  qmtuns_write_params_t params      = {.client_idx = 0, .msgid = 42, .topic_count = 1};
  strcpy(params.topics[0], "test/topic");

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=0,42,\"test/topic\"", buffer);
}

static void test_qmtuns_write_formatter_multiple_topics(void)
{
  char                  buffer[256] = {0};
  qmtuns_write_params_t params      = {.client_idx = 2, .msgid = 100, .topic_count = 3};
  strcpy(params.topics[0], "test/topic1");
  strcpy(params.topics[1], "test/topic2");
  strcpy(params.topics[2], "test/topic3");

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=2,100,\"test/topic1\",\"test/topic2\",\"test/topic3\"", buffer);
}

static void test_qmtuns_write_formatter_edge_case_params(void)
{
  char buffer[64] = {0};

  // Test with minimum valid values
  qmtuns_write_params_t params_min = {
      .client_idx = QMTUNS_CLIENT_IDX_MIN, .msgid = QMTUNS_MSGID_MIN, .topic_count = 1};
  strcpy(params_min.topics[0], "a"); // Minimum topic

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(&params_min, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=0,1,\"a\"", buffer);

  // Test with maximum valid client_idx
  qmtuns_write_params_t params_max_client = {
      .client_idx = QMTUNS_CLIENT_IDX_MAX, .msgid = 1000, .topic_count = 1};
  strcpy(params_max_client.topics[0], "test/topic");

  memset(buffer, 0, sizeof(buffer));
  err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(
      &params_max_client, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=5,1000,\"test/topic\"", buffer);

  // Test with maximum valid msgid
  qmtuns_write_params_t params_max_msgid = {
      .client_idx = 3, .msgid = QMTUNS_MSGID_MAX, .topic_count = 1};
  strcpy(params_max_msgid.topics[0], "test/topic");

  memset(buffer, 0, sizeof(buffer));
  err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(
      &params_max_msgid, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=3,65535,\"test/topic\"", buffer);

  // Test with maximum topics
  qmtuns_write_params_t params_max_topics = {
      .client_idx = 1, .msgid = 500, .topic_count = QMTUNS_MAX_TOPICS};
  for (int i = 0; i < QMTUNS_MAX_TOPICS; i++)
  {
    sprintf(params_max_topics.topics[i], "topic%d", i + 1);
  }

  memset(buffer, 0, sizeof(buffer));
  err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(
      &params_max_topics, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=1,500,\"topic1\",\"topic2\",\"topic3\",\"topic4\",\"topic5\"", buffer);
}

static void test_qmtuns_write_formatter_invalid_client_idx(void)
{
  char                  buffer[64] = {0};
  qmtuns_write_params_t params     = {.client_idx  = QMTUNS_CLIENT_IDX_MAX + 1, // Invalid: too high
                                      .msgid       = 42,
                                      .topic_count = 1};
  strcpy(params.topics[0], "test/topic");

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtuns_write_formatter_invalid_msgid(void)
{
  char                  buffer[64] = {0};
  qmtuns_write_params_t params     = {.client_idx  = 0,
                                      .msgid       = 0, // Invalid: too low
                                      .topic_count = 1};
  strcpy(params.topics[0], "test/topic");

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  // Test with msgid too large
  params.msgid = QMTUNS_MSGID_MAX + 1; // Invalid: too high
  err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtuns_write_formatter_invalid_topic_count(void)
{
  char                  buffer[64] = {0};
  qmtuns_write_params_t params     = {
          .client_idx = 0, .msgid = 42, .topic_count = 0}; // Invalid: no topics

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  // Test with too many topics
  params.topic_count = QMTUNS_MAX_TOPICS + 1; // Invalid: too many topics
  err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtuns_write_formatter_empty_topic(void)
{
  char                  buffer[64] = {0};
  qmtuns_write_params_t params     = {
          .client_idx = 0, .msgid = 42, .topic_count = 1, .topics = {{""}}}; // Empty topic

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtuns_write_formatter_buffer_too_small(void)
{
  char                  buffer[5] = {0}; // Too small to hold the result
  qmtuns_write_params_t params    = {.client_idx = 0, .msgid = 42, .topic_count = 1};
  strcpy(params.topics[0], "test/topic");

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
}

static void test_qmtuns_write_formatter_null_args(void)
{
  char                  buffer[64] = {0};
  qmtuns_write_params_t params     = {.client_idx = 0, .msgid = 42, .topic_count = 1};
  strcpy(params.topics[0], "test/topic");

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(NULL, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(&params, NULL, sizeof(buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, 0);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Write Command Parser =====

static void test_qmtuns_write_parser_ok_only_response(void)
{
  qmtuns_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTUNS_WRITE_OK_RESPONSE, &response);

  // Should not fail, but also not populate any response data yet
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx);
  TEST_ASSERT_FALSE(response.present.has_msgid);
  TEST_ASSERT_FALSE(response.present.has_result);
}

static void test_qmtuns_write_parser_success_response(void)
{
  qmtuns_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTUNS_WRITE_URC_SUCCESS, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(0, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_msgid);
  TEST_ASSERT_EQUAL(42, response.msgid);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTUNS_RESULT_SUCCESS, response.result);
}

static void test_qmtuns_write_parser_retransmission_response(void)
{
  qmtuns_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].parser(
      VALID_QMTUNS_WRITE_URC_RETRANSMISSION, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(2, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_msgid);
  TEST_ASSERT_EQUAL(100, response.msgid);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTUNS_RESULT_RETRANSMISSION, response.result);
}

static void test_qmtuns_write_parser_failure_response(void)
{
  qmtuns_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTUNS_WRITE_URC_FAILURE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(5, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_msgid);
  TEST_ASSERT_EQUAL(1000, response.msgid);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTUNS_RESULT_FAILED_TO_SEND, response.result);
}

static void test_qmtuns_write_parser_invalid_response(void)
{
  qmtuns_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].parser(INVALID_QMTUNS_WRITE_RESPONSE, &response);

  // Should not populate fields with invalid values
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx);
  TEST_ASSERT_FALSE(response.present.has_msgid);
  TEST_ASSERT_FALSE(response.present.has_result);
}

static void test_qmtuns_write_parser_malformed_response(void)
{
  qmtuns_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].parser(MALFORMED_QMTUNS_WRITE_RESPONSE, &response);

  // Should return invalid response error if response is malformed
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
}

static void test_qmtuns_write_parser_null_args(void)
{
  qmtuns_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTUNS_WRITE_URC_SUCCESS, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Command Definition =====

static void test_qmtuns_command_definition(void)
{
  TEST_ASSERT_EQUAL_STRING("QMTUNS", AT_CMD_QMTUNS.name);
  TEST_ASSERT_EQUAL_STRING("Unsubscribe from Topics", AT_CMD_QMTUNS.description);

  // Test command should have parser but no formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_TEST].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_TEST].formatter);
  TEST_ASSERT_EQUAL(AT_CMD_RESPONSE_TYPE_DATA_REQUIRED,
                    AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_TEST].response_type);

  // Read command should not exist
  TEST_ASSERT_NULL(AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_READ].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_READ].formatter);

  // Write command should have both parser and formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].parser);
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].formatter);
  TEST_ASSERT_EQUAL(AT_CMD_RESPONSE_TYPE_DATA_REQUIRED,
                    AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].response_type);

  // Execute command should not exist
  TEST_ASSERT_NULL(AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_EXECUTE].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_EXECUTE].formatter);

  // Timeout should be 15000ms as per specification
  TEST_ASSERT_EQUAL(15000, AT_CMD_QMTUNS.timeout_ms);
}

// ===== Integration with AT command parser =====

static void test_qmtuns_at_parser_integration(void)
{
  at_parsed_response_t    base_response   = {0};
  qmtuns_write_response_t qmtuns_response = {0};

  // Parse base response
  esp_err_t err = at_cmd_parse_response(VALID_QMTUNS_WRITE_URC_SUCCESS, &base_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(base_response.has_basic_response);
  TEST_ASSERT_TRUE(base_response.basic_response_is_ok);
  TEST_ASSERT_TRUE(base_response.has_data_response);

  // Parse command-specific data
  err = AT_CMD_QMTUNS.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTUNS_WRITE_URC_SUCCESS,
                                                          &qmtuns_response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(qmtuns_response.present.has_client_idx);
  TEST_ASSERT_EQUAL(0, qmtuns_response.client_idx);
  TEST_ASSERT_TRUE(qmtuns_response.present.has_msgid);
  TEST_ASSERT_EQUAL(42, qmtuns_response.msgid);
  TEST_ASSERT_TRUE(qmtuns_response.present.has_result);
  TEST_ASSERT_EQUAL(QMTUNS_RESULT_SUCCESS, qmtuns_response.result);
}

// ===== Run All Tests =====

void run_test_at_cmd_qmtuns_all(void)
{
  UNITY_BEGIN();

  // Enum mapping tests
  RUN_TEST(test_qmtuns_result_to_str);

  // Test command parser tests
  RUN_TEST(test_qmtuns_test_parser_valid_response);
  RUN_TEST(test_qmtuns_test_parser_null_args);
  RUN_TEST(test_qmtuns_test_parser_invalid_response);

  // Write command formatter tests
  RUN_TEST(test_qmtuns_write_formatter_single_topic);
  RUN_TEST(test_qmtuns_write_formatter_multiple_topics);
  RUN_TEST(test_qmtuns_write_formatter_edge_case_params);
  RUN_TEST(test_qmtuns_write_formatter_invalid_client_idx);
  RUN_TEST(test_qmtuns_write_formatter_invalid_msgid);
  RUN_TEST(test_qmtuns_write_formatter_invalid_topic_count);
  RUN_TEST(test_qmtuns_write_formatter_empty_topic);
  RUN_TEST(test_qmtuns_write_formatter_buffer_too_small);
  RUN_TEST(test_qmtuns_write_formatter_null_args);

  // Write command parser tests
  RUN_TEST(test_qmtuns_write_parser_ok_only_response);
  RUN_TEST(test_qmtuns_write_parser_success_response);
  RUN_TEST(test_qmtuns_write_parser_retransmission_response);
  RUN_TEST(test_qmtuns_write_parser_failure_response);
  RUN_TEST(test_qmtuns_write_parser_invalid_response);
  RUN_TEST(test_qmtuns_write_parser_malformed_response);
  RUN_TEST(test_qmtuns_write_parser_null_args);

  // Command definition tests
  RUN_TEST(test_qmtuns_command_definition);

  // Integration tests
  RUN_TEST(test_qmtuns_at_parser_integration);

  UNITY_END();
}
