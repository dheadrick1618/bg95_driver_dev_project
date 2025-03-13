#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdio.h>
#include <unity.h>

static const char* TAG = "Test_Orchestrator";

/* Forward declarations of all test suites */
void run_test_at_cmd_parser_all(void);
void run_test_mock_uart_interface_all(void);
void run_test_at_cmd_handler_all(void);
void run_test_at_cmd_cpin_all(void);
void run_test_at_cmd_csq_all(void);
void run_test_at_cmd_cops_all(void);
void run_test_at_cmd_cgdcont_all(void);
void run_test_at_cmd_qmtcfg_all(void);

/* Define test suite information */
typedef struct
{
  const char* name;
  void (*test_function)(void);
} test_suite_t;

/* Array of all test suites */
static const test_suite_t test_suites[] = {
    {"AT CMD PARSER Tests", run_test_at_cmd_parser_all},
    {"MOCK UART Interface Tests", run_test_mock_uart_interface_all},
    {"AT CMD HANDLER Tests", run_test_at_cmd_handler_all},
    {"AT CMD: CPIN Tests", run_test_at_cmd_cpin_all},
    {"AT CMD: CSQ Tests", run_test_at_cmd_csq_all},
    {"AT CMD: COPS Tests", run_test_at_cmd_cops_all},
    {"AT CMD: CGDCONT Tests", run_test_at_cmd_cgdcont_all},
    {"AT CMD: QMTCFG Tests", run_test_at_cmd_qmtcfg_all},
};

#define NUM_TEST_SUITES (sizeof(test_suites) / sizeof(test_suite_t))
#define TEST_TASK_STACK_SIZE (8192) /* Generous stack size */
#define TEST_TASK_PRIORITY (5)

/* Queue for test completion notification */
static QueueHandle_t test_completion_queue;

/* Task to run a single test suite */
static void test_task(void* pvParameters)
{
  test_suite_t* suite = (test_suite_t*) pvParameters;

  /* Log the test suite we're running */
  ESP_LOGI(TAG, "Starting test suite: %s", suite->name);
  printf("\n===== %s =====\n", suite->name);

  /* Run the test suite function */
  suite->test_function();

  /* Notify completion */
  uint32_t suite_index = suite - test_suites;
  if (xQueueSend(test_completion_queue, &suite_index, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    ESP_LOGE(TAG, "Failed to send completion notification");
  }

  /* Delete the task */
  vTaskDelete(NULL);
}

void app_main(void)
{
  /* Create a queue for test completion notification */
  test_completion_queue = xQueueCreate(NUM_TEST_SUITES, sizeof(uint32_t));
  if (test_completion_queue == NULL)
  {
    ESP_LOGE(TAG, "Failed to create test completion queue");
    return;
  }

  printf("\n======== RUNNING ALL TESTS ========\n");

  /* Run the first test suite */
  uint32_t current_suite = 0;
  ESP_LOGI(TAG,
           "Creating task for test suite %lu: %s",
           (unsigned long) current_suite,
           test_suites[current_suite].name);

  TaskHandle_t test_task_handle;
  BaseType_t   result = xTaskCreate(test_task,
                                  "test_task",
                                  TEST_TASK_STACK_SIZE,
                                  (void*) &test_suites[current_suite],
                                  TEST_TASK_PRIORITY,
                                  &test_task_handle);

  if (result != pdPASS)
  {
    ESP_LOGE(TAG, "Failed to create test task");
    return;
  }

  current_suite++;

  /* Wait for each test to complete and then start the next one */
  while (current_suite < NUM_TEST_SUITES)
  {
    uint32_t completed_suite;

    /* Wait for a test to complete */
    if (xQueueReceive(test_completion_queue, &completed_suite, portMAX_DELAY) == pdTRUE)
    {
      ESP_LOGI(TAG, "Test suite %lu completed", (unsigned long) completed_suite);

      /* Start the next test */
      ESP_LOGI(TAG,
               "Creating task for test suite %lu: %s",
               (unsigned long) current_suite,
               test_suites[current_suite].name);

      result = xTaskCreate(test_task,
                           "test_task",
                           TEST_TASK_STACK_SIZE,
                           (void*) &test_suites[current_suite],
                           TEST_TASK_PRIORITY,
                           &test_task_handle);

      if (result != pdPASS)
      {
        ESP_LOGE(TAG, "Failed to create test task");
        break;
      }

      current_suite++;
    }
  }

  /* Wait for the last test to complete */
  uint32_t completed_suite;
  xQueueReceive(test_completion_queue, &completed_suite, portMAX_DELAY);
  ESP_LOGI(TAG, "Test suite %lu completed", (unsigned long) completed_suite);

  printf("\n======== ALL TESTS COMPLETED ========\n");

  /* Clean up */
  vQueueDelete(test_completion_queue);
}
