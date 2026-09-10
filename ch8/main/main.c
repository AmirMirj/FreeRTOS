// UART for ESP32-S3 is configured with changes in menuconfig where systemview
// is not enabled.

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "MainApp";

#define GREEN_LED_PIN GPIO_NUM_4
#define BLUE_LED_PIN GPIO_NUM_5

#define STACK_SIZE_BYTES 4096
#define TRACE_CORE 0 // Pin all tasks to Core 0 for clean sequencing logs

// Storage pointer for the binary semaphore
SemaphoreHandle_t semPtr = NULL;

void GreenTaskA(void *argument);
void BlueTaskB(void *argument);

void init_gpios(void) {
  gpio_config_t io_conf = {.intr_type = GPIO_INTR_DISABLE,
                           .mode = GPIO_MODE_OUTPUT,
                           .pin_bit_mask =
                               (1ULL << GREEN_LED_PIN) | (1ULL << BLUE_LED_PIN),
                           .pull_down_en = 0,
                           .pull_up_en = 0};
  gpio_config(&io_conf);
}

void app_main(void) {
  init_gpios();

  // Allow the serial port interface monitor text alignment to settle down
  vTaskDelay(pdMS_TO_TICKS(1000));
  ESP_LOGI(TAG, "Application started. Initializing Binary Semaphore...");

  // Create a binary semaphore
  semPtr = xSemaphoreCreateBinary();
  configASSERT(semPtr != NULL);

  // 1. Create GreenTaskA (Higher Priority: tskIDLE_PRIORITY + 2)
  xTaskCreatePinnedToCore(GreenTaskA, "GreenTaskA", STACK_SIZE_BYTES, NULL,
                          tskIDLE_PRIORITY + 2, NULL, TRACE_CORE);

  // 2. Create BlueTaskB (Lower Priority: tskIDLE_PRIORITY + 1)
  xTaskCreatePinnedToCore(BlueTaskB, "BlueTaskB", STACK_SIZE_BYTES, NULL,
                          tskIDLE_PRIORITY + 1, NULL, TRACE_CORE);

  ESP_LOGI(TAG, "All tasks created successfully.");
}

/**
 * GreenTaskA blinks the Green LED continuously and 'gives' semPtr every 5
 * loops.
 */
void GreenTaskA(void *argument) {
  uint8_t count = 0;

  while (1) {
    // Every 5 times through the loop, give the semaphore
    if (++count >= 5) {
      count = 0;
      ESP_LOGI(TAG, "GreenTaskA gives semPtr signal");
      xSemaphoreGive(semPtr);
    }

    // Toggle Green LED ON
    ESP_LOGI(TAG, "Green LED: ON");
    gpio_set_level(GREEN_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Toggle Green LED OFF
    ESP_LOGI(TAG, "Green LED: OFF");
    gpio_set_level(GREEN_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/**
 * BlueTaskB waits indefinitely to 'take' semPtr, then triple-blinks the Blue
 * LED.
 */
void BlueTaskB(void *argument) {
  while (1) {
    ESP_LOGI(TAG, "BlueTaskB waiting for semPtr...");

    // Wait indefinitely (portMAX_DELAY) for the signal
    if (xSemaphoreTake(semPtr, portMAX_DELAY) == pdPASS) {
      ESP_LOGI(TAG, "BlueTaskB received semPtr! Executing triple-blink...");

      // Triple blink the Blue LED with explicit logs
      for (uint8_t i = 0; i < 3; i++) {
        ESP_LOGI(TAG, "Blue LED: ON (Blink %d)", i + 1);
        gpio_set_level(BLUE_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(50));

        ESP_LOGI(TAG, "Blue LED: OFF (Blink %d)", i + 1);
        gpio_set_level(BLUE_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
      }
    }
  }
}
