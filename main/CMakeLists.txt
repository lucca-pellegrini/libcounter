idf_component_register(SRCS "../src/main.c"
                       "../src/sensor.c"
                       "../src/display.c"
                       "../src/button.c"
                       "../src/led.c"
                    INCLUDE_DIRS "../include"
                    REQUIRES driver freertos esp_timer
                             esp_driver_gpio esp_driver_i2c
                             esp_ssd1306 led_strip)
