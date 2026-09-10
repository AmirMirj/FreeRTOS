#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>

#include "SEGGER_SYSVIEW.h"
#include "SEGGER_SYSVIEW_FreeRTOS.h"

static const char *TAG = "MainApp";

#define GREEN_LED_PIN GPIO_NUM_4
#define BLUE_LED_PIN GPIO_NUM_5
#define RED_LED_PIN GPIO_NUM_6

#define STACK_SIZE 4096
#define NUM_BLUE_TASKS 3

static StackType_t RedTaskStack[STACK_SIZE];
static StaticTask_t RedTaskTCB;
TaskHandle_t blueTaskHandles[NUM_BLUE_TASKS] = {NULL};

static volatile bool terminate_blue_tasks = false;

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

  // 1. Create GreenTask (Pinned to Core 0)
  TaskHandle_t green_task = NULL;
  xTaskCreatePinnedToCore(GreenTask, "GreenTask", STACK_SIZE, NULL,
                          tskIDLE_PRIORITY + 2, &green_task, 0);
  if (green_task != NULL) {
    register_task_with_sysview(green_task);
  }

  // 2. Create all BlueTask instances (Pinned to Core 0)
  for (int i = 0; i < NUM_BLUE_TASKS; i++) {
    char taskName[16]; // <-- FIXED: Corrected string buffer size
    snprintf(taskName, sizeof(taskName), "BlueTask_%d", i);

    xTaskCreatePinnedToCore(BlueTask, taskName, STACK_SIZE, (void *)(intptr_t)i,
                            tskIDLE_PRIORITY + 1, &blueTaskHandles[i], 0);
    if (blueTaskHandles[i] != NULL) {
      register_task_with_sysview(blueTaskHandles[i]);
    }
  }

  // 3. Create RedTask (Statically Allocated, Pinned to Core 0)
  TaskHandle_t red_task = xTaskCreateStaticPinnedToCore(
      RedTask, "RedTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, RedTaskStack,
      &RedTaskTCB, 0);
  if (red_task != NULL) {
    register_task_with_sysview(red_task);
  }
}

void GreenTask(void *argument) {
  SEGGER_SYSVIEW_PrintfHost("Task1 running while Green LED is on\n");
  gpio_set_level(GREEN_LED_PIN, 1);

  vTaskDelay(pdMS_TO_TICKS(1500));

  gpio_set_level(GREEN_LED_PIN, 0);
  vTaskDelete(NULL);
}

void BlueTask(void *argument) {
  int task_id = (int)(intptr_t)argument;

  while (1) {
    if (terminate_blue_tasks) {
      SEGGER_SYSVIEW_PrintfHost("BlueTask_%d self-deleting\n", task_id);
      blueTaskHandles[task_id] = NULL;
      vTaskDelete(NULL);
    }

    SEGGER_SYSVIEW_PrintfHost("BlueTask_%d running\n", task_id);
    gpio_set_level(BLUE_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(800));

    uint32_t sum = 0;
    for (uint32_t i = 0; i < 200000; i++) {
      sum += i;
    }

    gpio_set_level(BLUE_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(800));
  }
}

void RedTask(void *argument) {
  uint8_t blueRunCount = 0;
  while (1) {
    lookBusy();

    SEGGER_SYSVIEW_PrintfHost("RedTaskRunning\n");
    gpio_set_level(RED_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(RED_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(500));

    bool any_blue_task_exists = false;
    for (int i = 0; i < NUM_BLUE_TASKS; i++) {
      if (blueTaskHandles[i] != NULL) {
        any_blue_task_exists = true;
        break;
      }
    }

    if (any_blue_task_exists) {
      blueRunCount++;
      SEGGER_SYSVIEW_PrintfHost("Blue task count = %u\n", blueRunCount);

      if (blueRunCount >= 1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        terminate_blue_tasks = true;
        SEGGER_SYSVIEW_PrintfHost("All BlueTasks signaled for deletion\n");
        break;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  vTaskDelete(NULL);
}

void lookBusy(void) {
  volatile uint32_t result = 0;

  for (uint32_t i = 0; i < 500000; i++) {
    result += i;
    result ^= (i << 2);
  }

  SEGGER_SYSVIEW_PrintfHost("looking busy %lu\n", result);
}
