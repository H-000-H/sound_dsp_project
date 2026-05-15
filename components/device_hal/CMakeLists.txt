idf_component_register(

    SRCS

        "button/src/button.cpp"

        "gpio/src/gpio_controller.cpp"

        "gpio/src/rgb_led.cpp"

        "i2s/src/i2s_bus.cpp"

        "pwm/src/pwm_controller.cpp"

        "spi/src/spi_bus.cpp"

        "lcd/src/st7789.cpp"

        "blue/src/blue.cpp"

        "network/src/wifi_manager.cpp"

        "time/src/m_time.cpp"

        "serial/src/serial_console.cpp"

    INCLUDE_DIRS

        "base/inc"

        "button/inc"

        "gpio/inc"

        "spi/inc"

        "i2s/inc"

        "pwm/inc"

        "lcd/inc"

        "blue/inc"

        "network/inc"

        "time/inc"

        "serial/inc"

    REQUIRES freertos config

    PRIV_REQUIRES bsp driver algorithm esp_driver_uart esp_driver_usb_serial_jtag

)
