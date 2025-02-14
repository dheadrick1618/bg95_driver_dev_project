
#include "at_cmd_parser.h"

#include <esp_err.h>
#include <unity.h>

static const char* basic_ok_response    = "\r\nOK\r\n";
static const char* basic_error_response = "\r\nERROR\r\n";

static void test_basic_ok_response(void)
{
  at_parsed_response_t parsed = {0};

  esp_err_t err = at_cmd_parse_response(basic_ok_response, &parsed);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(parsed.has_basic_response);
  TEST_ASSERT_TRUE(parsed.basic_response_is_ok);
  TEST_ASSERT_FALSE(parsed.has_data_response);
}

void run_test_at_cmd_parser_all(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_basic_ok_response);
  UNITY_END();
}
