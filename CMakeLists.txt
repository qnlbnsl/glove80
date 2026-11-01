zephyr_include_directories(include)

if(CONFIG_INPUT_IQS5XX)
    zephyr_library()
    zephyr_library_sources(drivers/input/iqs5xx.c)
endif()

# Touchpad (PTP) support is built into the ZMK app itself: it needs ZMK's headers and event
# manager.
if(CONFIG_PTP_USB)
    target_sources(app PRIVATE src/ptp/ptp_usb.c)
endif()
if(CONFIG_PTP_SPLIT_PERIPHERAL OR CONFIG_PTP_SPLIT_CENTRAL)
    target_sources(app PRIVATE src/ptp/ptp_pack.c)
endif()
if(CONFIG_PTP_SPLIT_PERIPHERAL)
    target_sources(app PRIVATE src/ptp/ptp_split_peripheral.c)
endif()
if(CONFIG_PTP_SPLIT_CENTRAL)
    target_sources(app PRIVATE src/ptp/ptp_split_central.c)
endif()
