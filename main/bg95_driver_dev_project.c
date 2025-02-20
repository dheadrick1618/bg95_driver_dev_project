#include "bg95_driver.h"
#include "freertos/projdefs.h"

#include <esp_err.h>
#include <esp_log.h>
#include <stdio.h>

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
static void conn_to_network_bearer_task(void* pvParams)
{
  bg95_handle_t* bg95_handle = (bg95_handle_t*) pvParams;

  esp_err_t err;

  for (;;)
  {
    // Check SIM card status
    // -----------------------------------------------------------
    cpin_status_t sim_card_status;
    err = bg95_get_sim_card_status(bg95_handle, &sim_card_status);
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "Failed to get PIN status: %d", err);
      return;
    }

    // Check signal quality (AT+CSQ)
    // -----------------------------------------------------------
    int16_t rssi_dbm;
    err = bg95_get_signal_quality_dbm(bg95_handle, &rssi_dbm);
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "Failed to check signal quality: %d", err);
      return;
    }

    // Check available operators list (AT+COPS)
    // -----------------------------------------------------------

    // Define PDP context with your carriers APN (AT+CGDCONT)
    // -----------------------------------------------------------

    // Activate PDP context (AT+CGACT)
    // -----------------------------------------------------------

    // Verify IP address assignment (AT+CGPADDR)
    // -----------------------------------------------------------

    // Check network registration status (AT+CREG)
    // -----------------------------------------------------------

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void app_main(void)
{
  ESP_LOGI(TAG, "BG95 Driver Dev Project Main started ...");

  config_and_init_uart();
  init_bg95();

  BaseType_t ret = xTaskCreate(
      conn_to_network_bearer_task, "conn_to_network_bearer_task", 24000, &handle, 2, NULL);

  if (ret != pdPASS)
  {
    ESP_LOGE(TAG, "ERROR creating conn to network bearer task failed");
    return;
  }
}
