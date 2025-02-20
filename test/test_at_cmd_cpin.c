#include "at_cmd_cpin.h"

#include <unity.h>

static const char* TAG = "TEST_CPIN_AT_CMD";

// Test response examples
static const char* VALID_CPIN_READY_RESPONSE = "\r\n+CPIN: READY\r\nOK\r\n";
static const char* VALID_CPIN_PIN_RESPONSE   = "\r\n+CPIN: SIM PIN\r\nOK\r\n";
static const char* VALID_CPIN_ERROR_RESPONSE = "\r\nERROR\r\n";
static const char* INVALID_CPIN_RESPONSE     = "\r\n+CPIN: INVALID_STATUS\r\nOK\r\n";
static const char* MALFORMED_CPIN_RESPONSE   = "\r\n+CPIN:\r\nOK\r\n";

// ===== Test Enum String Mapping =====

static void test_cpin_status_to_str_valid(void)
{
  const char* str = enum_to_str(CPIN_STATUS_READY, CPIN_STATUS_MAP, CPIN_STATUS_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("READY", str);

  str = enum_to_str(CPIN_STATUS_SIM_PIN, CPIN_STATUS_MAP, CPIN_STATUS_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("SIM PIN", str);
}

static void test_cpin_status_to_str_invalid(void)
{
  const char* str = enum_to_str(99, // Value much larger than range of cpin enum
                                CPIN_STATUS_MAP,
                                CPIN_STATUS_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", str);
}

static void test_cpin_str_to_status_valid(void)
{
  enum_convert_result_t result = str_to_enum("READY", CPIN_STATUS_MAP, CPIN_STATUS_MAP_SIZE);
  TEST_ASSERT_TRUE(result.is_valid);
  TEST_ASSERT_EQUAL(CPIN_STATUS_READY, result.value);
  TEST_ASSERT_EQUAL(ESP_OK, result.error);
}

static void test_cpin_str_to_status_invalid(void)
{
  enum_convert_result_t result =
      str_to_enum("INVALID_STATUS", CPIN_STATUS_MAP, CPIN_STATUS_MAP_SIZE);
  TEST_ASSERT_FALSE(result.is_valid);
  TEST_ASSERT_EQUAL(-1, result.value);
}

// ===== Test Read Command Parser =====

static void test_cpin_read_parser_ready_status(void)
{
  cpin_read_response_t response = {0};
  esp_err_t            err =
      AT_CMD_CPIN.type_info[AT_CMD_TYPE_READ].parser(VALID_CPIN_READY_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.status_valid);
  TEST_ASSERT_EQUAL(CPIN_STATUS_READY, response.status);
}

static void test_cpin_read_parser_pin_required(void)
{
  cpin_read_response_t response = {0};
  esp_err_t            err =
      AT_CMD_CPIN.type_info[AT_CMD_TYPE_READ].parser(VALID_CPIN_PIN_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(response.status_valid);
  TEST_ASSERT_EQUAL(CPIN_STATUS_SIM_PIN, response.status);
}

static void test_cpin_read_parser_invalid_status(void)
{
  cpin_read_response_t response = {0};
  esp_err_t err = AT_CMD_CPIN.type_info[AT_CMD_TYPE_READ].parser(INVALID_CPIN_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
  TEST_ASSERT_FALSE(response.status_valid);
}

static void test_cpin_read_parser_malformed_response(void)
{
  cpin_read_response_t response = {0};
  esp_err_t            err =
      AT_CMD_CPIN.type_info[AT_CMD_TYPE_READ].parser(MALFORMED_CPIN_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
  TEST_ASSERT_FALSE(response.status_valid);
}

// ===== Test Write Command Formatter =====

static void test_cpin_write_formatter_pin_only(void)
{
  char                buffer[32] = {0};
  cpin_write_params_t params     = {.pin = "1234", .has_new_pin = false};

  esp_err_t err =
      AT_CMD_CPIN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=\"1234\"", buffer);
}

static void test_cpin_write_formatter_with_new_pin(void)
{
  char                buffer[32] = {0};
  cpin_write_params_t params     = {.pin = "1234", .new_pin = "5678", .has_new_pin = true};

  esp_err_t err =
      AT_CMD_CPIN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=\"1234\",\"5678\"", buffer);
}

static void test_cpin_write_formatter_buffer_overflow(void)
{
  char                buffer[4] = {0}; // Too small
  cpin_write_params_t params    = {.pin = "1234", .has_new_pin = false};

  esp_err_t err =
      AT_CMD_CPIN.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_GREATER_THAN(0, err); // ESP_OK is zero ... this would indidcate error response
}

// ===== Test Command Definition =====

static void test_cpin_command_definition(void)
{
  TEST_ASSERT_EQUAL_STRING("CPIN", AT_CMD_CPIN.name);
  // TEST_ASSERT_EQUAL_STRING("Enter PIN", AT_CMD_CPIN.description);
  // TEST_ASSERT_EQUAL(5000, AT_CMD_CPIN.timeout_ms);

  // Test command should have no parser or formatter
  TEST_ASSERT_NULL(AT_CMD_CPIN.type_info[AT_CMD_TYPE_TEST].parser);
  TEST_ASSERT_NULL(AT_CMD_CPIN.type_info[AT_CMD_TYPE_TEST].formatter);

  // Read command should have parser but no formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_CPIN.type_info[AT_CMD_TYPE_READ].parser);
  TEST_ASSERT_NULL(AT_CMD_CPIN.type_info[AT_CMD_TYPE_READ].formatter);

  // Write command should have formatter but no parser
  TEST_ASSERT_NULL(AT_CMD_CPIN.type_info[AT_CMD_TYPE_WRITE].parser);
  TEST_ASSERT_NOT_NULL(AT_CMD_CPIN.type_info[AT_CMD_TYPE_WRITE].formatter);
}

void run_test_at_cmd_cpin_all(void)
{
  UNITY_BEGIN();

  // Enum mapping tests
  RUN_TEST(test_cpin_status_to_str_valid);
  RUN_TEST(test_cpin_status_to_str_invalid);
  RUN_TEST(test_cpin_str_to_status_valid);
  RUN_TEST(test_cpin_str_to_status_invalid);

  // Read parser tests
  RUN_TEST(test_cpin_read_parser_ready_status);
  RUN_TEST(test_cpin_read_parser_pin_required);
  RUN_TEST(test_cpin_read_parser_invalid_status);
  RUN_TEST(test_cpin_read_parser_malformed_response);

  // Write formatter tests
  RUN_TEST(test_cpin_write_formatter_pin_only);
  RUN_TEST(test_cpin_write_formatter_with_new_pin);
  RUN_TEST(test_cpin_write_formatter_buffer_overflow);

  // Command definition tests
  RUN_TEST(test_cpin_command_definition);

  UNITY_END();
}
