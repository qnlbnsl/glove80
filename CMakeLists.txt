if(CONFIG_INPUT_IQS5XX)
    zephyr_library()
    zephyr_library_sources(drivers/input/iqs5xx.c)
endif()
