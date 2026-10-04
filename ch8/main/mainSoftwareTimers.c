/**
 * MIT License
 *
 * Copyright (c) 2019 Brian Amos
 * Ported to ESP32-S3 (ESP-IDF) with SystemView & Software Timers
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "SEGGER_SYSVIEW.h"
#include "SEGGER_SYSVIEW_FreeRTOS.h"

#define GREEN_LED_PIN GPIO_NUM_4
#define BLUE_LED_PIN  GPIO_NUM_5
#define BUTTON_PIN    GPIO_NUM_0 // Typical active-low Boot button on ESP32-S3

void oneShotCallBack(TimerHandle_t xTimer);
void repeatCallBack(TimerHandle_t xTimer);

static void init_hardware(void) {
  // Configure LED pins
  gpio_config_t led_conf = {
      .intr_type = GPIO_INTR_DISABLE,
      .mode = GPIO_MODE_OUTPUT,
      .pin_bit_mask = (1ULL << GREEN_LED_PIN) | (1ULL << BLUE_LED_PIN),
      .pull_down_en = 0,
      .pull_up_en = 0};
  gpio_config(&led_conf);

  // Configure Button pin with internal pull-up
  gpio_config_t btn_conf = {.intr_type = GPIO_INTR_DISABLE,
                            .mode = GPIO_MODE_INPUT,
                            .pin_bit_mask = (1ULL << BUTTON_PIN),
                            .pull_down_en = 0,
                            .pull_up_en = 1};
  gpio_config(&btn_conf);
}

void app_main(void) {
  init_hardware();

  // Allow OpenOCD tracing connection time to establish
  vTaskDelay(pdMS_TO_TICKS(1000));

  SEGGER_SYSVIEW_Conf();
  SEGGER_SYSVIEW_Start();

  // Create auto-reload timer (500 ms)
  TimerHandle_t repeatHandle =
      xTimerCreate("myRepeatTimer", pdMS_TO_TICKS(500), pdTRUE, NULL,
                   repeatCallBack);
  configASSERT(repeatHandle != NULL);
  xTimerStart(repeatHandle, 0);

  // Initial state: Turn on Blue LED
  gpio_set_level(BLUE_LED_PIN, 1);

  // Create single-shot timer (2200 ms)
  TimerHandle_t oneShotHandle =
      xTimerCreate("myOneShotTimer", pdMS_TO_TICKS(2200), pdFALSE, NULL,
                   oneShotCallBack);
  configASSERT(oneShotHandle != NULL);
  xTimerStart(oneShotHandle, 0);

  SEGGER_SYSVIEW_PrintfHost("Waiting for push button press on GPIO %d...",
                            BUTTON_PIN);

  // Busy-wait for push button (active low) before letting app_main finish.
  // Note: Since FreeRTOS scheduler is already active in ESP-IDF prior to
  // app_main, timer commands queue up immediately and execute in the background
  // via the Timer Service Task (Tmr Svc).
  while (gpio_get_level(BUTTON_PIN) != 0) {
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  SEGGER_SYSVIEW_PrintfHost("Button pressed!");
}

void oneShotCallBack(TimerHandle_t xTimer) {
  SEGGER_SYSVIEW_PrintfHost("blue LED off\n");
  gpio_set_level(BLUE_LED_PIN, 0);
}

void repeatCallBack(TimerHandle_t xTimer) {
  static uint32_t counter = 0;

  SEGGER_SYSVIEW_PrintfHost("toggle Green LED\n");
  if (counter++ % 2) {
    gpio_set_level(GREEN_LED_PIN, 1);
  } else {
    gpio_set_level(GREEN_LED_PIN, 0);
  }
}
