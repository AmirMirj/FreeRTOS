#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

// Define logging tag
static const char *TAG = "MainApp";

// GPIO Pin assignments for ESP32-S3 (Adjust these to match your board)
#define GREEN_LED_PIN GPIO_NUM_4
#define BLUE_LED_PIN GPIO_NUM_5
#define RED_LED_PIN GPIO_NUM_6

// Function prototypes
void GreenTask(void *argument);
void BlueTask(void *argument);
void RedTask(void *argument);
void lookBusy(void);

// Task handle used by RedTask to delete BlueTask
TaskHandle_t blueTaskHandle = NULL;

// ESP-IDF stack sizes are in BYTES (128 words * 4 = 512 bytes)
// Standard ESP-IDF tasks often require at least 2048 bytes minimum due to
// driver overhead
#define STACK_SIZE 2048

// Static allocation structures for the Red Task
static StackType_t RedTaskStack[STACK_SIZE];
static StaticTask_t RedTaskTCB;

// Helper functions to mimic the STM32 Led.On/Off syntax
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
  // Initialize hardware GPIOs
  init_gpios();
  ESP_LOGI(TAG, "Hardware Initialized.");

  // Create GreenTask using standard dynamic allocation
  if (xTaskCreate(GreenTask, "GreenTask", STACK_SIZE, NULL,
                  tskIDLE_PRIORITY + 2, NULL) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create GreenTask");
    while (1) {
      vTaskDelay(1);
    }
  }

  // Create BlueTask with error checking
  if (xTaskCreate(BlueTask, "BlueTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 1,
                  &blueTaskHandle) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create BlueTask");
    while (1) {
      vTaskDelay(1);
    }
  }

  // Create RedTask using static memory allocation
  xTaskCreateStatic(RedTask, "RedTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 1,
                    RedTaskStack, &RedTaskTCB);

  // FreeRTOS scheduler starts automatically on ESP32.
  // This main thread can safely exit or sleep.
  ESP_LOGI(TAG, "Scheduler started automatically.");
}

void GreenTask(void *argument) {
  ESP_LOGI(TAG, "Task1 running while Green LED is on");
  gpio_set_level(GREEN_LED_PIN, 1);

  vTaskDelay(1500 / portTICK_PERIOD_MS);

  gpio_set_level(GREEN_LED_PIN, 0);

  // Task deletes itself
  vTaskDelete(NULL);

  // Code never reaches here
  gpio_set_level(GREEN_LED_PIN, 1);
}

void BlueTask(void *argument) {
  while (1) {
    ESP_LOGI(TAG, "BlueTaskRunning");
    gpio_set_level(BLUE_LED_PIN, 1);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    gpio_set_level(BLUE_LED_PIN, 0);
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void RedTask(void *argument) {
  uint8_t firstRun = 1;

  while (1) {
    lookBusy();

    ESP_LOGI(TAG, "RedTaskRunning");
    gpio_set_level(RED_LED_PIN, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(RED_LED_PIN, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    if (firstRun == 1) {
      if (blueTaskHandle != NULL) {
        vTaskDelete(blueTaskHandle);
        ESP_LOGI(TAG, "RedTask deleted BlueTask");
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
  ESP_LOGI(TAG, "looking busy %" PRIu32, dontCare);
}
