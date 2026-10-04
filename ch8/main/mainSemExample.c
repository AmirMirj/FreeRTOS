#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>

#include "SEGGER_SYSVIEW.h"
#include "SEGGER_SYSVIEW_FreeRTOS.h"

#define GREEN_LED_PIN GPIO_NUM_4
#define BLUE_LED_PIN GPIO_NUM_5

#define STACK_SIZE 4096
#define TRACE_CORE 0 // Pin all tasks to Core 0 for clean sequencing logs

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

  // Give OpenOCD time to establish tracing connection before sending system
  // packets
  vTaskDelay(pdMS_TO_TICKS(1000));

  SEGGER_SYSVIEW_Conf();
  SEGGER_SYSVIEW_Start();

  // Create the binary semaphore
  semPtr = xSemaphoreCreateBinary();
  configASSERT(semPtr != NULL);

  // 1. Create GreenTaskA (Higher Priority: tskIDLE_PRIORITY + 2)
  TaskHandle_t green_task = NULL;
  xTaskCreatePinnedToCore(GreenTaskA, "GreenTaskA", STACK_SIZE, NULL,
                          tskIDLE_PRIORITY + 2, &green_task, TRACE_CORE);
  if (green_task != NULL) {
    register_task_with_sysview(green_task);
  }

  // 2. Create BlueTaskB (Lower Priority: tskIDLE_PRIORITY + 1)
  TaskHandle_t blue_task = NULL;
  xTaskCreatePinnedToCore(BlueTaskB, "BlueTaskB", STACK_SIZE, NULL,
                          tskIDLE_PRIORITY + 1, &blue_task, TRACE_CORE);
  if (blue_task != NULL) {
    register_task_with_sysview(blue_task);
  }

  SEGGER_SYSVIEW_PrintfHost("All tasks created successfully.\n");
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
      SEGGER_SYSVIEW_PrintfHost("GreenTaskA gives semPtr signal\n");
      xSemaphoreGive(semPtr);
    }

    // Toggle Green LED ON
    SEGGER_SYSVIEW_PrintfHost("Green LED: ON\n");
    gpio_set_level(GREEN_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Toggle Green LED OFF
    SEGGER_SYSVIEW_PrintfHost("Green LED: OFF\n");
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
    SEGGER_SYSVIEW_PrintfHost("BlueTaskB waiting for semPtr...\n");

    // Wait indefinitely (portMAX_DELAY) for the signal
    if (xSemaphoreTake(semPtr, portMAX_DELAY) == pdPASS) {
      SEGGER_SYSVIEW_PrintfHost(
          "BlueTaskB received semPtr! Executing triple-blink...\n");

      // Triple blink the Blue LED
      for (uint8_t i = 0; i < 3; i++) {
        SEGGER_SYSVIEW_PrintfHost("Blue LED: ON (Blink %u)\n",
                                  (unsigned)(i + 1));
        gpio_set_level(BLUE_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(50));

        SEGGER_SYSVIEW_PrintfHost("Blue LED: OFF (Blink %u)\n",
                                  (unsigned)(i + 1));
        gpio_set_level(BLUE_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
      }
    }
  }
}