idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES common driver esp_driver_gpio
)
