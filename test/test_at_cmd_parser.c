
#include "at_cmd_parser.h"

#include <esp_err.h>
#include <string.h>
#include <unity.h>

static const char* basic_ok_response    = "\r\nOK\r\n";
static const char* basic_error_response = "\r\nERROR\r\n";

// MQTT command responses
// static const char* mqtt_open_test_response =
//     "\r\n+QMTOPEN: (0-5),<host_name>,(0-65535)\r\nOK\r\n";

// Response has OK first, then data
static const char* mqtt_open_cmd_write_response = "\r\nOK\r\n+QMTOPEN: 0,0\r\n";

// Response has OK after data
static const char* mqtt_open_cmd_read_response =
    "\r\n+QMTOPEN: 0,\"iot-as-mqtt.cn-shanghai.aliyuncs.com\",1883\r\nOK\r\n";

static const char* mqtt_conn_cmd_response = "\r\nOK\r\n+QMTCONN: 0,0,0\r\n";

// static const char* mqtt_sub_cmd_response = "\r\nOK\r\n+QMTSUB: 0,1,0,2\r\n";
//
// static const char* mqtt_pub_cmd_response = "\r\nOK\r\n+QMTPUB: 0,0,0\r\n";

static void test_basic_ok_response(void)
{
  at_parsed_response_t parsed = {0};

  esp_err_t err = at_cmd_parse_response(basic_ok_response, &parsed);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(parsed.has_basic_response);
  TEST_ASSERT_TRUE(parsed.basic_response_is_ok);
  TEST_ASSERT_FALSE(parsed.has_data_response);
}

static void test_basic_error_response(void)
{
  at_parsed_response_t parsed = {0};

  esp_err_t err = at_cmd_parse_response(basic_error_response, &parsed);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(parsed.has_basic_response);
  TEST_ASSERT_FALSE(parsed.basic_response_is_ok);
  TEST_ASSERT_FALSE(parsed.has_data_response);
}

static void test_null_response_arg(void)
{
  at_parsed_response_t parsed = {0};
  esp_err_t            err    = at_cmd_parse_response(NULL, &parsed);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_null_parsed_struct_arg(void)
{
  esp_err_t err = at_cmd_parse_response(basic_ok_response, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ----------------- TESTING EACH TYPE OF COMMAND PARSING -----------------------
// ------------------------------------------------------------------------------

static void test_mqtt_open_cmd_write_type_response(void)
{
  at_parsed_response_t parsed = {0};
  esp_err_t            err    = at_cmd_parse_response(mqtt_open_cmd_write_response, &parsed);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(parsed.has_basic_response);
  TEST_ASSERT_TRUE(parsed.basic_response_is_ok);
  TEST_ASSERT_TRUE(parsed.has_data_response);

  // Should find both pieces in correct order
  const char* ok_pos   = strstr(mqtt_open_cmd_write_response, "OK");
  const char* data_pos = strstr(mqtt_open_cmd_write_response, "+QMTOPEN:");
  TEST_ASSERT_NOT_NULL(ok_pos);
  TEST_ASSERT_NOT_NULL(data_pos);
  TEST_ASSERT_TRUE(ok_pos < data_pos); // OK comes before data
}

static void test_mqtt_open_cmd_read_type_response(void)
{
  at_parsed_response_t parsed = {0};
  esp_err_t            err    = at_cmd_parse_response(mqtt_open_cmd_read_response, &parsed);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(parsed.has_basic_response);
  TEST_ASSERT_TRUE(parsed.basic_response_is_ok);
  TEST_ASSERT_TRUE(parsed.has_data_response);
  TEST_ASSERT_NOT_NULL(strstr(parsed.data_response, "iot-as-mqtt.cn-shanghai.aliyuncs.com"));
}

static void test_mqtt_conn_cmd_response(void)
{
  at_parsed_response_t parsed = {0};
  esp_err_t            err    = at_cmd_parse_response(mqtt_conn_cmd_response, &parsed);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(parsed.has_basic_response);
  TEST_ASSERT_TRUE(parsed.basic_response_is_ok);
  TEST_ASSERT_TRUE(parsed.has_data_response);
  TEST_ASSERT_NOT_NULL(strstr(parsed.data_response, "+QMTCONN: 0,0,0"));
}

void run_test_at_cmd_parser_all(void)
{
  UNITY_BEGIN();

  // Basic response tests
  RUN_TEST(test_basic_ok_response);
  RUN_TEST(test_basic_error_response);
  RUN_TEST(test_null_response_arg);
  RUN_TEST(test_null_parsed_struct_arg);

  // MQTT command response tests
  // TODO: test response
  RUN_TEST(test_mqtt_open_cmd_write_type_response);
  RUN_TEST(test_mqtt_open_cmd_read_type_response);
  RUN_TEST(test_mqtt_conn_cmd_response);

  UNITY_END();
}
