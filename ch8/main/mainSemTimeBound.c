/**
 * MIT License
 *
 * Copyright (c) 2019 Brian Amos
 * Ported to ESP32-S3 (ESP-IDF) with SystemView
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "SEGGER_SYSVIEW.h"
#include "SEGGER_SYSVIEW_FreeRTOS.h"

#define GREEN_LED_PIN GPIO_NUM_4
#define BLUE_LED_PIN GPIO_NUM_5
#define RED_LED_PIN GPIO_NUM_6

#define STACK_SIZE 4096
#define TRACE_CORE 0 // Pin tasks to Core 0 for clean tracing

static void greenBlink(void);
static void blueTripleBlink(void);
void GreenTaskA(void *argument);
void TaskB(void *argument);

// Storage for a pointer to a binary semaphore
SemaphoreHandle_t semPtr = NULL;

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

static void register_task_with_sysview(TaskHandle_t task_handle) {
  if (task_handle == NULL) {
    return;
  }

  const char *task_name = pcTaskGetName(task_handle);
  const UBaseType_t priority = uxTaskPriorityGet(task_handle);

  SYSVIEW_AddTask((U32)task_handle, task_name, (unsigned)priority, 0U, 0U);
}

void app_main(void) {
  init_gpios();

  // Allow OpenOCD tracing connection time to establish
  vTaskDelay(pdMS_TO_TICKS(1000));

  SEGGER_SYSVIEW_Conf();
  SEGGER_SYSVIEW_Start();

  // Create binary semaphore
  semPtr = xSemaphoreCreateBinary();
  configASSERT(semPtr != NULL);

  // 1. Create GreenTaskA (Higher Priority: tskIDLE_PRIORITY + 2)
  TaskHandle_t green_task = NULL;
  xTaskCreatePinnedToCore(GreenTaskA, "GreenTaskA", STACK_SIZE, NULL,
                          tskIDLE_PRIORITY + 2, &green_task, TRACE_CORE);
  if (green_task != NULL) {
    register_task_with_sysview(green_task);
  }

  // 2. Create TaskB (Lower Priority: tskIDLE_PRIORITY + 1)
  TaskHandle_t blue_task = NULL;
  xTaskCreatePinnedToCore(TaskB, "TaskB", STACK_SIZE, NULL,
                          tskIDLE_PRIORITY + 1, &blue_task, TRACE_CORE);
  if (blue_task != NULL) {
    register_task_with_sysview(blue_task);
  }

  SEGGER_SYSVIEW_PrintfHost("All tasks created successfully.\n");
}

/**
 * Task A periodically 'gives' semPtr.
 * Generates a random threshold between 3 and 7 loops to create variable signal
 * timing.
 */
void GreenTaskA(void *argument) {
  uint8_t count = 0;

  while (1) {
    // Generate random number of loops between 3 and 7 (inclusive)
    uint8_t numLoops = 3 + (esp_random() % 5);

    if (++count >= numLoops) {
      count = 0;
      SEGGER_SYSVIEW_PrintfHost("Task A (green LED) gives semPtr\n");
      xSemaphoreGive(semPtr);
    }

    greenBlink();
  }
}

/**
 * Attempt to take semPtr within a 500 ms time limit:
 * - On pdPASS: Turn off Red LED and execute triple-blink on Blue LED.
 * - On pdFALSE (Timeout): Turn on Red LED to indicate deadline missed.
 */
void TaskB(void *argument) {
  while (1) {
    SEGGER_SYSVIEW_PrintfHost("attempt to take semPtr\n");

    // Time-bound attempt to take semaphore (500 ms timeout)
    if (xSemaphoreTake(semPtr, pdMS_TO_TICKS(500)) == pdPASS) {
      gpio_set_level(RED_LED_PIN, 0);
      SEGGER_SYSVIEW_PrintfHost("received semPtr\n");
      blueTripleBlink();
    } else {
      // Timeout triggered: Semaphore not given within 500 ms window
      SEGGER_SYSVIEW_PrintfHost("FAILED to receive semphr in time\n");
      gpio_set_level(RED_LED_PIN, 1);
    }
  }
}

/**
 * Single Green LED toggle (100 ms ON, 100 ms OFF)
 */
static void greenBlink(void) {
  gpio_set_level(GREEN_LED_PIN, 1);
  vTaskDelay(pdMS_TO_TICKS(100));
  gpio_set_level(GREEN_LED_PIN, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
}

/**
 * Triple Blue LED blink in rapid succession (50 ms pulse width)
 */
static void blueTripleBlink(void) {
  for (uint8_t i = 0; i < 3; i++) {
    gpio_set_level(BLUE_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(BLUE_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
