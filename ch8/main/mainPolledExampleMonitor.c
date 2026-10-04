#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static const char *TAG = "MainApp";

#define GREEN_LED_PIN GPIO_NUM_4
#define BLUE_LED_PIN GPIO_NUM_5

#define STACK_SIZE 4096
#define TRACE_CORE 0 // Pin all tasks to Core 0 for clean sequencing logs

// Shared polling flag
volatile uint32_t flag = 0;

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
  ESP_LOGI(TAG, "Application started. Initializing Tasks with Shared Flag...");

  // 1. Create GreenTaskA (Higher Priority: tskIDLE_PRIORITY + 2)
  xTaskCreatePinnedToCore(GreenTaskA, "GreenTaskA", STACK_SIZE, NULL,
                          tskIDLE_PRIORITY + 2, NULL, TRACE_CORE);

  // 2. Create BlueTaskB (Lower Priority: tskIDLE_PRIORITY + 1)
  xTaskCreatePinnedToCore(BlueTaskB, "BlueTaskB", STACK_SIZE, NULL,
                          tskIDLE_PRIORITY + 1, NULL, TRACE_CORE);

  ESP_LOGI(TAG, "All tasks created successfully.");
}

/**
 * Task A periodically sets 'flag' to 1, signaling Task B to run.
 */
void GreenTaskA(void *argument) {
  uint8_t count = 0;

  while (1) {
    // Every 5 times through the loop, set the flag
    if (++count >= 5) {
      count = 0;
      ESP_LOGI(TAG, "Task A (green LED) sets flag");
      flag = 1; // set 'flag' to 1 to "signal" BlueTaskB to run
    }

    ESP_LOGI(TAG, "Green LED: ON");
    gpio_set_level(GREEN_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Green LED: OFF");
    gpio_set_level(GREEN_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/**
 * Wait until flag != 0 then triple blink the Blue LED.
 */
void BlueTaskB(void *argument) {
  while (1) {
    ESP_LOGI(TAG, "Task B (Blue LED) starts polling on flag");

    // Repeatedly poll on flag. As soon as it is non-zero, exit loop
    while (!flag) {
      // Brief delay to prevent triggering ESP32 Task Watchdog Timer (TWDT)
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "Task B (Blue LED) received flag");
    flag = 0;

    // Triple blink the Blue LED
    for (uint8_t i = 0; i < 3; i++) {
      ESP_LOGI(TAG, "Blue LED: ON (Blink %u)", i + 1);
      gpio_set_level(BLUE_LED_PIN, 1);
      vTaskDelay(pdMS_TO_TICKS(50));

      ESP_LOGI(TAG, "Blue LED: OFF (Blink %u)", i + 1);
      gpio_set_level(BLUE_LED_PIN, 0);
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}
