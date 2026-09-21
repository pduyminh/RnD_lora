idf_component_register(
    SRCS "main.c" "kinematics.c" "axis_driver.c"
    INCLUDE_DIRS "."
    REQUIRES common driver esp_driver_gpio esp_driver_mcpwm
)
