#include "at_cmd_cgpaddr.h"
#include "at_cmd_cops.h"
#include "at_cmd_qmtcfg.h"
#include "at_cmd_qmtclose.h"
#include "at_cmd_qmtconn.h"
#include "at_cmd_qmtdisc.h"
#include "at_cmd_qmtopen.h"
#include "bg95_driver.h"
#include "freertos/projdefs.h"

#include <esp_err.h>
#include <esp_log.h>
#include <stdio.h>
#include <string.h>

static const char* TAG = "Main";

static bg95_uart_interface_t uart = {0};

static bg95_handle_t handle = {0};

static void config_and_init_uart(void)
{
  bg95_uart_config_t uart_config = {.tx_gpio_num = 32, .rx_gpio_num = 33, .port_num = 2};

  esp_err_t err = bg95_uart_interface_init_hw(&uart, uart_config);

  // ESP_LOGI(TAG,
  //          "UART interface check - write: %p, read: %p, context: %p",
  //          uart.write,
  //          uart.read,
  //          uart.context);

  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to init UART: %d", err);
    return;
  }

  if (!uart.write || !uart.read)
  {
    ESP_LOGE(TAG, "UART functions not properly initialized");
    return;
  }

  // LOOPBACK TEST - for verifying hardware UART port and pins working
  // err = bg95_uart_interface_loopback_test(&uart);
  // if (err != ESP_OK) {
  //     ESP_LOGE(TAG, "Loopback test failed - check your UART connections");
  //     return;
  // }
}

static void init_bg95(void)
{

  ESP_LOGI(TAG, "Initializing BG95 driver");
  esp_err_t err = bg95_init(&handle, &uart);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to init driver");
  }
}

// TODO:  Eventually this will be its own fxn in the BG95 API and will handle all these nested AT
// cmd operations
static void connect_to_cell_network_task(void* pvParams)
{
  bg95_handle_t* bg95_handle = (bg95_handle_t*) pvParams;
  esp_err_t      err;
  int            cid             = 1;
  int            mqtt_client_idx = 0;

  for (;;)
  {

    // check if connected already, if it is, then skip network connection attempt
    bool is_pdp_context_active = false;
    err = bg95_is_pdp_context_active(bg95_handle, cid, &is_pdp_context_active);
    if (err != ESP_OK)
    {
      // loop until connected to network
      continue;
    }

    if (is_pdp_context_active == false)
    {
      err = bg95_connect_to_network(bg95_handle);
      if (err != ESP_OK)
      {
        // loop until connected to network
        continue;
      }
    }

    // check if connection is OPEN to mqtt broker already, if it is, then skip mqtt connection OPEN
    // attempt
    qmtopen_read_response_t response_status = {0};
    err = bg95_mqtt_network_open_status(bg95_handle, mqtt_client_idx, &response_status);
    if (err != ESP_OK)
    {
      // loop until successfully able to check mqtt connection status
      // No response means no mqtt connection open, thus we should try and open a connection
      const char*              host_name        = "dummy.hostname.here";
      uint16_t                 port             = 1883; // 1883 is standard port for non SSL MQTT
      qmtopen_write_response_t qmtopen_response = {0};
      err =
          bg95_mqtt_open_network(bg95_handle, mqtt_client_idx, host_name, port, &qmtopen_response);
      if (err != ESP_OK)
      {
        // loop until able to form connection
        continue;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    // Check if mqtt client is CONNECTED already, if it is, then skip client connection attempt

    qmtconn_read_response_t qmtconn_read_response = {0};
    err = bg95_mqtt_query_connection_state(bg95_handle, mqtt_client_idx, &qmtconn_read_response);
    if (err != ESP_OK)
    {
      // loop until able to successfully check connection state
      continue;
    }

    if (qmtconn_read_response.state != QMTCONN_STATE_CONNECTED)
    {
      const char*              mqtt_client_id_str     = "dummy_mqtt_client";
      const char*              mqtt_client_username   = "dummy_mqtt_username";
      const char*              mqtt_client_password   = "dummy_mqtt_password";
      qmtconn_write_response_t qmtconn_write_response = {0};
      err                                             = bg95_mqtt_connect(bg95_handle,
                              mqtt_client_idx,
                              mqtt_client_id_str,
                              mqtt_client_username,
                              mqtt_client_password,
                              &qmtconn_write_response);
    }

    qmtdisc_write_response_t qmtdisc_write_response = {0};
    err = bg95_mqtt_disconnect(bg95_handle, mqtt_client_idx, &qmtdisc_write_response);

    qmtclose_write_response_t qmtclose_response = {0};
    err = bg95_mqtt_close_network(bg95_handle, mqtt_client_idx, &qmtclose_response);

    /////////

    if (err == ESP_OK)
    {
      vTaskDelete(NULL);
    }
    vTaskDelay(1000);

    // // Check SIM card status
    // // -----------------------------------------------------------
    // cpin_status_t sim_card_status;
    // err = bg95_get_sim_card_status(bg95_handle, &sim_card_status);
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to get PIN status: %d", err);
    //   // return;
    // }
    //
    // // Check signal quality (AT+CSQ)
    // // -----------------------------------------------------------
    // int16_t rssi_dbm;
    // err = bg95_get_signal_quality_dbm(bg95_handle, &rssi_dbm);
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to check signal quality: %d", err);
    //   // return;
    // }
    //
    // // Check available operators list (AT+COPS)
    // // -----------------------------------------------------------
    // cops_operator_data_t operator_data;
    // err = bg95_get_current_operator(bg95_handle, &operator_data);
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to get current operator");
    //   // return;
    // }
    //
    // // First check if PDP context is already active
    // // -----------------------------------------------------------
    // uint8_t cid       = 1; // Use context ID 1
    // bool    is_active = false;
    // err               = bg95_is_pdp_context_active(bg95_handle, cid, &is_active);
    //
    // if (err == ESP_OK)
    // {
    //   if (is_active)
    //   {
    //     ESP_LOGI(TAG, "PDP context %d is already active", cid);
    //
    //     // Since context is already active, get its IP address
    //     char ip_address[CGPADDR_ADDRESS_MAX_CHARS];
    //     err = bg95_get_pdp_address_for_cid(bg95_handle, cid, ip_address, sizeof(ip_address));
    //
    //     if (err == ESP_OK)
    //     {
    //       ESP_LOGI(TAG, "PDP context %d has IP address: %s", cid, ip_address);
    //       // Context is active and has IP - we're good to go for network operations
    //     }
    //     else
    //     {
    //       ESP_LOGW(
    //           TAG, "Failed to get IP address for active context %d: %s", cid,
    //           esp_err_to_name(err));
    //     }
    //   }
    //   else
    //   {
    //     ESP_LOGI(TAG, "PDP context %d is not active, proceeding with setup", cid);
    //
    //     // Define PDP context with your carrier's APN (AT+CGDCONT)
    //     // -----------------------------------------------------------
    //     cgdcont_pdp_type_t pdp_type = CGDCONT_PDP_TYPE_IP; // Use IP type
    //     const char*        apn      = "simbase";           // Use 'simbase' as the APN
    //
    //     err = bg95_define_pdp_context(bg95_handle, cid, pdp_type, apn);
    //     if (err != ESP_OK)
    //     {
    //       ESP_LOGE(TAG, "Failed to define PDP context: %d", err);
    //       // Don't return, continue with other operations
    //     }
    //     else
    //     {
    //       ESP_LOGI(TAG, "Successfully defined PDP context with APN: %s", apn);
    //
    //       // SOFT RESET now to implement the defined PDP context and APN
    //       err = bg95_soft_restart(bg95_handle);
    //       if (err != ESP_OK)
    //       {
    //         ESP_LOGE(TAG, "Failed to soft restart BG95 %d", err);
    //         // Don't return, continue with other operations
    //       }
    //       else
    //       {
    //         ESP_LOGI(TAG, "Successfully soft restarted BG95");
    //
    //         // Activate PDP context (AT+CGACT)
    //         // -----------------------------------------------------------
    //         err = bg95_activate_pdp_context(bg95_handle, cid);
    //         if (err != ESP_OK)
    //         {
    //           ESP_LOGE(TAG,
    //                    "Failed to activate PDP context for cid: %d, error: %s",
    //                    cid,
    //                    esp_err_to_name(err));
    //         }
    //         else
    //         {
    //           ESP_LOGI(TAG, "Successfully Activated PDP context for CID: %d", cid);
    //
    //           // Verify IP address assignment (AT+CGPADDR)
    //           // -----------------------------------------------------------
    //           char ip_address[CGPADDR_ADDRESS_MAX_CHARS];
    //           err = bg95_get_pdp_address_for_cid(bg95_handle, cid, ip_address,
    //           sizeof(ip_address));
    //
    //           if (err == ESP_OK)
    //           {
    //             ESP_LOGI(TAG, "PDP context %d assigned IP address: %s", cid, ip_address);
    //             // Now that we have an IP address, we could proceed with network operations
    //           }
    //           else
    //           {
    //             ESP_LOGW(
    //                 TAG, "Failed to get IP address after activation: %s", esp_err_to_name(err));
    //             // Activation seemed successful but no IP address - may need to retry or check
    //             // network
    //           }
    //         }
    //       }
    //     }
    //   }
    // }
    // else
    // {
    //   ESP_LOGE(TAG, "Failed to check PDP context status: %s", esp_err_to_name(err));
    // }

    // qmtcfg_test_response_t config_params = {0};
    // err                                  = bg95_get_mqtt_config_params(bg95_handle,
    // &config_params);

    // err = bg95_mqtt_config_set_pdp_context(bg95_handle, 0, 1);
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to set and query MQTT PDP context: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Create a response structure to hold the query results
    // qmtcfg_write_pdpcid_response_t pdp_response = {0};
    //
    // // Query the PDP context for MQTT client index 0
    // esp_err_t err = bg95_mqtt_config_query_pdp_context(bg95_handle, 0, &pdp_response);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to query MQTT PDP context: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // ESP_LOGI(TAG, "--------- TESTING MQTT SSL CONFIGURATION ---------");
    //
    // // First, query the current SSL configuration
    // qmtcfg_write_ssl_response_t ssl_response = {0};
    // err = bg95_mqtt_config_query_ssl(bg95_handle, 0, &ssl_response);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to query MQTT SSL configuration: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Display current configuration
    // if (ssl_response.present.has_ssl_enable)
    // {
    //   ESP_LOGI(TAG,
    //            "Current MQTT SSL mode: %s",
    //            ssl_response.ssl_enable == QMTCFG_SSL_ENABLE ? "Enabled" : "Disabled");
    //
    //   if (ssl_response.present.has_ctx_index && ssl_response.ssl_enable == QMTCFG_SSL_ENABLE)
    //   {
    //     ESP_LOGI(TAG, "Using SSL context index: %d", ssl_response.ctx_index);
    //   }
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "No SSL configuration found");
    // }
    //
    // // Now set a new SSL configuration
    // // For testing, we'll toggle the current configuration
    // qmtcfg_ssl_mode_t new_ssl_mode =
    //     (ssl_response.present.has_ssl_enable && ssl_response.ssl_enable == QMTCFG_SSL_ENABLE)
    //         ? QMTCFG_SSL_DISABLE
    //         : QMTCFG_SSL_ENABLE;
    //
    // uint8_t new_ctx_index = 0; // Default context index
    // if (ssl_response.present.has_ctx_index)
    // {
    //   // Use a different context index for testing
    //   new_ctx_index = (ssl_response.ctx_index + 1) % 6; // Keep within 0-5 range
    // }
    //
    // ESP_LOGI(TAG,
    //          "Setting new SSL mode: %s, context index: %d",
    //          new_ssl_mode == QMTCFG_SSL_ENABLE ? "Enabled" : "Disabled",
    //          new_ctx_index);
    //
    // err = bg95_mqtt_config_set_ssl(bg95_handle, 0, new_ssl_mode, new_ctx_index);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to set MQTT SSL configuration: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Query again to verify the changes
    // memset(&ssl_response, 0, sizeof(ssl_response));
    // err = bg95_mqtt_config_query_ssl(bg95_handle, 0, &ssl_response);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(
    //       TAG, "Failed to query MQTT SSL configuration after update: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Verify the new configuration
    // if (ssl_response.present.has_ssl_enable)
    // {
    //   ESP_LOGI(TAG,
    //            "Updated MQTT SSL mode: %s",
    //            ssl_response.ssl_enable == QMTCFG_SSL_ENABLE ? "Enabled" : "Disabled");
    //
    //   if (ssl_response.present.has_ctx_index && ssl_response.ssl_enable == QMTCFG_SSL_ENABLE)
    //   {
    //     ESP_LOGI(TAG, "Using SSL context index: %d", ssl_response.ctx_index);
    //   }
    //
    //   // Verify if the new values match what we set
    //   if (ssl_response.ssl_enable == new_ssl_mode)
    //   {
    //     ESP_LOGI(TAG, "SSL mode successfully updated!");
    //   }
    //   else
    //   {
    //     ESP_LOGW(TAG, "SSL mode doesn't match the requested value");
    //   }
    //
    //   if (ssl_response.present.has_ctx_index && ssl_response.ssl_enable == QMTCFG_SSL_ENABLE &&
    //       ssl_response.ctx_index == new_ctx_index)
    //   {
    //     ESP_LOGI(TAG, "SSL context index successfully updated!");
    //   }
    //   else if (ssl_response.ssl_enable == QMTCFG_SSL_ENABLE)
    //   {
    //     ESP_LOGW(TAG, "SSL context index doesn't match the requested value");
    //   }
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "No SSL configuration found after update");
    // }
    //
    // ESP_LOGI(TAG, "--------- MQTT SSL CONFIGURATION TEST COMPLETE ---------");
    //
    // ESP_LOGI(TAG, "--------- TESTING MQTT KEEPALIVE CONFIGURATION ---------");
    //
    // // First, query the current keepalive configuration
    // qmtcfg_write_keepalive_response_t keepalive_response = {0};
    // err = bg95_mqtt_config_query_keepalive(bg95_handle, 0, &keepalive_response);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to query MQTT keepalive configuration: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Display current configuration
    // if (keepalive_response.present.has_keep_alive_time)
    // {
    //   ESP_LOGI(TAG, "Current MQTT keep-alive time: %d seconds",
    //   keepalive_response.keep_alive_time);
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "No keep-alive configuration found");
    // }
    //
    // // Now set a new keepalive configuration
    // // For testing, we'll use a different value than current
    // uint16_t current_time = 0;
    // if (keepalive_response.present.has_keep_alive_time)
    // {
    //   current_time = keepalive_response.keep_alive_time;
    // }
    //
    // uint16_t new_keep_alive_time;
    // if (current_time == 120)
    // {                            // 120 is the default value
    //   new_keep_alive_time = 180; // Set to 3 minutes if it was default
    // }
    // else
    // {
    //   new_keep_alive_time = 120; // Set to default otherwise
    // }
    //
    // ESP_LOGI(TAG, "Setting new keep-alive time: %d seconds", new_keep_alive_time);
    //
    // err = bg95_mqtt_config_set_keepalive(bg95_handle, 0, new_keep_alive_time);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to set MQTT keep-alive time: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Query again to verify the changes
    // memset(&keepalive_response, 0, sizeof(keepalive_response));
    // err = bg95_mqtt_config_query_keepalive(bg95_handle, 0, &keepalive_response);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to query MQTT keep-alive time after update: %s",
    //   esp_err_to_name(err)); return;
    // }
    //
    // // Verify the new configuration
    // if (keepalive_response.present.has_keep_alive_time)
    // {
    //   ESP_LOGI(TAG, "Updated MQTT keep-alive time: %d seconds",
    //   keepalive_response.keep_alive_time);
    //
    //   // Verify if the new values match what we set
    //   if (keepalive_response.keep_alive_time == new_keep_alive_time)
    //   {
    //     ESP_LOGI(TAG, "Keep-alive time successfully updated!");
    //   }
    //   else
    //   {
    //     ESP_LOGW(TAG,
    //              "Keep-alive time doesn't match the requested value. "
    //              "Requested: %d, Got: %d",
    //              new_keep_alive_time,
    //              keepalive_response.keep_alive_time);
    //   }
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "No keep-alive configuration found after update");
    // }
    //
    // ESP_LOGI(TAG, "--------- MQTT KEEPALIVE CONFIGURATION TEST COMPLETE ---------");
    //
    // ESP_LOGI(TAG, "--------- TESTING MQTT TIMEOUT CONFIGURATION ---------");
    //
    // // First, query the current timeout configuration
    // qmtcfg_write_timeout_response_t timeout_response = {0};
    // err = bg95_mqtt_config_query_timeout(bg95_handle, 0, &timeout_response);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to query MQTT timeout configuration: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Display current configuration
    // ESP_LOGI(TAG, "Current MQTT timeout configuration:");
    //
    // uint8_t                 current_pkt_timeout    = 5;                             // Default
    // uint8_t                 current_retry_times    = 3;                             // Default
    // qmtcfg_timeout_notice_t current_timeout_notice = QMTCFG_TIMEOUT_NOTICE_DISABLE; // Default
    //
    // if (timeout_response.present.has_pkt_timeout)
    // {
    //   current_pkt_timeout = timeout_response.pkt_timeout;
    //   ESP_LOGI(TAG, "  Packet timeout: %d seconds", current_pkt_timeout);
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "  Packet timeout information not available");
    // }
    //
    // if (timeout_response.present.has_retry_times)
    // {
    //   current_retry_times = timeout_response.retry_times;
    //   ESP_LOGI(TAG, "  Retry times: %d", current_retry_times);
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "  Retry times information not available");
    // }
    //
    // if (timeout_response.present.has_timeout_notice)
    // {
    //   current_timeout_notice = timeout_response.timeout_notice;
    //   ESP_LOGI(TAG,
    //            "  Timeout notice: %s",
    //            current_timeout_notice == QMTCFG_TIMEOUT_NOTICE_ENABLE ? "Enabled" : "Disabled");
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "  Timeout notice information not available");
    // }
    //
    // // Now set new timeout parameters that are different from current values
    // uint8_t                 new_pkt_timeout = (current_pkt_timeout == 5) ? 10 : 5;
    // uint8_t                 new_retry_times = (current_retry_times == 3) ? 5 : 3;
    // qmtcfg_timeout_notice_t new_timeout_notice =
    //     (current_timeout_notice == QMTCFG_TIMEOUT_NOTICE_ENABLE) ? QMTCFG_TIMEOUT_NOTICE_DISABLE
    //                                                              : QMTCFG_TIMEOUT_NOTICE_ENABLE;
    //
    // ESP_LOGI(TAG, "Setting new timeout parameters:");
    // ESP_LOGI(TAG, "  Packet timeout: %d seconds", new_pkt_timeout);
    // ESP_LOGI(TAG, "  Retry times: %d", new_retry_times);
    // ESP_LOGI(TAG,
    //          "  Timeout notice: %s",
    //          new_timeout_notice == QMTCFG_TIMEOUT_NOTICE_ENABLE ? "Enabled" : "Disabled");
    //
    // err = bg95_mqtt_config_set_timeout(
    //     bg95_handle, 0, new_pkt_timeout, new_retry_times, new_timeout_notice);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to set MQTT timeout parameters: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Query again to verify the changes
    // memset(&timeout_response, 0, sizeof(timeout_response));
    // err = bg95_mqtt_config_query_timeout(bg95_handle, 0, &timeout_response);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(
    //       TAG, "Failed to query MQTT timeout parameters after update: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Verify the new configuration
    // ESP_LOGI(TAG, "Updated MQTT timeout configuration:");
    //
    // bool all_params_match = true;
    //
    // if (timeout_response.present.has_pkt_timeout)
    // {
    //   ESP_LOGI(TAG, "  Packet timeout: %d seconds", timeout_response.pkt_timeout);
    //   if (timeout_response.pkt_timeout != new_pkt_timeout)
    //   {
    //     ESP_LOGW(TAG, "  Packet timeout doesn't match the requested value");
    //     all_params_match = false;
    //   }
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "  Packet timeout information not available after update");
    //   all_params_match = false;
    // }
    //
    // if (timeout_response.present.has_retry_times)
    // {
    //   ESP_LOGI(TAG, "  Retry times: %d", timeout_response.retry_times);
    //   if (timeout_response.retry_times != new_retry_times)
    //   {
    //     ESP_LOGW(TAG, "  Retry times doesn't match the requested value");
    //     all_params_match = false;
    //   }
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "  Retry times information not available after update");
    //   all_params_match = false;
    // }
    //
    // if (timeout_response.present.has_timeout_notice)
    // {
    //   ESP_LOGI(TAG,
    //            "  Timeout notice: %s",
    //            timeout_response.timeout_notice == QMTCFG_TIMEOUT_NOTICE_ENABLE ? "Enabled"
    //                                                                            : "Disabled");
    //   if (timeout_response.timeout_notice != new_timeout_notice)
    //   {
    //     ESP_LOGW(TAG, "  Timeout notice doesn't match the requested value");
    //     all_params_match = false;
    //   }
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "  Timeout notice information not available after update");
    //   all_params_match = false;
    // }
    //
    // if (all_params_match)
    // {
    //   ESP_LOGI(TAG, "All timeout parameters successfully updated!");
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "Some timeout parameters don't match the requested values");
    // }
    //
    // ESP_LOGI(TAG, "--------- MQTT TIMEOUT CONFIGURATION TEST COMPLETE ---------");
    //
    // ESP_LOGI(TAG, "--------- TESTING MQTT SESSION CONFIGURATION ---------");
    //
    // // First, query the current session configuration
    // qmtcfg_write_session_response_t session_response = {0};
    // err = bg95_mqtt_config_query_session(bg95_handle, 0, &session_response);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to query MQTT session configuration: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Display current configuration
    // if (session_response.present.has_clean_session)
    // {
    //   ESP_LOGI(TAG,
    //            "Current MQTT session type: %s",
    //            session_response.clean_session == QMTCFG_CLEAN_SESSION_ENABLE
    //                ? "Clean (discard information)"
    //                : "Persistent (store subscriptions)");
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "No session configuration found");
    // }
    //
    // // Now set a new session configuration
    // // For testing, we'll toggle the current configuration
    // qmtcfg_clean_session_t new_clean_session;
    //
    // if (session_response.present.has_clean_session)
    // {
    //   // Toggle the current setting
    //   new_clean_session = (session_response.clean_session == QMTCFG_CLEAN_SESSION_ENABLE)
    //                           ? QMTCFG_CLEAN_SESSION_DISABLE
    //                           : QMTCFG_CLEAN_SESSION_ENABLE;
    // }
    // else
    // {
    //   // Default to clean session if current setting is unknown
    //   new_clean_session = QMTCFG_CLEAN_SESSION_ENABLE;
    // }
    //
    // ESP_LOGI(TAG,
    //          "Setting new session type: %s",
    //          new_clean_session == QMTCFG_CLEAN_SESSION_ENABLE ? "Clean (discard information)"
    //                                                           : "Persistent (store
    //                                                           subscriptions)");
    //
    // err = bg95_mqtt_config_set_session(bg95_handle, 0, new_clean_session);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to set MQTT session type: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Query again to verify the changes
    // memset(&session_response, 0, sizeof(session_response));
    // err = bg95_mqtt_config_query_session(bg95_handle, 0, &session_response);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to query MQTT session type after update: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Verify the new configuration
    // if (session_response.present.has_clean_session)
    // {
    //   ESP_LOGI(TAG,
    //            "Updated MQTT session type: %s",
    //            session_response.clean_session == QMTCFG_CLEAN_SESSION_ENABLE
    //                ? "Clean (discard information)"
    //                : "Persistent (store subscriptions)");
    //
    //   // Verify if the new value matches what we set
    //   if (session_response.clean_session == new_clean_session)
    //   {
    //     ESP_LOGI(TAG, "Session type successfully updated!");
    //   }
    //   else
    //   {
    //     ESP_LOGW(TAG, "Session type doesn't match the requested value");
    //   }
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "No session configuration found after update");
    // }
    //
    // ESP_LOGI(TAG, "--------- MQTT SESSION CONFIGURATION TEST COMPLETE ---------");
    //
    // ESP_LOGI(TAG, "--------- TESTING MQTT WILL CONFIGURATION ---------");
    //
    // // First, query the current will configuration
    // qmtcfg_write_will_response_t will_response = {0};
    // err = bg95_mqtt_config_query_will(bg95_handle, 0, &will_response);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to query MQTT will configuration: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Display current configuration
    // if (will_response.present.has_will_flag)
    // {
    //   if (will_response.will_flag == QMTCFG_WILL_FLAG_IGNORE)
    //   {
    //     ESP_LOGI(TAG, "Current MQTT will configuration: Disabled");
    //   }
    //   else
    //   {
    //     ESP_LOGI(TAG, "Current MQTT will configuration: Enabled");
    //
    //     if (will_response.present.has_will_qos)
    //     {
    //       ESP_LOGI(TAG, "  QoS: %d", will_response.will_qos);
    //     }
    //
    //     if (will_response.present.has_will_retain)
    //     {
    //       ESP_LOGI(TAG,
    //                "  Retain: %s",
    //                will_response.will_retain == QMTCFG_WILL_RETAIN_ENABLE ? "Yes" : "No");
    //     }
    //
    //     if (will_response.present.has_will_topic)
    //     {
    //       ESP_LOGI(TAG, "  Topic: %s", will_response.will_topic);
    //     }
    //
    //     if (will_response.present.has_will_message)
    //     {
    //       ESP_LOGI(TAG, "  Message: %s", will_response.will_message);
    //     }
    //   }
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "No will configuration found");
    // }
    //
    // // Now set a new will configuration
    // // For testing, we'll toggle between enabling and disabling will
    // qmtcfg_will_flag_t new_will_flag;
    //
    // if (will_response.present.has_will_flag)
    // {
    //   // Toggle the current setting
    //   new_will_flag = (will_response.will_flag == QMTCFG_WILL_FLAG_REQUIRE)
    //                       ? QMTCFG_WILL_FLAG_IGNORE
    //                       : QMTCFG_WILL_FLAG_REQUIRE;
    // }
    // else
    // {
    //   // Default to enabling will if current setting is unknown
    //   new_will_flag = QMTCFG_WILL_FLAG_REQUIRE;
    // }
    //
    // if (new_will_flag == QMTCFG_WILL_FLAG_IGNORE)
    // {
    //   // Disable will
    //   ESP_LOGI(TAG, "Disabling MQTT will");
    //
    //   err = bg95_mqtt_config_set_will(bg95_handle, 0, QMTCFG_WILL_FLAG_IGNORE, 0, 0, NULL, NULL);
    // }
    // else
    // {
    //   // Enable will with sample settings
    //   qmtcfg_will_qos_t    new_will_qos     = QMTCFG_WILL_QOS_1;          // At least once
    //   qmtcfg_will_retain_t new_will_retain  = QMTCFG_WILL_RETAIN_DISABLE; // Don't retain
    //   const char*          new_will_topic   = "device/offline";
    //   const char*          new_will_message = "Device disconnected unexpectedly";
    //
    //   ESP_LOGI(TAG, "Enabling MQTT will with:");
    //   ESP_LOGI(TAG, "  QoS: %d", new_will_qos);
    //   ESP_LOGI(TAG, "  Retain: %s", new_will_retain == QMTCFG_WILL_RETAIN_ENABLE ? "Yes" : "No");
    //   ESP_LOGI(TAG, "  Topic: %s", new_will_topic);
    //   ESP_LOGI(TAG, "  Message: %s", new_will_message);
    //
    //   err = bg95_mqtt_config_set_will(bg95_handle,
    //                                   0,
    //                                   QMTCFG_WILL_FLAG_REQUIRE,
    //                                   new_will_qos,
    //                                   new_will_retain,
    //                                   new_will_topic,
    //                                   new_will_message);
    // }
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to set MQTT will configuration: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Query again to verify the changes
    // memset(&will_response, 0, sizeof(will_response));
    // err = bg95_mqtt_config_query_will(bg95_handle, 0, &will_response);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(
    //       TAG, "Failed to query MQTT will configuration after update: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Verify the new configuration
    // if (will_response.present.has_will_flag)
    // {
    //   if (will_response.will_flag == QMTCFG_WILL_FLAG_IGNORE)
    //   {
    //     ESP_LOGI(TAG, "Updated MQTT will configuration: Disabled");
    //
    //     if (new_will_flag == QMTCFG_WILL_FLAG_IGNORE)
    //     {
    //       ESP_LOGI(TAG, "Will configuration successfully updated (disabled)!");
    //     }
    //     else
    //     {
    //       ESP_LOGW(TAG, "Will flag doesn't match the requested value (should be enabled)");
    //     }
    //   }
    //   else
    //   {
    //     ESP_LOGI(TAG, "Updated MQTT will configuration: Enabled");
    //
    //     if (new_will_flag == QMTCFG_WILL_FLAG_REQUIRE)
    //     {
    //       ESP_LOGI(TAG, "Will configuration successfully updated (enabled)!");
    //
    //       // Log details of the enabled will configuration
    //       if (will_response.present.has_will_qos)
    //       {
    //         ESP_LOGI(TAG, "  QoS: %d", will_response.will_qos);
    //       }
    //
    //       if (will_response.present.has_will_retain)
    //       {
    //         ESP_LOGI(TAG,
    //                  "  Retain: %s",
    //                  will_response.will_retain == QMTCFG_WILL_RETAIN_ENABLE ? "Yes" : "No");
    //       }
    //
    //       if (will_response.present.has_will_topic)
    //       {
    //         ESP_LOGI(TAG, "  Topic: %s", will_response.will_topic);
    //       }
    //
    //       if (will_response.present.has_will_message)
    //       {
    //         ESP_LOGI(TAG, "  Message: %s", will_response.will_message);
    //       }
    //     }
    //     else
    //     {
    //       ESP_LOGW(TAG, "Will flag doesn't match the requested value (should be disabled)");
    //     }
    //   }
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "No will configuration found after update");
    // }
    //
    // ESP_LOGI(TAG, "--------- MQTT WILL CONFIGURATION TEST COMPLETE ---------");
    //
    // ESP_LOGI(TAG, "--------- TESTING MQTT RECEIVE MODE CONFIGURATION ---------");
    //
    // // First, query the current receive mode configuration
    // qmtcfg_write_recv_mode_response_t recv_mode_response = {0};
    // err = bg95_mqtt_config_query_recv_mode(bg95_handle, 0, &recv_mode_response);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to query MQTT receive mode configuration: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Display current configuration
    // ESP_LOGI(TAG, "Current MQTT receive mode configuration:");
    //
    // qmtcfg_msg_recv_mode_t  current_msg_recv_mode  = QMTCFG_MSG_RECV_MODE_CONTAIN_IN_URC; //
    // Default qmtcfg_msg_len_enable_t current_msg_len_enable = QMTCFG_MSG_LEN_DISABLE; // Default
    //
    // if (recv_mode_response.present.has_msg_recv_mode)
    // {
    //   current_msg_recv_mode = recv_mode_response.msg_recv_mode;
    //   ESP_LOGI(TAG,
    //            "  Message receive mode: %s",
    //            current_msg_recv_mode == QMTCFG_MSG_RECV_MODE_CONTAIN_IN_URC
    //                ? "Contained in URC"
    //                : "Not contained in URC");
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "  Message receive mode information not available");
    // }
    //
    // if (recv_mode_response.present.has_msg_len_enable)
    // {
    //   current_msg_len_enable = recv_mode_response.msg_len_enable;
    //   ESP_LOGI(TAG,
    //            "  Message length: %s",
    //            current_msg_len_enable == QMTCFG_MSG_LEN_ENABLE ? "Contained in URC"
    //                                                            : "Not contained in URC");
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "  Message length enable information not available");
    // }
    //
    // // Now set new receive mode parameters that are different from current values
    // qmtcfg_msg_recv_mode_t new_msg_recv_mode =
    //     (current_msg_recv_mode == QMTCFG_MSG_RECV_MODE_CONTAIN_IN_URC)
    //         ? QMTCFG_MSG_RECV_MODE_NOT_CONTAIN_IN_URC
    //         : QMTCFG_MSG_RECV_MODE_CONTAIN_IN_URC;
    //
    // qmtcfg_msg_len_enable_t new_msg_len_enable = (current_msg_len_enable ==
    // QMTCFG_MSG_LEN_ENABLE)
    //                                                  ? QMTCFG_MSG_LEN_DISABLE
    //                                                  : QMTCFG_MSG_LEN_ENABLE;
    //
    // ESP_LOGI(TAG, "Setting new receive mode parameters:");
    // ESP_LOGI(TAG,
    //          "  Message receive mode: %s",
    //          new_msg_recv_mode == QMTCFG_MSG_RECV_MODE_CONTAIN_IN_URC ? "Contained in URC"
    //                                                                   : "Not contained in URC");
    // ESP_LOGI(TAG,
    //          "  Message length: %s",
    //          new_msg_len_enable == QMTCFG_MSG_LEN_ENABLE ? "Contained in URC"
    //                                                      : "Not contained in URC");
    //
    // err = bg95_mqtt_config_set_recv_mode(bg95_handle, 0, new_msg_recv_mode, new_msg_len_enable);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG, "Failed to set MQTT receive mode parameters: %s", esp_err_to_name(err));
    //   return;
    // }
    //
    // // Query again to verify the changes
    // memset(&recv_mode_response, 0, sizeof(recv_mode_response));
    // err = bg95_mqtt_config_query_recv_mode(bg95_handle, 0, &recv_mode_response);
    //
    // if (err != ESP_OK)
    // {
    //   ESP_LOGE(TAG,
    //            "Failed to query MQTT receive mode parameters after update: %s",
    //            esp_err_to_name(err));
    //   return;
    // }
    //
    // // Verify the new configuration
    // ESP_LOGI(TAG, "Updated MQTT receive mode configuration:");
    //
    // all_params_match = true;
    //
    // if (recv_mode_response.present.has_msg_recv_mode)
    // {
    //   ESP_LOGI(TAG,
    //            "  Message receive mode: %s",
    //            recv_mode_response.msg_recv_mode == QMTCFG_MSG_RECV_MODE_CONTAIN_IN_URC
    //                ? "Contained in URC"
    //                : "Not contained in URC");
    //
    //   if (recv_mode_response.msg_recv_mode != new_msg_recv_mode)
    //   {
    //     ESP_LOGW(TAG, "  Message receive mode doesn't match the requested value");
    //     all_params_match = false;
    //   }
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "  Message receive mode information not available after update");
    //   all_params_match = false;
    // }
    //
    // if (recv_mode_response.present.has_msg_len_enable)
    // {
    //   ESP_LOGI(TAG,
    //            "  Message length: %s",
    //            recv_mode_response.msg_len_enable == QMTCFG_MSG_LEN_ENABLE ? "Contained in URC"
    //                                                                       : "Not contained in
    //                                                                       URC");
    //
    //   if (recv_mode_response.msg_len_enable != new_msg_len_enable)
    //   {
    //     ESP_LOGW(TAG, "  Message length enable doesn't match the requested value");
    //     all_params_match = false;
    //   }
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "  Message length enable information not available after update");
    //   all_params_match = false;
    // }
    //
    // if (all_params_match)
    // {
    //   ESP_LOGI(TAG, "All receive mode parameters successfully updated!");
    // }
    // else
    // {
    //   ESP_LOGW(TAG, "Some receive mode parameters don't match the requested values");
    // }
    //
    // ESP_LOGI(TAG, "--------- MQTT RECEIVE MODE CONFIGURATION TEST COMPLETE ---------");

    // vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void app_main(void)
{
  ESP_LOGI(TAG, "BG95 Driver Dev Project Main started ...");

  config_and_init_uart();
  init_bg95();

  // TODO: check if already connected to network - if so, then skip the step to connect to network

  BaseType_t ret = xTaskCreate(
      connect_to_cell_network_task, "conn_to_network_bearer_task", 24000, &handle, 2, NULL);

  if (ret != pdPASS)
  {
    ESP_LOGE(TAG, "ERROR creating conn to network bearer task failed");
    return;
  }
}
