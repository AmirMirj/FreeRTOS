/**
 * MIT License
 *
 * Copyright (c) 2019 Brian Amos
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>

#include "SEGGER_SYSVIEW.h"
#include "SEGGER_SYSVIEW_FreeRTOS.h"

// Pin Definitions
#define GREEN_LED_PIN GPIO_NUM_4
#define BLUE_LED_PIN GPIO_NUM_5
#define RED_LED_PIN GPIO_NUM_6

#define STACK_SIZE 4096
#define TRACE_CORE 0 // Pin tasks to Core 0 for clean sequencing logs

/*********************************************
 * A simple demonstration of using queues across
 * multiple tasks with pass by value.
 * This time, a large struct is copied into
 * the queue
 *********************************************/

/**
 * Define a structure specifying LED states and the duration
 * in milliseconds for which that state should last.
 */
typedef struct {
  uint8_t redLEDState : 1;   // 1 bit wide
  uint8_t blueLEDState : 1;  // 1 bit wide
  uint8_t greenLEDState : 1; // 1 bit wide
  uint32_t msDelayTime;      // Min number of mS to remain in this state
} LedStates_t;

void recvTask(void *NotUsed);
void sendingTask(void *NotUsed);

// Handle for the queue used by recvTask and sendingTask
static QueueHandle_t ledCmdQueue = NULL;

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

  // Give OpenOCD time to establish tracing connection
  vTaskDelay(pdMS_TO_TICKS(1000));

  SEGGER_SYSVIEW_Conf();
  SEGGER_SYSVIEW_Start();

  /**
   * Create a queue that can store up to 8 copies of the struct.
   * Using sizeof allows us to modify the struct and have the queue
   * item storage sized appropriately at compile time.
   */
  ledCmdQueue = xQueueCreate(8, sizeof(LedStates_t));
  configASSERT(ledCmdQueue != NULL);

  // Setup tasks and register them with SystemView
  TaskHandle_t recv_handle = NULL;
  xTaskCreatePinnedToCore(recvTask, "recvTask", STACK_SIZE, NULL,
                          tskIDLE_PRIORITY + 1, &recv_handle, TRACE_CORE);
  configASSERT(recv_handle != NULL);
  register_task_with_sysview(recv_handle);

  TaskHandle_t send_handle = NULL;
  xTaskCreatePinnedToCore(sendingTask, "sendingTask", STACK_SIZE, NULL,
                          tskIDLE_PRIORITY + 2, &send_handle, TRACE_CORE);
  configASSERT(send_handle != NULL);
  register_task_with_sysview(send_handle);

  SEGGER_SYSVIEW_PrintfHost("Queue demo initialized successfully.\n");
}

/**
 * This receive task watches a queue for a new ledCmd to be added to it
 */
void recvTask(void *NotUsed) {
  LedStates_t nextCmd;

  while (1) {
    if (xQueueReceive(ledCmdQueue, &nextCmd, portMAX_DELAY) == pdTRUE) {
      SEGGER_SYSVIEW_PrintfHost(
          "Recv State -> Red:%d Green:%d Blue:%d Delay:%dms\n",
          nextCmd.redLEDState, nextCmd.greenLEDState, nextCmd.blueLEDState,
          (int)nextCmd.msDelayTime);

      gpio_set_level(RED_LED_PIN, nextCmd.redLEDState);
      gpio_set_level(BLUE_LED_PIN, nextCmd.blueLEDState);
      gpio_set_level(GREEN_LED_PIN, nextCmd.greenLEDState);
    }

    vTaskDelay(pdMS_TO_TICKS(nextCmd.msDelayTime));
  }
}

/**
 * sendingTask modifies a single nextStates variable
 * and passes it to the queue.
 * Each time the variable is passed to the queue, its
 * value is copied into the queue, which is allowed to
 * fill to capacity.
 */
void sendingTask(void *NotUsed) {
  // A single instance of nextStates is defined here
  LedStates_t nextStates;

  while (1) {
    nextStates.redLEDState = 1;
    nextStates.greenLEDState = 1;
    nextStates.blueLEDState = 1;
    nextStates.msDelayTime = 100;

    SEGGER_SYSVIEW_PrintfHost("Sending Cmd 1\n");
    xQueueSend(ledCmdQueue, &nextStates, portMAX_DELAY);

    nextStates.blueLEDState = 0; // Turn off just the blue LED
    nextStates.msDelayTime = 1500;
    SEGGER_SYSVIEW_PrintfHost("Sending Cmd 2\n");
    xQueueSend(ledCmdQueue, &nextStates, portMAX_DELAY);

    nextStates.greenLEDState = 0; // Turn off just the green LED
    nextStates.msDelayTime = 200;
    SEGGER_SYSVIEW_PrintfHost("Sending Cmd 3\n");
    xQueueSend(ledCmdQueue, &nextStates, portMAX_DELAY);

    nextStates.redLEDState = 0;
    SEGGER_SYSVIEW_PrintfHost("Sending Cmd 4\n");
    xQueueSend(ledCmdQueue, &nextStates, portMAX_DELAY);
  }
}
