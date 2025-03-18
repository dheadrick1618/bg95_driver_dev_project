#include "at_cmd_parser.h"
#include "at_cmd_qmtsub.h"

#include <esp_err.h>
#include <string.h>
#include <unity.h>

static const char* TAG = "TEST_QMTSUB_AT_CMD";

// Test response examples
static const char* VALID_QMTSUB_TEST_RESPONSE =
    "\r\n+QMTSUB: (0-5),(1-65535),\"<topic>\",(0-2)\r\nOK\r\n";
static const char* VALID_QMTSUB_WRITE_OK_RESPONSE = "\r\nOK\r\n"; // Initial OK response
static const char* VALID_QMTSUB_WRITE_URC_SUCCESS =
    "\r\nOK\r\n+QMTSUB: 0,10,0,1\r\n"; // URC with success result and QoS level 1
static const char* VALID_QMTSUB_WRITE_URC_RETRANSMISSION =
    "\r\nOK\r\n+QMTSUB: 2,42,1,3\r\n"; // URC with retransmission (3 attempts)
static const char* VALID_QMTSUB_WRITE_URC_FAILURE =
    "\r\nOK\r\n+QMTSUB: 5,100,2\r\n"; // URC with failed to send
static const char* INVALID_QMTSUB_WRITE_RESPONSE =
    "\r\nOK\r\n+QMTSUB: 99,99999,99\r\n"; // Invalid values

// ===== Test Enum String Mapping =====

static void test_qmtsub_result_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING(
      "Packet sent successfully and ACK received",
      enum_to_str(QMTSUB_RESULT_SUCCESS, QMTSUB_RESULT_MAP, QMTSUB_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Packet retransmission",
      enum_to_str(QMTSUB_RESULT_RETRANSMISSION, QMTSUB_RESULT_MAP, QMTSUB_RESULT_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Failed to send a packet",
      enum_to_str(QMTSUB_RESULT_FAILED_TO_SEND, QMTSUB_RESULT_MAP, QMTSUB_RESULT_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(99, QMTSUB_RESULT_MAP, QMTSUB_RESULT_MAP_SIZE));
}

static void test_qmtsub_qos_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING(
      "At most once", enum_to_str(QMTSUB_QOS_AT_MOST_ONCE, QMTSUB_QOS_MAP, QMTSUB_QOS_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "At least once", enum_to_str(QMTSUB_QOS_AT_LEAST_ONCE, QMTSUB_QOS_MAP, QMTSUB_QOS_MAP_SIZE));

  TEST_ASSERT_EQUAL_STRING(
      "Exactly once", enum_to_str(QMTSUB_QOS_EXACTLY_ONCE, QMTSUB_QOS_MAP, QMTSUB_QOS_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(99, QMTSUB_QOS_MAP, QMTSUB_QOS_MAP_SIZE));
}

// ===== Test Test Command Parser =====

static void test_qmtsub_test_parser_valid_response(void)
{
  qmtsub_test_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_TEST].parser(VALID_QMTSUB_TEST_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(0, response.client_idx_min);
  TEST_ASSERT_EQUAL(5, response.client_idx_max);
  TEST_ASSERT_EQUAL(1, response.msgid_min);
  TEST_ASSERT_EQUAL(65535, response.msgid_max);
  TEST_ASSERT_EQUAL(0, response.qos_min);
  TEST_ASSERT_EQUAL(2, response.qos_max);
}

static void test_qmtsub_test_parser_null_args(void)
{
  qmtsub_test_response_t response = {0};

  esp_err_t err = AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_TEST].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_TEST].parser(VALID_QMTSUB_TEST_RESPONSE, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Write Command Formatter =====

static void test_qmtsub_write_formatter_single_topic(void)
{
  char                  buffer[128] = {0};
  qmtsub_write_params_t params      = {
           .client_idx  = 0,
           .msgid       = 10,
           .topic_count = 1,
           .topics      = {{.topic = "test/topic", .qos = QMTSUB_QOS_AT_MOST_ONCE}}};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=0,10,\"test/topic\",0", buffer);
}

static void test_qmtsub_write_formatter_multiple_topics(void)
{
  char                  buffer[256] = {0};
  qmtsub_write_params_t params      = {
           .client_idx  = 2,
           .msgid       = 42,
           .topic_count = 3,
           .topics      = {{.topic = "test/topic1", .qos = QMTSUB_QOS_AT_MOST_ONCE},
                           {.topic = "test/topic2", .qos = QMTSUB_QOS_AT_LEAST_ONCE},
                           {.topic = "test/topic3", .qos = QMTSUB_QOS_EXACTLY_ONCE}}};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=2,42,\"test/topic1\",0,\"test/topic2\",1,\"test/topic3\",2", buffer);
}

static void test_qmtsub_write_formatter_edge_case_params(void)
{
  char buffer[64] = {0};

  // Test with minimum valid values
  qmtsub_write_params_t params_min = {.client_idx  = QMTSUB_CLIENT_IDX_MIN,
                                      .msgid       = QMTSUB_MSGID_MIN,
                                      .topic_count = 1,
                                      .topics      = {{.topic = "a", // Minimum topic
                                                       .qos   = QMTSUB_QOS_AT_MOST_ONCE}}};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params_min, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=0,1,\"a\",0", buffer);

  // Test with maximum valid client_idx
  qmtsub_write_params_t params_max_client = {
      .client_idx  = QMTSUB_CLIENT_IDX_MAX,
      .msgid       = 1000,
      .topic_count = 1,
      .topics      = {{.topic = "test/topic", .qos = QMTSUB_QOS_AT_MOST_ONCE}}};

  memset(buffer, 0, sizeof(buffer));
  err = AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(
      &params_max_client, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=5,1000,\"test/topic\",0", buffer);
}

static void test_qmtsub_write_formatter_invalid_client_idx(void)
{
  char                  buffer[64] = {0};
  qmtsub_write_params_t params     = {
          .client_idx  = QMTSUB_CLIENT_IDX_MAX + 1, // Invalid: too high
          .msgid       = 10,
          .topic_count = 1,
          .topics      = {{.topic = "test/topic", .qos = QMTSUB_QOS_AT_MOST_ONCE}}};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtsub_write_formatter_invalid_msgid(void)
{
  char                  buffer[64] = {0};
  qmtsub_write_params_t params     = {
          .client_idx  = 0,
          .msgid       = 0, // Invalid: too low
          .topic_count = 1,
          .topics      = {{.topic = "test/topic", .qos = QMTSUB_QOS_AT_MOST_ONCE}}};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  // Test with msgid too large
  params.msgid = QMTSUB_MSGID_MAX + 1; // Invalid: too high
  err = AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtsub_write_formatter_invalid_topic_count(void)
{
  char                  buffer[64] = {0};
  qmtsub_write_params_t params     = {.client_idx  = 0,
                                      .msgid       = 10,
                                      .topic_count = 0, // Invalid: no topics
                                      .topics      = {}};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  // Test with too many topics
  params.topic_count = QMTSUB_MAX_TOPICS + 1; // Invalid: too many topics
  err = AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtsub_write_formatter_invalid_qos(void)
{
  char                  buffer[64] = {0};
  qmtsub_write_params_t params     = {.client_idx  = 0,
                                      .msgid       = 10,
                                      .topic_count = 1,
                                      .topics      = {{
                                               .topic = "test/topic",
                                               .qos   = 3 // Invalid QoS (only 0-2 are valid)
                                  }}};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtsub_write_formatter_empty_topic(void)
{
  char                  buffer[64] = {0};
  qmtsub_write_params_t params     = {.client_idx  = 0,
                                      .msgid       = 10,
                                      .topic_count = 1,
                                      .topics      = {{.topic = "", // Empty topic
                                                       .qos   = QMTSUB_QOS_AT_MOST_ONCE}}};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtsub_write_formatter_buffer_too_small(void)
{
  char                  buffer[5] = {0}; // Too small to hold the result
  qmtsub_write_params_t params    = {
         .client_idx  = 0,
         .msgid       = 10,
         .topic_count = 1,
         .topics      = {{.topic = "test/topic", .qos = QMTSUB_QOS_AT_MOST_ONCE}}};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
}

static void test_qmtsub_write_formatter_null_args(void)
{
  char                  buffer[64] = {0};
  qmtsub_write_params_t params     = {
          .client_idx  = 0,
          .msgid       = 10,
          .topic_count = 1,
          .topics      = {{.topic = "test/topic", .qos = QMTSUB_QOS_AT_MOST_ONCE}}};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(NULL, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, NULL, sizeof(buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, 0);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Write Command Parser =====

static void test_qmtsub_write_parser_ok_only_response(void)
{
  qmtsub_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTSUB_WRITE_OK_RESPONSE, &response);

  // Should not fail, but also not populate any response data yet
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx);
  TEST_ASSERT_FALSE(response.present.has_msgid);
  TEST_ASSERT_FALSE(response.present.has_result);
  TEST_ASSERT_FALSE(response.present.has_value);
}

static void test_qmtsub_write_parser_success_response(void)
{
  qmtsub_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTSUB_WRITE_URC_SUCCESS, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(0, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_msgid);
  TEST_ASSERT_EQUAL(10, response.msgid);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTSUB_RESULT_SUCCESS, response.result);
  TEST_ASSERT_TRUE(response.present.has_value);
  TEST_ASSERT_EQUAL(1, response.value); // Granted QoS level
}

static void test_qmtsub_write_parser_retransmission_response(void)
{
  qmtsub_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].parser(
      VALID_QMTSUB_WRITE_URC_RETRANSMISSION, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(2, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_msgid);
  TEST_ASSERT_EQUAL(42, response.msgid);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTSUB_RESULT_RETRANSMISSION, response.result);
  TEST_ASSERT_TRUE(response.present.has_value);
  TEST_ASSERT_EQUAL(3, response.value); // Number of retransmissions
}

static void test_qmtsub_write_parser_failure_response(void)
{
  qmtsub_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTSUB_WRITE_URC_FAILURE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_client_idx);
  TEST_ASSERT_EQUAL(5, response.client_idx);
  TEST_ASSERT_TRUE(response.present.has_msgid);
  TEST_ASSERT_EQUAL(100, response.msgid);
  TEST_ASSERT_TRUE(response.present.has_result);
  TEST_ASSERT_EQUAL(QMTSUB_RESULT_FAILED_TO_SEND, response.result);
  TEST_ASSERT_FALSE(response.present.has_value); // No value in failure case
}

static void test_qmtsub_write_parser_invalid_response(void)
{
  qmtsub_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].parser(INVALID_QMTSUB_WRITE_RESPONSE, &response);

  // Should not populate fields with invalid values
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(response.present.has_client_idx);
  TEST_ASSERT_FALSE(response.present.has_msgid);
  TEST_ASSERT_FALSE(response.present.has_result);
  TEST_ASSERT_FALSE(response.present.has_value);
}

static void test_qmtsub_write_parser_null_args(void)
{
  qmtsub_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTSUB_WRITE_URC_SUCCESS, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Command Definition =====

static void test_qmtsub_command_definition(void)
{
  TEST_ASSERT_EQUAL_STRING("QMTSUB", AT_CMD_QMTSUB.name);
  TEST_ASSERT_EQUAL_STRING("Subscribe to Topics", AT_CMD_QMTSUB.description);

  // Test command should have parser but no formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_TEST].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_TEST].formatter);
  TEST_ASSERT_EQUAL(AT_CMD_RESPONSE_TYPE_DATA_REQUIRED,
                    AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_TEST].response_type);

  // Read command should not exist
  TEST_ASSERT_NULL(AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_READ].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_READ].formatter);

  // Write command should have both parser and formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].parser);
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].formatter);
  TEST_ASSERT_EQUAL(AT_CMD_RESPONSE_TYPE_DATA_REQUIRED,
                    AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_WRITE].response_type);

  // Execute command should not exist
  TEST_ASSERT_NULL(AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_EXECUTE].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTSUB.type_info[AT_CMD_TYPE_EXECUTE].formatter);

  // Timeout should be 15000ms as per specification
  TEST_ASSERT_EQUAL(15000, AT_CMD_QMTSUB.timeout_ms);
}

// ===== Run All Tests =====

void run_test_at_cmd_qmtsub_all(void)
{
  UNITY_BEGIN();

  // Enum mapping tests
  RUN_TEST(test_qmtsub_result_to_str);
  RUN_TEST(test_qmtsub_qos_to_str);

  // Test command parser tests
  RUN_TEST(test_qmtsub_test_parser_valid_response);
  RUN_TEST(test_qmtsub_test_parser_null_args);

  // Write command formatter tests
  RUN_TEST(test_qmtsub_write_formatter_single_topic);
  RUN_TEST(test_qmtsub_write_formatter_multiple_topics);
  RUN_TEST(test_qmtsub_write_formatter_edge_case_params);
  RUN_TEST(test_qmtsub_write_formatter_invalid_client_idx);
  RUN_TEST(test_qmtsub_write_formatter_invalid_msgid);
  RUN_TEST(test_qmtsub_write_formatter_invalid_topic_count);
  RUN_TEST(test_qmtsub_write_formatter_invalid_qos);
  RUN_TEST(test_qmtsub_write_formatter_empty_topic);
  RUN_TEST(test_qmtsub_write_formatter_buffer_too_small);
  RUN_TEST(test_qmtsub_write_formatter_null_args);

  // Write command parser tests
  RUN_TEST(test_qmtsub_write_parser_ok_only_response);
  RUN_TEST(test_qmtsub_write_parser_success_response);
  RUN_TEST(test_qmtsub_write_parser_retransmission_response);
  RUN_TEST(test_qmtsub_write_parser_failure_response);
  RUN_TEST(test_qmtsub_write_parser_invalid_response);
  RUN_TEST(test_qmtsub_write_parser_null_args);

  // Command definition tests
  RUN_TEST(test_qmtsub_command_definition);

  UNITY_END();
}
