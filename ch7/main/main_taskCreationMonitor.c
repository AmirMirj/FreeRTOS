// UART for ESP32-S3 is configured with changes in menuconfig where systemview
// is not enabled.

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "MainApp";

#define GREEN_LED_PIN GPIO_NUM_4
#define BLUE_LED_PIN GPIO_NUM_5
#define RED_LED_PIN GPIO_NUM_6

#define STACK_SIZE_BYTES 4096
#define TRACE_CORE 0 // Pin all tasks to Core 0 for clean sequencing logs

// Correctly sized static allocation array matching ESP-IDF byte sizes
static uint8_t RedTaskStack[STACK_SIZE_BYTES];
static StaticTask_t RedTaskTCB;

// Define the number of instances
#define NUM_BLUE_TASKS 3

// Array to store handles for all 3 instances
TaskHandle_t blueTaskHandles[NUM_BLUE_TASKS] = {NULL};

void GreenTask(void *argument);
void BlueTask(void *argument);
void RedTask(void *argument);
void lookBusy(void);

void init_gpios(void) {
  gpio_config_t io_conf = {.intr_type = GPIO_INTR_DISABLE,
                           .mode = GPIO_MODE_OUTPUT,
                           .pin_bit_mask = (1ULL << GREEN_LED_PIN) |
                                           (1ULL << BLUE_LED_PIN) |
                                           (1ULL << RED_LED_PIN),
                           .pull_down_en = 0,
                           .pull_up_en = 0};
  gpio_config(&io_conf);
}

void app_main(void) {
  init_gpios();

  // Allow the serial port interface monitor text alignment to settle down
  vTaskDelay(pdMS_TO_TICKS(1000));
  ESP_LOGI(TAG, "Application started. Initializing Tasks...");

  // 1. Create GreenTask (Highest Priority)
  TaskHandle_t green_task = NULL;
  xTaskCreatePinnedToCore(GreenTask, "GreenTask", STACK_SIZE_BYTES, NULL,
                          tskIDLE_PRIORITY + 3, &green_task, TRACE_CORE);

  // 2. Create BlueTask (Normal Priority)
  for (int i = 0; i < NUM_BLUE_TASKS; i++) {
    char taskName[16];
    snprintf(taskName, sizeof(taskName), "BlueTask_%d", i);

    xTaskCreatePinnedToCore(BlueTask, "BlueTask", STACK_SIZE_BYTES, NULL,
                            tskIDLE_PRIORITY + 2, &blueTaskHandles[i],
                            TRACE_CORE);
  }

  // 3. Create RedTask (Statically Allocated)
  xTaskCreateStaticPinnedToCore(RedTask, "RedTask", STACK_SIZE_BYTES, NULL,
                                tskIDLE_PRIORITY + 2, RedTaskStack, &RedTaskTCB,
                                TRACE_CORE);

  ESP_LOGI(TAG, "All tasks deployed successfully.");
}

void GreenTask(void *argument) {
  ESP_LOGI(TAG, "GreenTask running. Turning Green LED ON.");
  gpio_set_level(GREEN_LED_PIN, 1);

  // This delay keeps this task alive, starving Blue and Red tasks due to
  // priority
  vTaskDelay(pdMS_TO_TICKS(1500));

  gpio_set_level(GREEN_LED_PIN, 0);
  ESP_LOGW(TAG, "GreenTask complete. Deleting itself.");
  vTaskDelete(NULL);
}

void BlueTask(void *argument) {
  // Retrieve the task ID from the argument
  int task_id = (int)(intptr_t)argument;

  while (1) {
    ESP_LOGI(TAG, "BlueTask %d executing.", task_id);
    gpio_set_level(BLUE_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(800));

    volatile uint32_t sum = 0;
    for (int i = 0; i < 200000; i++) {
      sum += i;
    }

    gpio_set_level(BLUE_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(800));
  }
}

void RedTask(void *argument) {
  uint8_t firstRun = 1;
  while (1) {
    lookBusy();

    ESP_LOGI(TAG, "RedTask executing. Turning Red LED ON.");
    gpio_set_level(RED_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(RED_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(500));

    if (firstRun == 1) {
      vTaskDelay(pdMS_TO_TICKS(2000)); // let Blue tasks run a bit longer

      // Loop through and safely delete all running blue tasks
      for (int i = 0; i < NUM_BLUE_TASKS; i++) {
        TaskHandle_t task_to_delete = blueTaskHandles[i];
        if (task_to_delete != NULL) {
          blueTaskHandles[i] = NULL;
          vTaskDelete(task_to_delete);
          ESP_LOGE(TAG, "RedTask successfully deleted BlueTask_%d.", i);
        }
      }
      firstRun = 0;
    }

    // Crucial yield point to allow background system idle tasks to breathe
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void lookBusy(void) {
  volatile uint32_t dontCare = 0;
  for (int i = 0; i < 200000; i++) {
    dontCare = i % 4;
  }
  ESP_LOGD(TAG, "Computational waste cycle finished: %lu", dontCare);
}
