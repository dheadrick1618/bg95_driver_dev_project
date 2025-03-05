#include "at_cmd_cgdcont.h"
#include "at_cmd_parser.h"

#include <esp_err.h>
#include <string.h>
#include <unity.h>

static const char* TAG = "TEST_CGDCONT_AT_CMD";

// Test response examples
// static const char* VALID_CGDCONT_TEST_RESPONSE =
//     "\r\n+CGDCONT: (1-15),\"IP\",\"APN\",\"PDP_addr\",(0-2),(0-4),(0)\r\nOK\r\n";

static const char* VALID_CGDCONT_READ_SINGLE_RESPONSE =
    "\r\n+CGDCONT: 1,\"IP\",\"internet\",\"0.0.0.0\",0,0,0\r\nOK\r\n";

static const char* VALID_CGDCONT_READ_MULTIPLE_RESPONSE =
    "\r\n+CGDCONT: 1,\"IP\",\"internet\",\"0.0.0.0\",0,0,0\r\n"
    "+CGDCONT: 2,\"IPV6\",\"ims\",\"\",1,1\r\n"
    "+CGDCONT: 3,\"IPV4V6\",\"custom.apn\",\"10.0.0.1\",2,2,0\r\nOK\r\n";

static const char* INVALID_CGDCONT_READ_RESPONSE =
    "\r\n+CGDCONT: 16,\"UNKNOWN\",\"invalid\",\"addr\",9,9,9\r\nOK\r\n";

static const char* MALFORMED_CGDCONT_RESPONSE = "\r\n+CGDCONT: \r\nOK\r\n";

// ===== Test Enum String Mapping =====

static void test_cgdcont_pdp_type_to_str_valid(void)
{
  const char* str =
      enum_to_str(CGDCONT_PDP_TYPE_IP, CGDCONT_PDP_TYPE_MAP, CGDCONT_PDP_TYPE_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("IP", str);

  str = enum_to_str(CGDCONT_PDP_TYPE_IPV6, CGDCONT_PDP_TYPE_MAP, CGDCONT_PDP_TYPE_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("IPV6", str);

  str = enum_to_str(CGDCONT_PDP_TYPE_IPV4V6, CGDCONT_PDP_TYPE_MAP, CGDCONT_PDP_TYPE_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("IPV4V6", str);

  str = enum_to_str(CGDCONT_PDP_TYPE_PPP, CGDCONT_PDP_TYPE_MAP, CGDCONT_PDP_TYPE_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("PPP", str);

  str = enum_to_str(CGDCONT_PDP_TYPE_NON_IP, CGDCONT_PDP_TYPE_MAP, CGDCONT_PDP_TYPE_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("Non-IP", str);
}

static void test_cgdcont_pdp_type_to_str_invalid(void)
{
  const char* str = enum_to_str(99, CGDCONT_PDP_TYPE_MAP, CGDCONT_PDP_TYPE_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", str);
}

static void test_cgdcont_data_comp_to_str_valid(void)
{
  const char* str =
      enum_to_str(CGDCONT_DATA_COMP_OFF, CGDCONT_DATA_COMP_MAP, CGDCONT_DATA_COMP_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("OFF", str);

  str = enum_to_str(CGDCONT_DATA_COMP_ON, CGDCONT_DATA_COMP_MAP, CGDCONT_DATA_COMP_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("ON", str);

  str = enum_to_str(CGDCONT_DATA_COMP_V42BIS, CGDCONT_DATA_COMP_MAP, CGDCONT_DATA_COMP_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("V.42bis", str);
}

static void test_cgdcont_head_comp_to_str_valid(void)
{
  const char* str =
      enum_to_str(CGDCONT_HEAD_COMP_OFF, CGDCONT_HEAD_COMP_MAP, CGDCONT_HEAD_COMP_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("OFF", str);

  str = enum_to_str(CGDCONT_HEAD_COMP_ON, CGDCONT_HEAD_COMP_MAP, CGDCONT_HEAD_COMP_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("ON", str);

  str = enum_to_str(CGDCONT_HEAD_COMP_RFC3095, CGDCONT_HEAD_COMP_MAP, CGDCONT_HEAD_COMP_MAP_SIZE);
  TEST_ASSERT_EQUAL_STRING("RFC 3095", str);
}

// ===== Test Test Command Parser =====

// static void test_cgdcont_test_parser_valid_response(void)
// {
//   cgdcont_test_response_t response = {0};
//
//   esp_err_t err =
//       AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_TEST].parser(VALID_CGDCONT_TEST_RESPONSE, &response);
//
//   TEST_ASSERT_EQUAL(ESP_OK, err);
//   TEST_ASSERT_EQUAL(1, response.min_cid);
//   TEST_ASSERT_EQUAL(15, response.max_cid);
//   TEST_ASSERT_TRUE(response.supports_pdp_type_ip);
//   TEST_ASSERT_TRUE(response.supports_data_comp[CGDCONT_DATA_COMP_OFF]);
//   TEST_ASSERT_TRUE(response.supports_data_comp[CGDCONT_DATA_COMP_ON]);
//   TEST_ASSERT_TRUE(response.supports_data_comp[CGDCONT_DATA_COMP_V42BIS]);
//   TEST_ASSERT_TRUE(response.supports_head_comp[CGDCONT_HEAD_COMP_OFF]);
//   TEST_ASSERT_TRUE(response.supports_head_comp[CGDCONT_HEAD_COMP_ON]);
//   TEST_ASSERT_TRUE(response.supports_head_comp[CGDCONT_HEAD_COMP_RFC1144]);
//   TEST_ASSERT_TRUE(response.supports_head_comp[CGDCONT_HEAD_COMP_RFC2507]);
//   TEST_ASSERT_TRUE(response.supports_head_comp[CGDCONT_HEAD_COMP_RFC3095]);
//   TEST_ASSERT_TRUE(response.supports_ipv4_addr_alloc[CGDCONT_IPV4_ADDR_ALLOC_NAS]);
// }
//
// static void test_cgdcont_test_parser_null_args(void)
// {
//   cgdcont_test_response_t response = {0};
//
//   esp_err_t err = AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_TEST].parser(NULL, &response);
//   TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
//
//   err = AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_TEST].parser(VALID_CGDCONT_TEST_RESPONSE, NULL);
//   TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
// }
//
// static void test_cgdcont_test_parser_malformed_response(void)
// {
//   cgdcont_test_response_t response = {0};
//   esp_err_t               err =
//       AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_TEST].parser(MALFORMED_CGDCONT_RESPONSE, &response);
//
//   TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
// }

// ===== Test Read Command Parser =====

static void test_cgdcont_read_parser_single_context(void)
{
  cgdcont_read_response_t response = {0};

  esp_err_t err = AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_READ].parser(
      VALID_CGDCONT_READ_SINGLE_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(1, response.num_contexts);

  // Check first context
  TEST_ASSERT_TRUE(response.contexts[0].present.has_cid);
  TEST_ASSERT_EQUAL(1, response.contexts[0].cid);

  TEST_ASSERT_TRUE(response.contexts[0].present.has_pdp_type);
  TEST_ASSERT_EQUAL(CGDCONT_PDP_TYPE_IP, response.contexts[0].pdp_type);

  TEST_ASSERT_TRUE(response.contexts[0].present.has_apn);
  TEST_ASSERT_EQUAL_STRING("internet", response.contexts[0].apn);

  TEST_ASSERT_TRUE(response.contexts[0].present.has_pdp_addr);
  TEST_ASSERT_EQUAL_STRING("0.0.0.0", response.contexts[0].pdp_addr);

  TEST_ASSERT_TRUE(response.contexts[0].present.has_data_comp);
  TEST_ASSERT_EQUAL(CGDCONT_DATA_COMP_OFF, response.contexts[0].data_comp);

  TEST_ASSERT_TRUE(response.contexts[0].present.has_head_comp);
  TEST_ASSERT_EQUAL(CGDCONT_HEAD_COMP_OFF, response.contexts[0].head_comp);

  TEST_ASSERT_TRUE(response.contexts[0].present.has_ipv4_addr_alloc);
  TEST_ASSERT_EQUAL(CGDCONT_IPV4_ADDR_ALLOC_NAS, response.contexts[0].ipv4_addr_alloc);
}

static void test_cgdcont_read_parser_multiple_contexts(void)
{
  cgdcont_read_response_t response = {0};

  esp_err_t err = AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_READ].parser(
      VALID_CGDCONT_READ_MULTIPLE_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(3, response.num_contexts);

  // Check first context
  TEST_ASSERT_EQUAL(1, response.contexts[0].cid);
  TEST_ASSERT_EQUAL(CGDCONT_PDP_TYPE_IP, response.contexts[0].pdp_type);
  TEST_ASSERT_EQUAL_STRING("internet", response.contexts[0].apn);

  // Check second context
  TEST_ASSERT_EQUAL(2, response.contexts[1].cid);
  TEST_ASSERT_EQUAL(CGDCONT_PDP_TYPE_IPV6, response.contexts[1].pdp_type);
  TEST_ASSERT_EQUAL_STRING("ims", response.contexts[1].apn);
  TEST_ASSERT_EQUAL_STRING("", response.contexts[1].pdp_addr);
  TEST_ASSERT_EQUAL(CGDCONT_DATA_COMP_ON, response.contexts[1].data_comp);
  TEST_ASSERT_EQUAL(CGDCONT_HEAD_COMP_ON, response.contexts[1].head_comp);

  // Check third context
  TEST_ASSERT_EQUAL(3, response.contexts[2].cid);
  TEST_ASSERT_EQUAL(CGDCONT_PDP_TYPE_IPV4V6, response.contexts[2].pdp_type);
  TEST_ASSERT_EQUAL_STRING("custom.apn", response.contexts[2].apn);
  TEST_ASSERT_EQUAL_STRING("10.0.0.1", response.contexts[2].pdp_addr);
  TEST_ASSERT_EQUAL(CGDCONT_DATA_COMP_V42BIS, response.contexts[2].data_comp);
  TEST_ASSERT_EQUAL(CGDCONT_HEAD_COMP_RFC1144, response.contexts[2].head_comp);
}

static void test_cgdcont_read_parser_null_args(void)
{
  cgdcont_read_response_t response = {0};

  esp_err_t err = AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_READ].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_READ].parser(VALID_CGDCONT_READ_SINGLE_RESPONSE, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_cgdcont_read_parser_malformed_response(void)
{
  cgdcont_read_response_t response = {0};
  esp_err_t               err =
      AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_READ].parser(MALFORMED_CGDCONT_RESPONSE, &response);

  // This should still return OK but with 0 contexts
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(0, response.num_contexts);
}

// ===== Test Write Command Formatter =====

static void test_cgdcont_write_formatter_reset_context(void)
{
  char                   buffer[64] = {0};
  cgdcont_write_params_t params     = {
          .cid = 1, .present = {0} // No fields present means reset
  };

  esp_err_t err =
      AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=1", buffer);
}

static void test_cgdcont_write_formatter_minimal_context(void)
{
  char                   buffer[64] = {0};
  cgdcont_write_params_t params     = {
          .cid = 1, .pdp_type = CGDCONT_PDP_TYPE_IP, .present = {.has_cid = 1, .has_pdp_type = 1}};

  esp_err_t err =
      AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=1,\"IP\"", buffer);
}

static void test_cgdcont_write_formatter_with_apn(void)
{
  char                   buffer[64] = {0};
  cgdcont_write_params_t params     = {.cid      = 2,
                                       .pdp_type = CGDCONT_PDP_TYPE_IPV6,
                                       .present  = {.has_cid = 1, .has_pdp_type = 1, .has_apn = 1}};
  strcpy(params.apn, "internet");

  esp_err_t err =
      AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=2,\"IPV6\",\"internet\"", buffer);
}

static void test_cgdcont_write_formatter_full_context(void)
{
  char                   buffer[128] = {0};
  cgdcont_write_params_t params      = {.cid             = 3,
                                        .pdp_type        = CGDCONT_PDP_TYPE_IPV4V6,
                                        .data_comp       = CGDCONT_DATA_COMP_ON,
                                        .head_comp       = CGDCONT_HEAD_COMP_RFC3095,
                                        .ipv4_addr_alloc = CGDCONT_IPV4_ADDR_ALLOC_NAS,
                                        .present         = {.has_cid             = 1,
                                                            .has_pdp_type        = 1,
                                                            .has_apn             = 1,
                                                            .has_pdp_addr        = 1,
                                                            .has_data_comp       = 1,
                                                            .has_head_comp       = 1,
                                                            .has_ipv4_addr_alloc = 1}};
  strcpy(params.apn, "custom.apn");
  strcpy(params.pdp_addr, "10.0.0.1");

  esp_err_t err =
      AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=3,\"IPV4V6\",\"custom.apn\",\"10.0.0.1\",1,4,0", buffer);
}

static void test_cgdcont_write_formatter_invalid_cid(void)
{
  char                   buffer[64] = {0};
  cgdcont_write_params_t params     = {.cid      = 16, // Invalid CID (> 15)
                                       .pdp_type = CGDCONT_PDP_TYPE_IP,
                                       .present  = {.has_cid = 1, .has_pdp_type = 1}};

  esp_err_t err =
      AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_cgdcont_write_formatter_buffer_too_small(void)
{
  char                   buffer[5] = {0}; // Too small for any meaningful command
  cgdcont_write_params_t params    = {
         .cid = 1, .pdp_type = CGDCONT_PDP_TYPE_IP, .present = {.has_cid = 1, .has_pdp_type = 1}};

  esp_err_t err =
      AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
}

// ===== Test Command Definition =====

static void test_cgdcont_command_definition(void)
{
  TEST_ASSERT_EQUAL_STRING("CGDCONT", AT_CMD_CGDCONT.name);
  // TEST_ASSERT_EQUAL_STRING("Define PDP Context", AT_CMD_CGDCONT.description);
  // TEST_ASSERT_EQUAL(300, AT_CMD_CGDCONT.timeout_ms);

  // NOTE: Test command type not yet implemented
  //  TEST_ASSERT_NOT_NULL(AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_TEST].parser);
  //  TEST_ASSERT_NULL(AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_TEST].formatter);

  // Read command should have parser but no formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_READ].parser);
  TEST_ASSERT_NULL(AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_READ].formatter);

  // Write command should have formatter but no parser
  TEST_ASSERT_NULL(AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_WRITE].parser);
  TEST_ASSERT_NOT_NULL(AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_WRITE].formatter);

  // Execute command should not be implemented
  TEST_ASSERT_NULL(AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_EXECUTE].parser);
  TEST_ASSERT_NULL(AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_EXECUTE].formatter);
}

// ===== Integration with AT command parser =====

static void test_cgdcont_at_parser_integration(void)
{
  at_parsed_response_t    base_response    = {0};
  cgdcont_read_response_t cgdcont_response = {0};

  // Parse base response
  esp_err_t err = at_cmd_parse_response(VALID_CGDCONT_READ_SINGLE_RESPONSE, &base_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(base_response.has_basic_response);
  TEST_ASSERT_TRUE(base_response.basic_response_is_ok);
  TEST_ASSERT_TRUE(base_response.has_data_response);

  // Parse command-specific data
  err = AT_CMD_CGDCONT.type_info[AT_CMD_TYPE_READ].parser(VALID_CGDCONT_READ_SINGLE_RESPONSE,
                                                          &cgdcont_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(1, cgdcont_response.num_contexts);
  TEST_ASSERT_EQUAL(1, cgdcont_response.contexts[0].cid);
  TEST_ASSERT_EQUAL(CGDCONT_PDP_TYPE_IP, cgdcont_response.contexts[0].pdp_type);
  TEST_ASSERT_EQUAL_STRING("internet", cgdcont_response.contexts[0].apn);
}

void run_test_at_cmd_cgdcont_all(void)
{
  UNITY_BEGIN();

  // Enum mapping tests
  RUN_TEST(test_cgdcont_pdp_type_to_str_valid);
  RUN_TEST(test_cgdcont_pdp_type_to_str_invalid);
  RUN_TEST(test_cgdcont_data_comp_to_str_valid);
  RUN_TEST(test_cgdcont_head_comp_to_str_valid);

  // // Test command parser tests
  // RUN_TEST(test_cgdcont_test_parser_valid_response);
  // RUN_TEST(test_cgdcont_test_parser_null_args);
  // RUN_TEST(test_cgdcont_test_parser_malformed_response);

  // Read command parser tests
  RUN_TEST(test_cgdcont_read_parser_single_context);
  RUN_TEST(test_cgdcont_read_parser_multiple_contexts);
  RUN_TEST(test_cgdcont_read_parser_null_args);
  RUN_TEST(test_cgdcont_read_parser_malformed_response);

  // Write command formatter tests
  RUN_TEST(test_cgdcont_write_formatter_reset_context);
  RUN_TEST(test_cgdcont_write_formatter_minimal_context);
  RUN_TEST(test_cgdcont_write_formatter_with_apn);
  RUN_TEST(test_cgdcont_write_formatter_full_context);
  RUN_TEST(test_cgdcont_write_formatter_invalid_cid);
  RUN_TEST(test_cgdcont_write_formatter_buffer_too_small);

  // Command definition test
  RUN_TEST(test_cgdcont_command_definition);

  // Integration test
  RUN_TEST(test_cgdcont_at_parser_integration);

  UNITY_END();
}
