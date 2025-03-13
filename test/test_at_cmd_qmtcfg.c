#include "at_cmd_parser.h"
#include "at_cmd_qmtcfg.h"

#include <esp_err.h>
#include <string.h>
#include <unity.h>

static const char* TAG = "TEST_QMTCFG_AT_CMD";

// Test response examples
static const char* VALID_QMTCFG_TEST_RESPONSE =
    "\r\n+QMTCFG: \"version\",(0-5),(3,4)\r\n"
    "+QMTCFG: \"pdpcid\",(0-5),(1-16)\r\n"
    "+QMTCFG: \"ssl\",(0-5),(0,1),(0-5)\r\n"
    "+QMTCFG: \"keepalive\",(0-5),(0-3600)\r\n"
    "+QMTCFG: \"session\",(0-5),(0,1)\r\n"
    "+QMTCFG: \"timeout\",(0-5),(1-60),(0-10),(0,1)\r\n"
    "+QMTCFG: \"will\",(0-5),(0,1),(0-2),(0,1),<will_topic>,<will_message>\r\n"
    "+QMTCFG: \"recv/mode\",(0-5),(0,1),(0,1)\r\n"
    "+QMTCFG: \"aliauth\",(0-5),<product_key>,<device_name>,<device_secret>\r\n"
    "OK\r\n";

static const char* VALID_QMTCFG_VERSION_RESPONSE   = "\r\n+QMTCFG: \"version\",4\r\nOK\r\n";
static const char* VALID_QMTCFG_PDPCID_RESPONSE    = "\r\n+QMTCFG: \"pdpcid\",1\r\nOK\r\n";
static const char* VALID_QMTCFG_SSL_RESPONSE       = "\r\n+QMTCFG: \"ssl\",1,2\r\nOK\r\n";
static const char* VALID_QMTCFG_KEEPALIVE_RESPONSE = "\r\n+QMTCFG: \"keepalive\",120\r\nOK\r\n";
static const char* VALID_QMTCFG_SESSION_RESPONSE   = "\r\n+QMTCFG: \"session\",1\r\nOK\r\n";
static const char* VALID_QMTCFG_TIMEOUT_RESPONSE   = "\r\n+QMTCFG: \"timeout\",5,3,1\r\nOK\r\n";
static const char* VALID_QMTCFG_WILL_RESPONSE =
    "\r\n+QMTCFG: \"will\",1,1,0,\"topic/test\",\"message test\"\r\nOK\r\n";
static const char* VALID_QMTCFG_RECV_MODE_RESPONSE = "\r\n+QMTCFG: \"recv/mode\",0,1\r\nOK\r\n";

static const char* WRITE_ONLY_RESPONSE = "\r\nOK\r\n"; // Response for write-only operations

static const char* INVALID_QMTCFG_RESPONSE = "\r\n+QMTCFG: \"invalid_type\",0\r\nOK\r\n";

// ===== Test Enum String Mapping =====

static void test_qmtcfg_enum_to_str_mappings(void)
{
  // Version mapping
  TEST_ASSERT_EQUAL_STRING(
      "MQTT v3.1",
      enum_to_str(QMTCFG_VERSION_MQTT_3_1, QMTCFG_VERSION_MAP, QMTCFG_VERSION_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING(
      "MQTT v3.1.1",
      enum_to_str(QMTCFG_VERSION_MQTT_3_1_1, QMTCFG_VERSION_MAP, QMTCFG_VERSION_MAP_SIZE));

  // SSL mode mapping
  TEST_ASSERT_EQUAL_STRING(
      "Normal TCP", enum_to_str(QMTCFG_SSL_DISABLE, QMTCFG_SSL_MODE_MAP, QMTCFG_SSL_MODE_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING(
      "SSL TCP", enum_to_str(QMTCFG_SSL_ENABLE, QMTCFG_SSL_MODE_MAP, QMTCFG_SSL_MODE_MAP_SIZE));

  // Clean session mapping
  TEST_ASSERT_EQUAL_STRING("Store subscriptions",
                           enum_to_str(QMTCFG_CLEAN_SESSION_DISABLE,
                                       QMTCFG_CLEAN_SESSION_MAP,
                                       QMTCFG_CLEAN_SESSION_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("Discard information",
                           enum_to_str(QMTCFG_CLEAN_SESSION_ENABLE,
                                       QMTCFG_CLEAN_SESSION_MAP,
                                       QMTCFG_CLEAN_SESSION_MAP_SIZE));

  // Will flag mapping
  TEST_ASSERT_EQUAL_STRING(
      "Ignore",
      enum_to_str(QMTCFG_WILL_FLAG_IGNORE, QMTCFG_WILL_FLAG_MAP, QMTCFG_WILL_FLAG_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING(
      "Require",
      enum_to_str(QMTCFG_WILL_FLAG_REQUIRE, QMTCFG_WILL_FLAG_MAP, QMTCFG_WILL_FLAG_MAP_SIZE));

  // Will QoS mapping
  TEST_ASSERT_EQUAL_STRING(
      "At most once",
      enum_to_str(QMTCFG_WILL_QOS_0, QMTCFG_WILL_QOS_MAP, QMTCFG_WILL_QOS_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING(
      "At least once",
      enum_to_str(QMTCFG_WILL_QOS_1, QMTCFG_WILL_QOS_MAP, QMTCFG_WILL_QOS_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING(
      "Exactly once",
      enum_to_str(QMTCFG_WILL_QOS_2, QMTCFG_WILL_QOS_MAP, QMTCFG_WILL_QOS_MAP_SIZE));

  // Will retain mapping
  TEST_ASSERT_EQUAL_STRING(
      "Don't retain",
      enum_to_str(QMTCFG_WILL_RETAIN_DISABLE, QMTCFG_WILL_RETAIN_MAP, QMTCFG_WILL_RETAIN_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING(
      "Retain",
      enum_to_str(QMTCFG_WILL_RETAIN_ENABLE, QMTCFG_WILL_RETAIN_MAP, QMTCFG_WILL_RETAIN_MAP_SIZE));

  // Message receive mode mapping
  TEST_ASSERT_EQUAL_STRING("Contained in URC",
                           enum_to_str(QMTCFG_MSG_RECV_MODE_CONTAIN_IN_URC,
                                       QMTCFG_MSG_RECV_MODE_MAP,
                                       QMTCFG_MSG_RECV_MODE_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("Not contained in URC",
                           enum_to_str(QMTCFG_MSG_RECV_MODE_NOT_CONTAIN_IN_URC,
                                       QMTCFG_MSG_RECV_MODE_MAP,
                                       QMTCFG_MSG_RECV_MODE_MAP_SIZE));

  // Message length enable mapping
  TEST_ASSERT_EQUAL_STRING("Length not contained",
                           enum_to_str(QMTCFG_MSG_LEN_DISABLE,
                                       QMTCFG_MSG_LEN_ENABLE_MAP,
                                       QMTCFG_MSG_LEN_ENABLE_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("Length contained",
                           enum_to_str(QMTCFG_MSG_LEN_ENABLE,
                                       QMTCFG_MSG_LEN_ENABLE_MAP,
                                       QMTCFG_MSG_LEN_ENABLE_MAP_SIZE));

  // Timeout notice mapping
  TEST_ASSERT_EQUAL_STRING("Do not report",
                           enum_to_str(QMTCFG_TIMEOUT_NOTICE_DISABLE,
                                       QMTCFG_TIMEOUT_NOTICE_MAP,
                                       QMTCFG_TIMEOUT_NOTICE_MAP_SIZE));
  TEST_ASSERT_EQUAL_STRING("Report",
                           enum_to_str(QMTCFG_TIMEOUT_NOTICE_ENABLE,
                                       QMTCFG_TIMEOUT_NOTICE_MAP,
                                       QMTCFG_TIMEOUT_NOTICE_MAP_SIZE));

  // Check invalid enum values
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", enum_to_str(99, QMTCFG_VERSION_MAP, QMTCFG_VERSION_MAP_SIZE));
}

// ===== Test Test Command Parser =====

static void test_qmtcfg_test_parser_valid_response(void)
{
  qmtcfg_test_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_TEST].parser(VALID_QMTCFG_TEST_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(0, response.client_idx_min);
  TEST_ASSERT_EQUAL(5, response.client_idx_max);
  TEST_ASSERT_EQUAL(2, response.num_supported_versions);
  TEST_ASSERT_EQUAL(QMTCFG_VERSION_MQTT_3_1, response.supported_versions[0]);
  TEST_ASSERT_EQUAL(QMTCFG_VERSION_MQTT_3_1_1, response.supported_versions[1]);
  TEST_ASSERT_EQUAL(1, response.pdp_cid_min);
  TEST_ASSERT_EQUAL(16, response.pdp_cid_max);
  TEST_ASSERT_TRUE(response.supports_ssl_modes[QMTCFG_SSL_DISABLE]);
  TEST_ASSERT_TRUE(response.supports_ssl_modes[QMTCFG_SSL_ENABLE]);
  TEST_ASSERT_EQUAL(0, response.ctx_index_min);
  TEST_ASSERT_EQUAL(5, response.ctx_index_max);
  TEST_ASSERT_EQUAL(0, response.keep_alive_min);
  TEST_ASSERT_EQUAL(3600, response.keep_alive_max);
  TEST_ASSERT_TRUE(response.supports_clean_session_modes[QMTCFG_CLEAN_SESSION_DISABLE]);
  TEST_ASSERT_TRUE(response.supports_clean_session_modes[QMTCFG_CLEAN_SESSION_ENABLE]);
  TEST_ASSERT_EQUAL(1, response.pkt_timeout_min);
  TEST_ASSERT_EQUAL(60, response.pkt_timeout_max);
  TEST_ASSERT_EQUAL(0, response.retry_times_min);
  TEST_ASSERT_EQUAL(10, response.retry_times_max);
  TEST_ASSERT_TRUE(response.supports_timeout_notice_modes[QMTCFG_TIMEOUT_NOTICE_DISABLE]);
  TEST_ASSERT_TRUE(response.supports_timeout_notice_modes[QMTCFG_TIMEOUT_NOTICE_ENABLE]);
  TEST_ASSERT_TRUE(response.supports_will_flags[QMTCFG_WILL_FLAG_IGNORE]);
  TEST_ASSERT_TRUE(response.supports_will_flags[QMTCFG_WILL_FLAG_REQUIRE]);
  TEST_ASSERT_TRUE(response.supports_will_qos[QMTCFG_WILL_QOS_0]);
  TEST_ASSERT_TRUE(response.supports_will_qos[QMTCFG_WILL_QOS_1]);
  TEST_ASSERT_TRUE(response.supports_will_qos[QMTCFG_WILL_QOS_2]);
  TEST_ASSERT_TRUE(response.supports_will_retain[QMTCFG_WILL_RETAIN_DISABLE]);
  TEST_ASSERT_TRUE(response.supports_will_retain[QMTCFG_WILL_RETAIN_ENABLE]);
  TEST_ASSERT_TRUE(response.supports_msg_recv_modes[QMTCFG_MSG_RECV_MODE_CONTAIN_IN_URC]);
  TEST_ASSERT_TRUE(response.supports_msg_recv_modes[QMTCFG_MSG_RECV_MODE_NOT_CONTAIN_IN_URC]);
  TEST_ASSERT_TRUE(response.supports_msg_len_modes[QMTCFG_MSG_LEN_DISABLE]);
  TEST_ASSERT_TRUE(response.supports_msg_len_modes[QMTCFG_MSG_LEN_ENABLE]);
}

static void test_qmtcfg_test_parser_null_args(void)
{
  qmtcfg_test_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_TEST].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_TEST].parser(VALID_QMTCFG_TEST_RESPONSE, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Write Command Formatter =====

static void test_qmtcfg_write_formatter_version(void)
{
  char                  buffer[64] = {0};
  qmtcfg_write_params_t params     = {0};

  // Set parameters for version configuration
  params.type                               = QMTCFG_TYPE_VERSION;
  params.params.version.client_idx          = 0;
  params.params.version.version             = QMTCFG_VERSION_MQTT_3_1_1;
  params.params.version.present.has_version = true;

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=\"version\",0,4", buffer); // 4 is MQTT v3.1.1
}

static void test_qmtcfg_write_formatter_version_query(void)
{
  char                  buffer[64] = {0};
  qmtcfg_write_params_t params     = {0};

  // Set parameters for version configuration without specifying version (query)
  params.type                               = QMTCFG_TYPE_VERSION;
  params.params.version.client_idx          = 0;
  params.params.version.present.has_version = false;

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=\"version\",0", buffer);
}

static void test_qmtcfg_write_formatter_pdpcid(void)
{
  char                  buffer[64] = {0};
  qmtcfg_write_params_t params     = {0};

  // Set parameters for PDP context ID configuration
  params.type                              = QMTCFG_TYPE_PDPCID;
  params.params.pdpcid.client_idx          = 1;
  params.params.pdpcid.pdp_cid             = 2;
  params.params.pdpcid.present.has_pdp_cid = true;

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=\"pdpcid\",1,2", buffer);
}

static void test_qmtcfg_write_formatter_ssl(void)
{
  char                  buffer[64] = {0};
  qmtcfg_write_params_t params     = {0};

  // Set parameters for SSL configuration
  params.type                              = QMTCFG_TYPE_SSL;
  params.params.ssl.client_idx             = 2;
  params.params.ssl.ssl_enable             = QMTCFG_SSL_ENABLE;
  params.params.ssl.ctx_index              = 3;
  params.params.ssl.present.has_ssl_enable = true;
  params.params.ssl.present.has_ctx_index  = true;

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=\"ssl\",2,1,3", buffer); // 1 is SSL enabled, 3 is ctx_index
}

static void test_qmtcfg_write_formatter_keepalive(void)
{
  char                  buffer[64] = {0};
  qmtcfg_write_params_t params     = {0};

  // Set parameters for keepalive configuration
  params.type                                         = QMTCFG_TYPE_KEEPALIVE;
  params.params.keepalive.client_idx                  = 3;
  params.params.keepalive.keep_alive_time             = 120;
  params.params.keepalive.present.has_keep_alive_time = true;

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=\"keepalive\",3,120", buffer);
}

static void test_qmtcfg_write_formatter_session(void)
{
  char                  buffer[64] = {0};
  qmtcfg_write_params_t params     = {0};

  // Set parameters for session configuration
  params.type                                     = QMTCFG_TYPE_SESSION;
  params.params.session.client_idx                = 4;
  params.params.session.clean_session             = QMTCFG_CLEAN_SESSION_ENABLE;
  params.params.session.present.has_clean_session = true;

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=\"session\",4,1", buffer); // 1 is clean session enabled
}

static void test_qmtcfg_write_formatter_timeout(void)
{
  char                  buffer[64] = {0};
  qmtcfg_write_params_t params     = {0};

  // Set all timeout parameters
  params.type                                      = QMTCFG_TYPE_TIMEOUT;
  params.params.timeout.client_idx                 = 0;
  params.params.timeout.pkt_timeout                = 5;
  params.params.timeout.retry_times                = 3;
  params.params.timeout.timeout_notice             = QMTCFG_TIMEOUT_NOTICE_ENABLE;
  params.params.timeout.present.has_pkt_timeout    = true;
  params.params.timeout.present.has_retry_times    = true;
  params.params.timeout.present.has_timeout_notice = true;

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=\"timeout\",0,5,3,1", buffer);
}

static void test_qmtcfg_write_formatter_timeout_partial(void)
{
  char                  buffer[64] = {0};
  qmtcfg_write_params_t params     = {0};

  // Set only some timeout parameters
  params.type                                   = QMTCFG_TYPE_TIMEOUT;
  params.params.timeout.client_idx              = 0;
  params.params.timeout.pkt_timeout             = 5;
  params.params.timeout.present.has_pkt_timeout = true;

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=\"timeout\",0,5", buffer);
}

static void test_qmtcfg_write_formatter_will(void)
{
  char                  buffer[128] = {0};
  qmtcfg_write_params_t params      = {0};

  // Set will parameters with all fields
  params.type                    = QMTCFG_TYPE_WILL;
  params.params.will.client_idx  = 1;
  params.params.will.will_flag   = QMTCFG_WILL_FLAG_REQUIRE;
  params.params.will.will_qos    = QMTCFG_WILL_QOS_1;
  params.params.will.will_retain = QMTCFG_WILL_RETAIN_DISABLE;
  strcpy(params.params.will.will_topic, "topic/test");
  strcpy(params.params.will.will_message, "message test");
  params.params.will.present.has_will_flag    = true;
  params.params.will.present.has_will_qos     = true;
  params.params.will.present.has_will_retain  = true;
  params.params.will.present.has_will_topic   = true;
  params.params.will.present.has_will_message = true;

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=\"will\",1,1,1,0,\"topic/test\",\"message test\"", buffer);
}

static void test_qmtcfg_write_formatter_will_flag_only(void)
{
  char                  buffer[64] = {0};
  qmtcfg_write_params_t params     = {0};

  // Set will flag to ignore (no additional parameters needed)
  params.type                              = QMTCFG_TYPE_WILL;
  params.params.will.client_idx            = 1;
  params.params.will.will_flag             = QMTCFG_WILL_FLAG_IGNORE;
  params.params.will.present.has_will_flag = true;

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=\"will\",1,0", buffer);
}

static void test_qmtcfg_write_formatter_will_invalid_params(void)
{
  char                  buffer[128] = {0};
  qmtcfg_write_params_t params      = {0};

  // Set will flag to require but don't provide all required parameters
  params.type                              = QMTCFG_TYPE_WILL;
  params.params.will.client_idx            = 1;
  params.params.will.will_flag             = QMTCFG_WILL_FLAG_REQUIRE;
  params.params.will.present.has_will_flag = true;
  // Missing will_qos, will_retain, etc.

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  // This should fail because all will parameters are required when will_flag is REQUIRE
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtcfg_write_formatter_recv_mode(void)
{
  char                  buffer[64] = {0};
  qmtcfg_write_params_t params     = {0};

  // Set receive mode parameters
  params.type                                        = QMTCFG_TYPE_RECV_MODE;
  params.params.recv_mode.client_idx                 = 2;
  params.params.recv_mode.msg_recv_mode              = QMTCFG_MSG_RECV_MODE_NOT_CONTAIN_IN_URC;
  params.params.recv_mode.msg_len_enable             = QMTCFG_MSG_LEN_ENABLE;
  params.params.recv_mode.present.has_msg_recv_mode  = true;
  params.params.recv_mode.present.has_msg_len_enable = true;

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("=\"recv/mode\",2,1,1", buffer);
}

static void test_qmtcfg_write_formatter_buffer_too_small(void)
{
  char                  buffer[5] = {0}; // Too small for any meaningful command
  qmtcfg_write_params_t params    = {0};

  params.type                               = QMTCFG_TYPE_VERSION;
  params.params.version.client_idx          = 0;
  params.params.version.version             = QMTCFG_VERSION_MQTT_3_1_1;
  params.params.version.present.has_version = true;

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
}

static void test_qmtcfg_write_formatter_invalid_type(void)
{
  char                  buffer[64] = {0};
  qmtcfg_write_params_t params     = {0};

  // Set an invalid configuration type
  params.type = QMTCFG_TYPE_MAX; // This should be beyond valid types

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static void test_qmtcfg_write_formatter_invalid_client_idx(void)
{
  char                  buffer[64] = {0};
  qmtcfg_write_params_t params     = {0};

  // Set an invalid client index (valid range is 0-5)
  params.type                               = QMTCFG_TYPE_VERSION;
  params.params.version.client_idx          = 6; // Invalid
  params.params.version.version             = QMTCFG_VERSION_MQTT_3_1_1;
  params.params.version.present.has_version = true;

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter(&params, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Write Command Parser =====

static void test_qmtcfg_write_parser_version_response(void)
{
  qmtcfg_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTCFG_VERSION_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(QMTCFG_TYPE_VERSION, response.type);
  TEST_ASSERT_TRUE(response.response.version.present.has_version);
  TEST_ASSERT_EQUAL(QMTCFG_VERSION_MQTT_3_1_1, response.response.version.version);
}

static void test_qmtcfg_write_parser_pdpcid_response(void)
{
  qmtcfg_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTCFG_PDPCID_RESPONSE, &response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(QMTCFG_TYPE_PDPCID, response.type);
  TEST_ASSERT_TRUE(response.response.pdpcid.present.has_pdp_cid);
  TEST_ASSERT_EQUAL(1, response.response.pdpcid.pdp_cid);
}

static void test_qmtcfg_write_parser_ssl_response(void)
{
  qmtcfg_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTCFG_SSL_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(QMTCFG_TYPE_SSL, response.type);
  TEST_ASSERT_TRUE(response.response.ssl.present.has_ssl_enable);
  TEST_ASSERT_EQUAL(QMTCFG_SSL_ENABLE, response.response.ssl.ssl_enable);
  TEST_ASSERT_TRUE(response.response.ssl.present.has_ctx_index);
  TEST_ASSERT_EQUAL(2, response.response.ssl.ctx_index);
}

static void test_qmtcfg_write_parser_keepalive_response(void)
{
  qmtcfg_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTCFG_KEEPALIVE_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(QMTCFG_TYPE_KEEPALIVE, response.type);
  TEST_ASSERT_TRUE(response.response.keepalive.present.has_keep_alive_time);
  TEST_ASSERT_EQUAL(120, response.response.keepalive.keep_alive_time);
}

static void test_qmtcfg_write_parser_session_response(void)
{
  qmtcfg_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTCFG_SESSION_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(QMTCFG_TYPE_SESSION, response.type);
  TEST_ASSERT_TRUE(response.response.session.present.has_clean_session);
  TEST_ASSERT_EQUAL(QMTCFG_CLEAN_SESSION_ENABLE, response.response.session.clean_session);
}

static void test_qmtcfg_write_parser_timeout_response(void)
{
  qmtcfg_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTCFG_TIMEOUT_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(QMTCFG_TYPE_TIMEOUT, response.type);
  TEST_ASSERT_TRUE(response.response.timeout.present.has_pkt_timeout);
  TEST_ASSERT_EQUAL(5, response.response.timeout.pkt_timeout);
  TEST_ASSERT_TRUE(response.response.timeout.present.has_retry_times);
  TEST_ASSERT_EQUAL(3, response.response.timeout.retry_times);
  TEST_ASSERT_TRUE(response.response.timeout.present.has_timeout_notice);
  TEST_ASSERT_EQUAL(QMTCFG_TIMEOUT_NOTICE_ENABLE, response.response.timeout.timeout_notice);
}

static void test_qmtcfg_write_parser_will_response(void)
{
  qmtcfg_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTCFG_WILL_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(QMTCFG_TYPE_WILL, response.type);
  TEST_ASSERT_TRUE(response.response.will.present.has_will_flag);
  TEST_ASSERT_EQUAL(QMTCFG_WILL_FLAG_REQUIRE, response.response.will.will_flag);
  TEST_ASSERT_TRUE(response.response.will.present.has_will_qos);
  TEST_ASSERT_EQUAL(QMTCFG_WILL_QOS_1, response.response.will.will_qos);
  TEST_ASSERT_TRUE(response.response.will.present.has_will_retain);
  TEST_ASSERT_EQUAL(QMTCFG_WILL_RETAIN_DISABLE, response.response.will.will_retain);
  TEST_ASSERT_TRUE(response.response.will.present.has_will_topic);
  TEST_ASSERT_EQUAL_STRING("topic/test", response.response.will.will_topic);
  TEST_ASSERT_TRUE(response.response.will.present.has_will_message);
  TEST_ASSERT_EQUAL_STRING("message test", response.response.will.will_message);
}

static void test_qmtcfg_write_parser_recv_mode_response(void)
{
  qmtcfg_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTCFG_RECV_MODE_RESPONSE, &response);

  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(QMTCFG_TYPE_RECV_MODE, response.type);
  TEST_ASSERT_TRUE(response.response.recv_mode.present.has_msg_recv_mode);
  TEST_ASSERT_EQUAL(QMTCFG_MSG_RECV_MODE_CONTAIN_IN_URC, response.response.recv_mode.msg_recv_mode);
  TEST_ASSERT_TRUE(response.response.recv_mode.present.has_msg_len_enable);
  TEST_ASSERT_EQUAL(QMTCFG_MSG_LEN_ENABLE, response.response.recv_mode.msg_len_enable);
}

static void test_qmtcfg_write_parser_ok_only_response(void)
{
  qmtcfg_write_response_t response = {0};

  // Test with a write-only response (only OK, no data)
  esp_err_t err = AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser(WRITE_ONLY_RESPONSE, &response);

  // This should succeed because some write operations don't return data
  TEST_ASSERT_EQUAL(ESP_OK, err);
}

static void test_qmtcfg_write_parser_invalid_type(void)
{
  qmtcfg_write_response_t response = {0};

  esp_err_t err =
      AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser(INVALID_QMTCFG_RESPONSE, &response);

  // Should return invalid response because the type is unknown
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
}

static void test_qmtcfg_write_parser_null_args(void)
{
  qmtcfg_write_response_t response = {0};

  esp_err_t err = AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser(NULL, &response);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

  err = AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTCFG_VERSION_RESPONSE, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// ===== Test Command Definition =====

static void test_qmtcfg_command_definition(void)
{
  TEST_ASSERT_EQUAL_STRING("QMTCFG", AT_CMD_QMTCFG.name);
  TEST_ASSERT_EQUAL_STRING("Configure Optional Parameters of MQTT", AT_CMD_QMTCFG.description);
  TEST_ASSERT_EQUAL(300, AT_CMD_QMTCFG.timeout_ms); // 300ms per spec

  // Test command should have parser but no formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_TEST].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_TEST].formatter);

  // Read command should not be implemented
  TEST_ASSERT_NULL(AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_READ].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_READ].formatter);

  // Write command should have both parser and formatter
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser);
  TEST_ASSERT_NOT_NULL(AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].formatter);

  // Execute command should not be implemented
  TEST_ASSERT_NULL(AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_EXECUTE].parser);
  TEST_ASSERT_NULL(AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_EXECUTE].formatter);
}

// ===== Integration with AT command parser =====

static void test_qmtcfg_at_parser_integration(void)
{
  at_parsed_response_t    base_response   = {0};
  qmtcfg_write_response_t qmtcfg_response = {0};

  // Parse base response
  esp_err_t err = at_cmd_parse_response(VALID_QMTCFG_VERSION_RESPONSE, &base_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(base_response.has_basic_response);
  TEST_ASSERT_TRUE(base_response.basic_response_is_ok);
  TEST_ASSERT_TRUE(base_response.has_data_response);

  // Parse command-specific data
  err = AT_CMD_QMTCFG.type_info[AT_CMD_TYPE_WRITE].parser(VALID_QMTCFG_VERSION_RESPONSE,
                                                          &qmtcfg_response);
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL(QMTCFG_TYPE_VERSION, qmtcfg_response.type);
  TEST_ASSERT_TRUE(qmtcfg_response.response.version.present.has_version);
  TEST_ASSERT_EQUAL(QMTCFG_VERSION_MQTT_3_1_1, qmtcfg_response.response.version.version);
}

// ===== Run All Tests =====

void run_test_at_cmd_qmtcfg_all(void)
{
  UNITY_BEGIN();

  // Enum string mapping tests
  RUN_TEST(test_qmtcfg_enum_to_str_mappings);

  // Test command parser tests
  RUN_TEST(test_qmtcfg_test_parser_valid_response);
  RUN_TEST(test_qmtcfg_test_parser_null_args);

  // Write command formatter tests
  RUN_TEST(test_qmtcfg_write_formatter_version);
  RUN_TEST(test_qmtcfg_write_formatter_version_query);
  RUN_TEST(test_qmtcfg_write_formatter_pdpcid);
  RUN_TEST(test_qmtcfg_write_formatter_ssl);
  RUN_TEST(test_qmtcfg_write_formatter_keepalive);
  RUN_TEST(test_qmtcfg_write_formatter_session);
  RUN_TEST(test_qmtcfg_write_formatter_timeout);
  RUN_TEST(test_qmtcfg_write_formatter_timeout_partial);
  RUN_TEST(test_qmtcfg_write_formatter_will);
  RUN_TEST(test_qmtcfg_write_formatter_will_flag_only);
  RUN_TEST(test_qmtcfg_write_formatter_will_invalid_params);
  RUN_TEST(test_qmtcfg_write_formatter_recv_mode);
  RUN_TEST(test_qmtcfg_write_formatter_buffer_too_small);
  RUN_TEST(test_qmtcfg_write_formatter_invalid_type);
  RUN_TEST(test_qmtcfg_write_formatter_invalid_client_idx);

  // Write command parser tests
  RUN_TEST(test_qmtcfg_write_parser_version_response);
  RUN_TEST(test_qmtcfg_write_parser_pdpcid_response);
  RUN_TEST(test_qmtcfg_write_parser_ssl_response);
  RUN_TEST(test_qmtcfg_write_parser_keepalive_response);
  RUN_TEST(test_qmtcfg_write_parser_session_response);
  RUN_TEST(test_qmtcfg_write_parser_timeout_response);
  RUN_TEST(test_qmtcfg_write_parser_will_response);
  RUN_TEST(test_qmtcfg_write_parser_recv_mode_response);
  RUN_TEST(test_qmtcfg_write_parser_ok_only_response);
  RUN_TEST(test_qmtcfg_write_parser_invalid_type);
  RUN_TEST(test_qmtcfg_write_parser_null_args);

  // Command definition tests
  RUN_TEST(test_qmtcfg_command_definition);

  // Integration tests
  RUN_TEST(test_qmtcfg_at_parser_integration);

  UNITY_END();
}
