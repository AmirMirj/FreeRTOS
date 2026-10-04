idf_component_register(
    SRCS "mainQueueCompositePassByValue.c"
    INCLUDE_DIRS "."
    REQUIRES app_trace esp_driver_gpio freertos
)
