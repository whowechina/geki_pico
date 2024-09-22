function(make_firmware board board_def)
    pico_sdk_init()
    add_executable(${board}
        main.c light.c button.c lever.c sound.c airkey.c vl53l0x.c vl53l1x.c
        save.c config.c commands.c cli.c hid.c usb_descriptors.c)
    target_compile_definitions(${board} PUBLIC ${board_def})
    pico_enable_stdio_usb(${board} 1)
    pico_enable_stdio_uart(${board} 0)

    pico_generate_pio_header(${board} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
    
    target_compile_options(${board} PRIVATE -Wall -Werror -Wfatal-errors -O3)
    target_include_directories(${board} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    
    target_link_libraries(${board} PRIVATE
        pico_multicore pico_stdlib hardware_pio hardware_pwm hardware_flash
        hardware_adc hardware_i2c hardware_watchdog
        tinyusb_device tinyusb_board)

    target_link_libraries(${board} PRIVATE aic)

    pico_add_extra_outputs(${board})

    add_custom_command(TARGET ${board} PRE_BUILD
    COMMAND touch ${CMAKE_CURRENT_SOURCE_DIR}/cli.c)

    add_custom_command(TARGET ${board} POST_BUILD
                       COMMAND cp ${board}.uf2 ${CMAKE_CURRENT_LIST_DIR}/..)
endfunction()

make_firmware(geki_pico BOARD_GEKI_PICO)

