#include "bg95_driver.h"

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

  ESP_LOGI(TAG,
           "UART interface check - write: %p, read: %p, context: %p",
           uart.write,
           uart.read,
           uart.context);

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

  // // Run loopback test
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

void app_main(void)
{
  ESP_LOGI(TAG, "BG95 Driver Dev Project Main started ...");

  esp_err_t err;

  config_and_init_uart();
  init_bg95();

  // Check SIM card status
  cpin_read_response_t pin_status = {0};

  err = bg95_get_pin_status(&handle, &pin_status);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to get PIN status: %d", err);
    return;
  }
}
