#include "at_cmd_cgpaddr.h"
#include "at_cmd_cops.h"
#include "at_cmd_qmtcfg.h"
#include "at_cmd_qmtclose.h"
#include "at_cmd_qmtconn.h"
#include "at_cmd_qmtdisc.h"
#include "at_cmd_qmtopen.h"
#include "at_cmd_qmtpub.h"
#include "bg95_driver.h"
#include "freertos/projdefs.h"

#include <esp_err.h>
#include <esp_log.h>
#include <stdio.h>
#include <stdlib.h> // For rand()
#include <string.h>

static const char* TAG = "Main";

// static global references to UART and BG95 handles used as Singletons
static bg95_uart_interface_t uart   = {0};
static bg95_handle_t         handle = {0};

#define UART_TX_GPIO 32
#define UART_RX_GPIO 33
#define UART_PORT_NUM 2

// MQTT Configuration Parameters
#define MQTT_CLIENT_IDX 0 // Use client index 0
#define MQTT_BROKER_HOST "host.name.here.io"
#define MQTT_BROKER_PORT 1883 // Standard MQTT port
#define MQTT_CLIENT_ID "client_id"
#define MQTT_USERNAME "client_username"
#define MQTT_PASSWORD "broker_client_password"
#define MQTT_PUBLISH_TOPIC "topic_name_here"
#define MQTT_PUBLISH_QOS QMTPUB_QOS_AT_LEAST_ONCE
#define MQTT_PUBLISH_RETAIN QMTPUB_RETAIN_DISABLED
#define MQTT_PUBLISH_MSGID 1 // Message ID (used for QoS > 0)

static void config_and_init_uart(void)
{
  bg95_uart_config_t uart_config = {
      .tx_gpio_num = UART_TX_GPIO, .rx_gpio_num = UART_RX_GPIO, .port_num = UART_PORT_NUM};

  esp_err_t err = bg95_uart_interface_init_hw(&uart, uart_config);

  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to init UART: %s", esp_err_to_name(err));
    return;
  }

  if (!uart.write || !uart.read)
  {
    ESP_LOGE(TAG, "UART functions not properly initialized");
    return;
  }
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

// This function demonstrates how to publish a message via MQTT
static esp_err_t publish_mqtt_message(bg95_handle_t* bg95_handle, const char* message)
{
  esp_err_t err;

  if (!bg95_handle->initialized)
  {
    ESP_LOGE(TAG, "BG95 driver not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Publishing message to topic '%s': %s", MQTT_PUBLISH_TOPIC, message);
  qmtpub_write_response_t pub_response = {0};

  err = bg95_mqtt_publish_fixed_length(bg95_handle,
                                       MQTT_CLIENT_IDX,
                                       MQTT_PUBLISH_MSGID,
                                       MQTT_PUBLISH_QOS,
                                       MQTT_PUBLISH_RETAIN,
                                       MQTT_PUBLISH_TOPIC,
                                       message,
                                       strlen(message),
                                       &pub_response);

  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to publish MQTT message: %s", esp_err_to_name(err));
    return err;
  }

  // Check the immediate response if available
  if (pub_response.present.has_result)
  {
    if (pub_response.result == QMTPUB_RESULT_SUCCESS)
    {
      ESP_LOGI(TAG, "Message published successfully!");
    }
    else
    {
      ESP_LOGW(TAG,
               "Message publication in progress, result: %s",
               enum_to_str(pub_response.result, QMTPUB_RESULT_MAP, QMTPUB_RESULT_MAP_SIZE));

      // For retransmission we might have a value
      if (pub_response.result == QMTPUB_RESULT_RETRANSMISSION && pub_response.present.has_value)
      {
        ESP_LOGW(TAG, "Retransmission count: %d", pub_response.value);
      }
    }
  }
  else
  {
    ESP_LOGI(TAG, "Message publication initiated, waiting for result...");
    // In a real application, you would handle the URC asynchronously
  }

  return ESP_OK;
}

static void connect_and_publish_task(void* pvParams)
{
  bg95_handle_t* bg95_handle = (bg95_handle_t*) pvParams;
  esp_err_t      err;
  int            cid             = 1;
  int            mqtt_client_idx = MQTT_CLIENT_IDX;
  int            msg_count       = 0;
  char           message_buffer[128];

// Define variables for subscription
#define MQTT_SUBSCRIBE_TOPIC "testbucket1/response"
#define MQTT_SUBSCRIBE_QOS QMTSUB_QOS_AT_LEAST_ONCE
#define MQTT_SUBSCRIBE_MSGID 2 // Different from publish msgid
#define MQTT_UNSUBSCRIBE_MSGID 3

  // Main connection and publishing loop
  for (;;)
  {
    // 1. Check if already connected to the network
    bool is_pdp_context_active = false;
    err = bg95_is_pdp_context_active(bg95_handle, cid, &is_pdp_context_active);
    if (err != ESP_OK || !is_pdp_context_active)
    {
      ESP_LOGI(TAG, "PDP context not active, connecting to network...");
      err = bg95_connect_to_network(bg95_handle);
      if (err != ESP_OK)
      {
        ESP_LOGE(TAG, "Failed to connect to network: %s", esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(5000)); // Wait before retrying
        continue;
      }

      ESP_LOGI(TAG, "Successfully connected to cellular network");
      // Wait a bit for connection to stabilize
      vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 2. Check if MQTT network connection is open
    qmtopen_read_response_t open_status = {0};
    err = bg95_mqtt_network_open_status(bg95_handle, mqtt_client_idx, &open_status);

    if (err != ESP_OK)
    {
      ESP_LOGI(
          TAG, "Opening MQTT network connection to %s:%d...", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
      qmtopen_write_response_t qmtopen_response = {0};

      err = bg95_mqtt_open_network(
          bg95_handle, mqtt_client_idx, MQTT_BROKER_HOST, MQTT_BROKER_PORT, &qmtopen_response);
      if (err != ESP_OK)
      {
        ESP_LOGE(TAG, "Failed to open MQTT network connection: %s", esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }

      ESP_LOGI(TAG, "MQTT network connection request sent");
      // Wait for connection to establish
      vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // 3. Check if client is connected to the MQTT broker
    qmtconn_read_response_t qmtconn_read_response = {0};
    err = bg95_mqtt_query_connection_state(bg95_handle, mqtt_client_idx, &qmtconn_read_response);

    if (err != ESP_OK || qmtconn_read_response.state != QMTCONN_STATE_CONNECTED)
    {
      ESP_LOGI(TAG, "Connecting to MQTT broker with client ID '%s'...", MQTT_CLIENT_ID);
      qmtconn_write_response_t qmtconn_write_response = {0};

      err = bg95_mqtt_connect(bg95_handle,
                              mqtt_client_idx,
                              MQTT_CLIENT_ID,
                              MQTT_USERNAME,
                              MQTT_PASSWORD,
                              &qmtconn_write_response);

      if (err != ESP_OK)
      {
        ESP_LOGE(TAG, "Failed to connect to MQTT broker: %s", esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }

      ESP_LOGI(TAG, "MQTT connection request sent");
      // Wait for connection to establish
      vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // 4. Subscribe to a topic for receiving responses (NEW FUNCTIONALITY)
    ESP_LOGI(TAG,
             "Subscribing to MQTT topic '%s' with QoS %d...",
             MQTT_SUBSCRIBE_TOPIC,
             MQTT_SUBSCRIBE_QOS);

    qmtsub_write_response_t sub_response = {0};
    err                                  = bg95_mqtt_subscribe(bg95_handle,
                              mqtt_client_idx,
                              MQTT_SUBSCRIBE_MSGID,
                              MQTT_SUBSCRIBE_TOPIC,
                              MQTT_SUBSCRIBE_QOS,
                              &sub_response);

    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "Failed to subscribe to topic: %s", esp_err_to_name(err));
    }
    else if (sub_response.present.has_result)
    {
      if (sub_response.result == QMTSUB_RESULT_SUCCESS)
      {
        ESP_LOGI(TAG, "Successfully subscribed to topic '%s'", MQTT_SUBSCRIBE_TOPIC);
        if (sub_response.present.has_value)
        {
          ESP_LOGI(TAG, "Granted QoS: %d", sub_response.value);
        }
      }
      else
      {
        ESP_LOGW(TAG,
                 "Subscription in progress, result: %s",
                 enum_to_str(sub_response.result, QMTSUB_RESULT_MAP, QMTSUB_RESULT_MAP_SIZE));
      }
    }
    else
    {
      ESP_LOGI(TAG, "Subscription request sent, waiting for result...");
    }

    // Wait a bit after subscription
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 5. Now that we have a connection, let's publish some messages
    for (int i = 0; i < 3; i++)
    { // Publish 3 messages per connection cycle
      // Create a sample message with incrementing counter
      snprintf(message_buffer,
               sizeof(message_buffer),
               "{\"device_id\":\"%s\",\"sequence\":%d,\"temperature\":%.1f,\"humidity\":%.1f}",
               MQTT_CLIENT_ID,
               msg_count++,
               25.5 + (float) (rand() % 10) / 10.0f,  // Random temperature data
               45.0 + (float) (rand() % 20) / 10.0f); // Random humidity data

      // Publish the message
      err = publish_mqtt_message(bg95_handle, message_buffer);
      if (err != ESP_OK)
      {
        ESP_LOGE(TAG, "Failed to publish message: %s", esp_err_to_name(err));
        break; // Exit the publishing loop on error
      }

      // Wait between publications
      vTaskDelay(pdMS_TO_TICKS(5000));
    }

    // 6. Unsubscribe from the topic before disconnecting
    ESP_LOGI(TAG, "Unsubscribing from MQTT topic '%s'...", MQTT_SUBSCRIBE_TOPIC);

    qmtuns_write_response_t unsub_response = {0};
    err                                    = bg95_mqtt_unsubscribe(bg95_handle,
                                mqtt_client_idx,
                                MQTT_UNSUBSCRIBE_MSGID,
                                MQTT_SUBSCRIBE_TOPIC,
                                &unsub_response);

    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "Failed to unsubscribe from topic: %s", esp_err_to_name(err));
    }
    else if (unsub_response.present.has_result)
    {
      if (unsub_response.result == QMTUNS_RESULT_SUCCESS)
      {
        ESP_LOGI(TAG, "Successfully unsubscribed from topic '%s'", MQTT_SUBSCRIBE_TOPIC);
      }
      else
      {
        ESP_LOGW(TAG,
                 "Unsubscription in progress, result: %s",
                 enum_to_str(unsub_response.result, QMTUNS_RESULT_MAP, QMTUNS_RESULT_MAP_SIZE));
      }
    }
    else
    {
      ESP_LOGI(TAG, "Unsubscription request sent, waiting for result...");
    }

    // Wait a bit after unsubscription
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 7. Disconnect MQTT (will reconnect on next loop iteration)
    qmtdisc_write_response_t qmtdisc_write_response = {0};
    err = bg95_mqtt_disconnect(bg95_handle, mqtt_client_idx, &qmtdisc_write_response);
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "Failed to disconnect from MQTT broker: %s", esp_err_to_name(err));
    }
    else
    {
      ESP_LOGI(TAG, "Successfully disconnected from MQTT broker");
    }

    // Wait before next connection cycle
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

void app_main(void)
{
  ESP_LOGI(TAG, "BG95 Driver Dev Project with MQTT Publish started ...");

  config_and_init_uart();
  init_bg95();

  // Create a task with larger stack for connection and MQTT operations
  BaseType_t ret = xTaskCreate(connect_and_publish_task,
                               "connect_publish_task",
                               24000, // Large stack size to prevent overflow
                               &handle,
                               2,
                               NULL);

  if (ret != pdPASS)
  {
    ESP_LOGE(TAG, "ERROR creating connect and publish task failed");
    return;
  }
}
