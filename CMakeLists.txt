zephyr_include_directories(include)

if(CONFIG_INPUT_IQS5XX)
    zephyr_library()
    zephyr_library_sources(drivers/input/iqs5xx.c)
endif()

# Built into the ZMK app itself: needs ZMK's headers and event manager.
if(CONFIG_PTP_USB)
    target_sources(app PRIVATE src/ptp/ptp_usb.c)
endif()
