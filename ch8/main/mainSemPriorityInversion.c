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

static void blinkTwice(gpio_num_t gpio_num);
static void lookBusy(uint32_t numIterations);

void TaskA(void *argument);
void TaskB(void *argument);
void TaskC(void *argument);

// Storage for a pointer to a semaphore
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

  // Create a binary semaphore using the FreeRTOS Heap
  semPtr = xSemaphoreCreateBinary();
  configASSERT(semPtr != NULL);

  // Initial "give" of the semaphore
  xSemaphoreGive(semPtr);

  // Task A: Highest Priority (tskIDLE_PRIORITY + 3)
  TaskHandle_t task_a_handle = NULL;
  xTaskCreatePinnedToCore(TaskA, "TaskA", STACK_SIZE, NULL,
                          tskIDLE_PRIORITY + 3, &task_a_handle, TRACE_CORE);
  if (task_a_handle != NULL) {
    register_task_with_sysview(task_a_handle);
  }

  // Task B: Medium Priority (tskIDLE_PRIORITY + 2)
  TaskHandle_t task_b_handle = NULL;
  xTaskCreatePinnedToCore(TaskB, "TaskB", STACK_SIZE, NULL,
                          tskIDLE_PRIORITY + 2, &task_b_handle, TRACE_CORE);
  if (task_b_handle != NULL) {
    register_task_with_sysview(task_b_handle);
  }

  // Task C: Lowest Priority (tskIDLE_PRIORITY + 1)
  TaskHandle_t task_c_handle = NULL;
  xTaskCreatePinnedToCore(TaskC, "TaskC", STACK_SIZE, NULL,
                          tskIDLE_PRIORITY + 1, &task_c_handle, TRACE_CORE);
  if (task_c_handle != NULL) {
    register_task_with_sysview(task_c_handle);
  }

  SEGGER_SYSVIEW_PrintfHost("All tasks created and semaphore initialized.\n");
}

/**
 * Task A is the highest priority task in the system.
 * It periodically takes the semaphore with a 200mS timeout.
 * If taken within 200 ms, Red LED is turned off and Green LED blinks twice.
 * If not taken within 200 ms, Red LED is turned on.
 */
void TaskA(void *argument) {
  while (1) {
    SEGGER_SYSVIEW_PrintfHost("attempt to take semPtr\n");
    if (xSemaphoreTake(semPtr, pdMS_TO_TICKS(200)) == pdPASS) {
      gpio_set_level(RED_LED_PIN, 0);
      SEGGER_SYSVIEW_PrintfHost("received semPtr\n");
      blinkTwice(GREEN_LED_PIN);
      xSemaphoreGive(semPtr);
    } else {
      SEGGER_SYSVIEW_PrintfHost("FAILED to receive semphr in time\n");
      gpio_set_level(RED_LED_PIN, 1);
    }

    // Sleep for a random period (5 to 30 ticks) to let lower tasks run
    uint32_t delay_ticks = 5 + (esp_random() % 26);
    vTaskDelay(delay_ticks);
  }
}

/**
 * Medium priority task that wakes up periodically and burns CPU cycles in a
 * busy loop.
 */
void TaskB(void *argument) {
  uint32_t counter = 0;
  while (1) {
    SEGGER_SYSVIEW_PrintfHost("starting iteration %u\n", counter++);

    uint32_t delay_ticks = 10 + (esp_random() % 16);
    vTaskDelay(delay_ticks);

    uint32_t iterations = 250000 + (esp_random() % 500001);
    lookBusy(iterations);
  }
}

/**
 * Lowest priority task in the system.
 * Attempts to take the semaphore with a 200mS timeout.
 * Double-blinks Blue LED on success; turns on Red LED on failure/timeout.
 */
void TaskC(void *argument) {
  while (1) {
    SEGGER_SYSVIEW_PrintfHost("attempt to take semPtr\n");
    if (xSemaphoreTake(semPtr, pdMS_TO_TICKS(200)) == pdPASS) {
      gpio_set_level(RED_LED_PIN, 0);
      SEGGER_SYSVIEW_PrintfHost("received semPtr\n");
      blinkTwice(BLUE_LED_PIN);
      xSemaphoreGive(semPtr);
    } else {
      SEGGER_SYSVIEW_PrintfHost("FAILED to receive semphr in time\n");
      gpio_set_level(RED_LED_PIN, 1);
    }
  }
}

/**
 * Blink the specified GPIO twice (43ms ON, 43ms OFF)
 */
static void blinkTwice(gpio_num_t gpio_num) {
  for (uint32_t i = 0; i < 2; i++) {
    gpio_set_level(gpio_num, 1);
    vTaskDelay(pdMS_TO_TICKS(43));
    gpio_set_level(gpio_num, 0);
    vTaskDelay(pdMS_TO_TICKS(43));
  }
}

/**
 * Burn CPU cycles for the given iteration count
 */
static void lookBusy(uint32_t numIterations) {
  volatile uint32_t dontCare = 0;
  for (uint32_t i = 0; i < numIterations; i++) {
    dontCare = i % 4;
  }
  (void)dontCare;
}
