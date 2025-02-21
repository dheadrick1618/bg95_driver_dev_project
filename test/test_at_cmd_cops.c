#include "at_cmd_cops.h"
#include "at_cmd_parser.h"

#include <esp_err.h>
#include <string.h>
#include <unity.h>

static const char* TAG = "TEST_COPS_AT_CMD";

// Test response examples
static const char* VALID_AUTO_MODE_RESPONSE   = "\r\n+COPS: 0\r\nOK\r\n";
static const char* VALID_MANUAL_MODE_RESPONSE = "\r\n+COPS: 1,0,\"Operator Name\",0\r\nOK\r\n";
static const char* VALID_SHORT_RESPONSE    = "\r\n+COPS: 0,1,\"OP\"\r\nOK\r\n"; // No ACT parameter
static const char* VALID_NUMERIC_RESPONSE  = "\r\n+COPS: 1,2,\"12345\",8\r\nOK\r\n"; // eMTC
static const char* INVALID_MODE_RESPONSE   = "\r\n+COPS: 5,0,\"Operator\",0\r\nOK\r\n";
static const char* INVALID_FORMAT_RESPONSE = "\r\n+COPS: 0,3,\"Operator\"\r\nOK\r\n";
static const char* INVALID_ACT_RESPONSE    = "\r\n+COPS: 0,0,\"Operator\",3\r\nOK\r\n";
static const char* MALFORMED_RESPONSE      = "\r\n+COPS: \r\nOK\r\n";

// ===== Test Enum Conversions =====

static void test_cops_stat_enum_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING("Unknown",
                           enum_to_str(COPS_STAT_UNKNOWN, COPS_STAT_MAP, COPS_STAT_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING(
      "Operator available",
      enum_to_str(COPS_STAT_OPERATOR_AVAILABLE, COPS_STAT_MAP, COPS_STAT_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING(
      "Current operator",
      enum_to_str(COPS_STAT_CURRENT_OPERATOR, COPS_STAT_MAP, COPS_STAT_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING(
      "Operator forbidden",
      enum_to_str(COPS_STAT_OPERATOR_FORBIDDEN, COPS_STAT_MAP, COPS_STAT_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(99, COPS_STAT_MAP, COPS_STAT_MAP_SIZE));
}

static void test_cops_mode_enum_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING("Automatic mode",
                           enum_to_str(COPS_MODE_AUTO, COPS_MODE_MAP, COPS_MODE_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("Manual operator selection",
                           enum_to_str(COPS_MODE_MANUAL, COPS_MODE_MAP, COPS_MODE_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("Manual deregister from network",
                           enum_to_str(COPS_MODE_DEREGISTER, COPS_MODE_MAP, COPS_MODE_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("Set only format",
                           enum_to_str(COPS_MODE_SET_FORMAT, COPS_MODE_MAP, COPS_MODE_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("Manual/automatic selection",
                           enum_to_str(COPS_MODE_MANUAL_AUTO, COPS_MODE_MAP, COPS_MODE_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(10, COPS_MODE_MAP, COPS_MODE_MAP_SIZE));
}

static void test_cops_format_enum_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING(
      "Long format alphanumeric",
      enum_to_str(COPS_FORMAT_LONG_ALPHA, COPS_FORMAT_MAP, COPS_FORMAT_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING(
      "Short format alphanumeric",
      enum_to_str(COPS_FORMAT_SHORT_ALPHA, COPS_FORMAT_MAP, COPS_FORMAT_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("Numeric",
                           enum_to_str(COPS_FORMAT_NUMERIC, COPS_FORMAT_MAP, COPS_FORMAT_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(5, COPS_FORMAT_MAP, COPS_FORMAT_MAP_SIZE));
}

static void test_cops_act_enum_to_str(void)
{
  TEST_ASSERT_EQUAL_STRING("GSM", enum_to_str(COPS_ACT_GSM, COPS_ACT_MAP, COPS_ACT_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("eMTC", enum_to_str(COPS_ACT_EMTC, COPS_ACT_MAP, COPS_ACT_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("NB-IoT", enum_to_str(COPS_ACT_NB_IOT, COPS_ACT_MAP, COPS_ACT_MAP_SIZE));

  // Test invalid value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(1, COPS_ACT_MAP, COPS_ACT_MAP_SIZE));
}

// ===== Test Read Command Parser =====

static void test_cops_read_parser_auto_mode(void)
{
  cops_read_response_t response = {0};

  esp_err_t err =
      AT_CMD_COPS.type_info[AT_CMD_TYPE_READ].parser(VALID_AUTO_MODE_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_mode);
  TEST_ASSERT_EQUAL(COPS_MODE_AUTO, response.mode);
  TEST_ASSERT_FALSE(response.present.has_format);
  TEST_ASSERT_FALSE(response.present.has_operator);
  TEST_ASSERT_FALSE(response.present.has_act);
}

static void test_cops_read_parser_manual_mode_full(void)
{
  cops_read_response_t response = {0};

  esp_err_t err =
      AT_CMD_COPS.type_info[AT_CMD_TYPE_READ].parser(VALID_MANUAL_MODE_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_mode);
  TEST_ASSERT_EQUAL(COPS_MODE_MANUAL, response.mode);
  TEST_ASSERT_TRUE(response.present.has_format);
  TEST_ASSERT_EQUAL(COPS_FORMAT_LONG_ALPHA, response.format);
  TEST_ASSERT_TRUE(response.present.has_operator);
  TEST_ASSERT_EQUAL_STRING("Operator Name", response.operator_name);
  TEST_ASSERT_TRUE(response.present.has_act);
  TEST_ASSERT_EQUAL(COPS_ACT_GSM, response.act);
}

static void test_cops_read_parser_short_response(void)
{
  cops_read_response_t response = {0};

  esp_err_t err = AT_CMD_COPS.type_info[AT_CMD_TYPE_READ].parser(VALID_SHORT_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_mode);
  TEST_ASSERT_EQUAL(COPS_MODE_AUTO, response.mode);
  TEST_ASSERT_TRUE(response.present.has_format);
  TEST_ASSERT_EQUAL(COPS_FORMAT_SHORT_ALPHA, response.format);
  TEST_ASSERT_TRUE(response.present.has_operator);
  TEST_ASSERT_EQUAL_STRING("OP", response.operator_name);
  TEST_ASSERT_FALSE(response.present.has_act);
}

static void test_cops_read_parser_numeric_emtc(void)
{
  cops_read_response_t response = {0};

  esp_err_t err = AT_CMD_COPS.type_info[AT_CMD_TYPE_READ].parser(VALID_NUMERIC_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_mode);
  TEST_ASSERT_EQUAL(COPS_MODE_MANUAL, response.mode);
  TEST_ASSERT_TRUE(response.present.has_format);
  TEST_ASSERT_EQUAL(COPS_FORMAT_NUMERIC, response.format);
  TEST_ASSERT_TRUE(response.present.has_operator);
  TEST_ASSERT_EQUAL_STRING("12345", response.operator_name);
  TEST_ASSERT_TRUE(response.present.has_act);
  TEST_ASSERT_EQUAL(COPS_ACT_EMTC, response.act);
}

static void test_cops_read_parser_invalid_mode(void)
{
  cops_read_response_t response = {0};

  esp_err_t err = AT_CMD_COPS.type_info[AT_CMD_TYPE_READ].parser(INVALID_MODE_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
}

static void test_cops_read_parser_invalid_format(void)
{
  cops_read_response_t response = {0};

  esp_err_t err =
      AT_CMD_COPS.type_info[AT_CMD_TYPE_READ].parser(INVALID_FORMAT_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_mode);
  TEST_ASSERT_EQUAL(COPS_MODE_AUTO, response.mode);
  TEST_ASSERT_FALSE(response.present.has_format); // Should not accept invalid format
  TEST_ASSERT_TRUE(response.present.has_operator);
  TEST_ASSERT_EQUAL_STRING("Operator", response.operator_name);
  TEST_ASSERT_FALSE(response.present.has_act);
}

static void test_cops_read_parser_invalid_act(void)
{
  cops_read_response_t response = {0};

  esp_err_t err = AT_CMD_COPS.type_info[AT_CMD_TYPE_READ].parser(INVALID_ACT_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.present.has_mode);
  TEST_ASSERT_EQUAL(COPS_MODE_AUTO, response.mode);
  TEST_ASSERT_TRUE(response.present.has_format);
  TEST_ASSERT_EQUAL(COPS_FORMAT_LONG_ALPHA, response.format);
  TEST_ASSERT_TRUE(response.present.has_operator);
  TEST_ASSERT_EQUAL_STRING("Operator", response.operator_name);
  TEST_ASSERT_FALSE(response.present.has_act); // Should not accept invalid ACT
}

static void test_cops_read_parser_malformed_response(void)
{
  cops_read_response_t response = {0};

  esp_err_t err = AT_CMD_COPS.type_info[AT_CMD_TYPE_READ].parser(MALFORMED_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
}

static void test_cops_read_parser_null_args(void)
{
  cops_read_response_t response = {0};

  esp_err_t err = AT_CMD_COPS.type_info[AT_CMD_TYPE_READ].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_COPS.type_info[AT_CMD_TYPE_READ].parser(VALID_AUTO_MODE_RESPONSE, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Integration with AT command parser =====

static void test_cops_at_parser_integration(void)
{
  at_parsed_response_t base_response = {0};
  cops_read_response_t cops_response = {0};

  // Parse base response
  esp_err_t err = at_cmd_parse_response(VALID_MANUAL_MODE_RESPONSE, &base_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(base_response.has_basic_response);
  TEST_ASSERT_TRUE(base_response.basic_response_is_ok);
  TEST_ASSERT_TRUE(base_response.has_data_response);

  // Parse command-specific data
  err = AT_CMD_COPS.type_info[AT_CMD_TYPE_READ].parser(VALID_MANUAL_MODE_RESPONSE, &cops_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(cops_response.present.has_mode);
  TEST_ASSERT_EQUAL(COPS_MODE_MANUAL, cops_response.mode);
  TEST_ASSERT_TRUE(cops_response.present.has_format);
  TEST_ASSERT_EQUAL(COPS_FORMAT_LONG_ALPHA, cops_response.format);
  TEST_ASSERT_TRUE(cops_response.present.has_operator);
  TEST_ASSERT_EQUAL_STRING("Operator Name", cops_response.operator_name);
  TEST_ASSERT_TRUE(cops_response.present.has_act);
  TEST_ASSERT_EQUAL(COPS_ACT_GSM, cops_response.act);
}

// ===== Test command definition =====

static void test_cops_command_definition(void)
{
  TEST_ASSERT_EQUAL_STRING("COPS", AT_CMD_COPS.name);
  // TEST_ASSERT_EQUAL_STRING("Operator Selection", AT_CMD_COPS.description);
  // TEST_ASSERT_EQUAL(180000, AT_CMD_COPS.timeout_ms); // 180 seconds per spec

  // Test command should not be implemented (as specified)
  // TEST_ASSERT_NULL(AT_CMD_COPS.type_info[AT_CMD_TYPE_TEST].parser);
  // TEST_ASSERT_NULL(AT_CMD_COPS.type_info[AT_CMD_TYPE_TEST].formatter);

  // Read command should have parser but no formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_COPS.type_info[AT_CMD_TYPE_READ].parser);
  TEST_ASSERT_NULL(AT_CMD_COPS.type_info[AT_CMD_TYPE_READ].formatter);

  // Write command should not be implemented (as you specified)
  // TEST_ASSERT_NULL(AT_CMD_COPS.type_info[AT_CMD_TYPE_WRITE].parser);
  // TEST_ASSERT_NULL(AT_CMD_COPS.type_info[AT_CMD_TYPE_WRITE].formatter);

  // Execute command should not be implemented
  // TEST_ASSERT_NULL(AT_CMD_COPS.type_info[AT_CMD_TYPE_EXECUTE].parser);
  // TEST_ASSERT_NULL(AT_CMD_COPS.type_info[AT_CMD_TYPE_EXECUTE].formatter);
}

void run_test_at_cmd_cops_all(void)
{
  UNITY_BEGIN();

  // Enum string mapping tests
  RUN_TEST(test_cops_stat_enum_to_str);
  RUN_TEST(test_cops_mode_enum_to_str);
  RUN_TEST(test_cops_format_enum_to_str);
  RUN_TEST(test_cops_act_enum_to_str);

  // Read parser tests
  RUN_TEST(test_cops_read_parser_auto_mode);
  RUN_TEST(test_cops_read_parser_manual_mode_full);
  RUN_TEST(test_cops_read_parser_short_response);
  RUN_TEST(test_cops_read_parser_numeric_emtc);
  RUN_TEST(test_cops_read_parser_invalid_mode);
  RUN_TEST(test_cops_read_parser_invalid_format);
  RUN_TEST(test_cops_read_parser_invalid_act);
  RUN_TEST(test_cops_read_parser_malformed_response);
  RUN_TEST(test_cops_read_parser_null_args);

  // Integration test
  RUN_TEST(test_cops_at_parser_integration);

  // Command definition test
  RUN_TEST(test_cops_command_definition);

  UNITY_END();
}
