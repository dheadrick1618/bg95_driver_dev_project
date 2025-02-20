#include "at_cmd_csq.h"
#include "at_cmd_parser.h"

#include <esp_err.h>
#include <string.h>
#include <unity.h>

static const char* TAG = "TEST_CSQ_AT_CMD";

// Test response examples
static const char* VALID_CSQ_RESPONSE         = "\r\n+CSQ: 24,0\r\nOK\r\n";
static const char* VALID_CSQ_UNKNOWN_RESPONSE = "\r\n+CSQ: 99,99\r\nOK\r\n";
static const char* VALID_CSQ_TEST_RESPONSE    = "\r\n+CSQ: (0-31,99),(0-7,99)\r\nOK\r\n";
static const char* INVALID_CSQ_RESPONSE = "\r\n+CSQ: 32,8\r\nOK\r\n"; // Invalid RSSI and BER values
static const char* MALFORMED_CSQ_RESPONSE = "\r\n+CSQ: 24\r\nOK\r\n"; // Missing BER value

// ===== Test Helper Functions =====

static void test_csq_rssi_to_dbm_regular_values(void)
{
  // Test specific values
  TEST_ASSERT_EQUAL_INT16(-113, csq_rssi_to_dbm(0)); // Min value
  TEST_ASSERT_EQUAL_INT16(-111, csq_rssi_to_dbm(1)); // Special case
  TEST_ASSERT_EQUAL_INT16(-109, csq_rssi_to_dbm(2)); // Start of formula range
  TEST_ASSERT_EQUAL_INT16(-53, csq_rssi_to_dbm(30)); // End of formula range
  TEST_ASSERT_EQUAL_INT16(-51, csq_rssi_to_dbm(31)); // Max value

  // Test formula range (values 2-30)
  for (int i = 2; i <= 30; i++)
  {
    int16_t expected = -109 + ((i - 2) * 2);
    TEST_ASSERT_EQUAL_INT16(expected, csq_rssi_to_dbm(i));
  }
}

static void test_csq_rssi_to_dbm_special_values(void)
{
  // Test unknown/invalid values
  TEST_ASSERT_EQUAL_INT16(0, csq_rssi_to_dbm(99));  // Unknown
  TEST_ASSERT_EQUAL_INT16(0, csq_rssi_to_dbm(32));  // Invalid value
  TEST_ASSERT_EQUAL_INT16(0, csq_rssi_to_dbm(255)); // Invalid value
}

static void test_csq_dbm_to_rssi_regular_values(void)
{
  // Test specific values
  TEST_ASSERT_EQUAL_UINT8(0, csq_dbm_to_rssi(-113)); // Min value
  TEST_ASSERT_EQUAL_UINT8(0, csq_dbm_to_rssi(-114)); // Below min
  TEST_ASSERT_EQUAL_UINT8(1, csq_dbm_to_rssi(-111)); // Special case
  TEST_ASSERT_EQUAL_UINT8(2, csq_dbm_to_rssi(-109)); // Start of formula range
  TEST_ASSERT_EQUAL_UINT8(30, csq_dbm_to_rssi(-53)); // End of formula range
  TEST_ASSERT_EQUAL_UINT8(31, csq_dbm_to_rssi(-51)); // Max value
  TEST_ASSERT_EQUAL_UINT8(31, csq_dbm_to_rssi(-50)); // Above max

  // Test formula range (values -109 to -53 dBm)
  for (int16_t dbm = -109; dbm <= -53; dbm += 2)
  {
    uint8_t expected = 2 + ((dbm + 109) / 2);
    TEST_ASSERT_EQUAL_UINT8(expected, csq_dbm_to_rssi(dbm));
  }
}

static void test_csq_dbm_to_rssi_special_values(void)
{
  // Test special cases
  TEST_ASSERT_EQUAL_UINT8(CSQ_RSSI_UNKNOWN, csq_dbm_to_rssi(-112)); // Between -113 and -111
  TEST_ASSERT_EQUAL_UINT8(CSQ_RSSI_UNKNOWN, csq_dbm_to_rssi(-110)); // Between -111 and -109
  TEST_ASSERT_EQUAL_UINT8(CSQ_RSSI_UNKNOWN, csq_dbm_to_rssi(-52));  // Between -53 and -51
}

static void test_csq_enum_to_str_mapping(void)
{
  // Test BER enum to string mapping
  TEST_ASSERT_EQUAL_STRING("BER < 0.2%", enum_to_str(CSQ_BER_0, CSQ_BER_MAP, CSQ_BER_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("0.2% <= BER < 0.4%",
                           enum_to_str(CSQ_BER_1, CSQ_BER_MAP, CSQ_BER_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("0.4% <= BER < 0.8%",
                           enum_to_str(CSQ_BER_2, CSQ_BER_MAP, CSQ_BER_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("0.8% <= BER < 1.6%",
                           enum_to_str(CSQ_BER_3, CSQ_BER_MAP, CSQ_BER_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("1.6% <= BER < 3.2%",
                           enum_to_str(CSQ_BER_4, CSQ_BER_MAP, CSQ_BER_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("3.2% <= BER < 6.4%",
                           enum_to_str(CSQ_BER_5, CSQ_BER_MAP, CSQ_BER_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("6.4% <= BER < 12.8%",
                           enum_to_str(CSQ_BER_6, CSQ_BER_MAP, CSQ_BER_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("12.8% <= BER", enum_to_str(CSQ_BER_7, CSQ_BER_MAP, CSQ_BER_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("Unknown or not detectable",
                           enum_to_str(CSQ_BER_UNKNOWN, CSQ_BER_MAP, CSQ_BER_MAP_SIZE));

  // Test invalid BER value
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(8, CSQ_BER_MAP, CSQ_BER_MAP_SIZE));
}

// ===== Test Execute Command Parser =====

static void test_csq_execute_parser_valid_response(void)
{
  csq_execute_response_t response = {0};

  esp_err_t err = AT_CMD_CSQ.type_info[AT_CMD_TYPE_EXECUTE].parser(VALID_CSQ_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_UINT8(24, response.rssi);
  TEST_ASSERT_EQUAL_UINT8(0, response.ber);
}

static void test_csq_execute_parser_unknown_values(void)
{
  csq_execute_response_t response = {0};

  esp_err_t err =
      AT_CMD_CSQ.type_info[AT_CMD_TYPE_EXECUTE].parser(VALID_CSQ_UNKNOWN_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_UINT8(99, response.rssi);
  TEST_ASSERT_EQUAL_UINT8(99, response.ber);
}

static void test_csq_execute_parser_invalid_values(void)
{
  csq_execute_response_t response = {0};

  esp_err_t err = AT_CMD_CSQ.type_info[AT_CMD_TYPE_EXECUTE].parser(INVALID_CSQ_RESPONSE, &response);

  // Should succeed but default to 99 for invalid values
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_UINT8(99, response.rssi); // 32 is invalid, should become 99
  TEST_ASSERT_EQUAL_UINT8(99, response.ber);  // 8 is invalid, should become 99
}

static void test_csq_execute_parser_malformed_response(void)
{
  csq_execute_response_t response = {0};

  esp_err_t err =
      AT_CMD_CSQ.type_info[AT_CMD_TYPE_EXECUTE].parser(MALFORMED_CSQ_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
}

static void test_csq_execute_parser_null_args(void)
{
  csq_execute_response_t response = {0};

  esp_err_t err = AT_CMD_CSQ.type_info[AT_CMD_TYPE_EXECUTE].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_CSQ.type_info[AT_CMD_TYPE_EXECUTE].parser(VALID_CSQ_RESPONSE, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Test Command Parser =====

static void test_csq_test_parser_valid_response(void)
{
  csq_test_response_t response = {0};

  esp_err_t err = AT_CMD_CSQ.type_info[AT_CMD_TYPE_TEST].parser(VALID_CSQ_TEST_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_UINT8(0, response.rssi_min);
  TEST_ASSERT_EQUAL_UINT8(31, response.rssi_max);
  TEST_ASSERT_EQUAL_UINT8(0, response.ber_min);
  TEST_ASSERT_EQUAL_UINT8(7, response.ber_max);
  TEST_ASSERT_TRUE(response.rssi_unknown);
  TEST_ASSERT_TRUE(response.ber_unknown);
}

static void test_csq_test_parser_null_args(void)
{
  csq_test_response_t response = {0};

  esp_err_t err = AT_CMD_CSQ.type_info[AT_CMD_TYPE_TEST].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_CSQ.type_info[AT_CMD_TYPE_TEST].parser(VALID_CSQ_TEST_RESPONSE, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Command Definition =====

static void test_csq_command_definition(void)
{
  TEST_ASSERT_EQUAL_STRING("CSQ", AT_CMD_CSQ.name);
  // TEST_ASSERT_EQUAL_STRING("Signal Quality Report", AT_CMD_CSQ.description);
  // TEST_ASSERT_EQUAL(300, AT_CMD_CSQ.timeout_ms);

  // Test command should have parser but no formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_CSQ.type_info[AT_CMD_TYPE_TEST].parser);
  TEST_ASSERT_NULL(AT_CMD_CSQ.type_info[AT_CMD_TYPE_TEST].formatter);

  // Read command should not be implemented
  TEST_ASSERT_NULL(AT_CMD_CSQ.type_info[AT_CMD_TYPE_READ].parser);
  TEST_ASSERT_NULL(AT_CMD_CSQ.type_info[AT_CMD_TYPE_READ].formatter);

  // Write command should not be implemented
  TEST_ASSERT_NULL(AT_CMD_CSQ.type_info[AT_CMD_TYPE_WRITE].parser);
  TEST_ASSERT_NULL(AT_CMD_CSQ.type_info[AT_CMD_TYPE_WRITE].formatter);

  // Execute command should have parser but no formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_CSQ.type_info[AT_CMD_TYPE_EXECUTE].parser);
  TEST_ASSERT_NULL(AT_CMD_CSQ.type_info[AT_CMD_TYPE_EXECUTE].formatter);
}

// ===== Integration with the AT command parser =====

static void test_csq_at_parser_integration(void)
{
  at_parsed_response_t   parsed_base  = {0};
  csq_execute_response_t csq_response = {0};

  // Parse base response
  esp_err_t err = at_cmd_parse_response(VALID_CSQ_RESPONSE, &parsed_base);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(parsed_base.has_basic_response);
  TEST_ASSERT_TRUE(parsed_base.basic_response_is_ok);
  TEST_ASSERT_TRUE(parsed_base.has_data_response);

  // Parse command-specific data response
  err = AT_CMD_CSQ.type_info[AT_CMD_TYPE_EXECUTE].parser(VALID_CSQ_RESPONSE, &csq_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_UINT8(24, csq_response.rssi);
  TEST_ASSERT_EQUAL_UINT8(0, csq_response.ber);
}

void run_test_at_cmd_csq_all(void)
{
  UNITY_BEGIN();

  // Helper function tests
  RUN_TEST(test_csq_rssi_to_dbm_regular_values);
  RUN_TEST(test_csq_rssi_to_dbm_special_values);
  RUN_TEST(test_csq_dbm_to_rssi_regular_values);
  RUN_TEST(test_csq_dbm_to_rssi_special_values);
  RUN_TEST(test_csq_enum_to_str_mapping);

  // Execute command parser tests
  RUN_TEST(test_csq_execute_parser_valid_response);
  RUN_TEST(test_csq_execute_parser_unknown_values);
  RUN_TEST(test_csq_execute_parser_invalid_values);
  RUN_TEST(test_csq_execute_parser_malformed_response);
  RUN_TEST(test_csq_execute_parser_null_args);

  // Test command parser tests
  RUN_TEST(test_csq_test_parser_valid_response);
  RUN_TEST(test_csq_test_parser_null_args);

  // Command definition tests
  RUN_TEST(test_csq_command_definition);

  // Integration tests
  RUN_TEST(test_csq_at_parser_integration);

  UNITY_END();
}
