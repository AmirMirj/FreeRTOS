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
#define BLUE_STACK_SIZE_BYTES (STACK_SIZE_BYTES * 10)
#define TRACE_CORE 0

static StackType_t RedTaskStack[STACK_SIZE_BYTES / sizeof(StackType_t)];
static StaticTask_t RedTaskTCB;
TaskHandle_t blueTaskHandle = NULL;

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

  vTaskDelay(pdMS_TO_TICKS(1000));
  ESP_LOGI(TAG, "Application started. Initializing Tasks...");

  // 1. Create GreenTask (Highest Priority)
  TaskHandle_t green_task = NULL;
  xTaskCreatePinnedToCore(GreenTask, "GreenTask", STACK_SIZE_BYTES, NULL,
                          tskIDLE_PRIORITY + 2, &green_task, TRACE_CORE);

  // 2. Create BlueTask (Normal Priority, oversized stack)
  xTaskCreatePinnedToCore(BlueTask, "BlueTask", BLUE_STACK_SIZE_BYTES, NULL,
                          tskIDLE_PRIORITY + 1, &blueTaskHandle, TRACE_CORE);

  // 3. Create RedTask (Statically Allocated)
  xTaskCreateStaticPinnedToCore(RedTask, "RedTask", STACK_SIZE_BYTES, NULL,
                                tskIDLE_PRIORITY + 1, RedTaskStack, &RedTaskTCB,
                                TRACE_CORE);

  ESP_LOGI(TAG, "All tasks deployed successfully.");
}

void GreenTask(void *argument) {
  ESP_LOGI(TAG, "Task1 running while Green LED is on");
  gpio_set_level(GREEN_LED_PIN, 1);

  vTaskDelay(pdMS_TO_TICKS(1500));

  gpio_set_level(GREEN_LED_PIN, 0);
  ESP_LOGW(TAG, "GreenTask deleting itself.");
  vTaskDelete(NULL);

  gpio_set_level(GREEN_LED_PIN, 1);
}

void BlueTask(void *argument) {
  while (1) {
    ESP_LOGI(TAG, "BlueTaskRunning");
    gpio_set_level(BLUE_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(BLUE_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void RedTask(void *argument) {
  uint8_t firstRun = 1;

  while (1) {
    lookBusy();

    ESP_LOGI(TAG, "RedTaskRunning");
    gpio_set_level(RED_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(RED_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(500));

    if (firstRun == 1) {
      TaskHandle_t task_to_delete = blueTaskHandle;
      if (task_to_delete != NULL) {
        blueTaskHandle = NULL;
        vTaskDelete(task_to_delete);
        ESP_LOGE(TAG, "RedTask deleted BlueTask.");
      }
      firstRun = 0;
    }
  }
}

void lookBusy(void) {
  volatile uint32_t dontCare = 0;
  for (int i = 0; i < 50000; i++) {
    dontCare = i % 4;
  }
  ESP_LOGD(TAG, "looking busy %lu", dontCare);
}