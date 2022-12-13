add_executable(spatial_audio_program)

# must match with pio filename and executable name from above
pico_generate_pio_header(spatial_audio_program ${CMAKE_CURRENT_LIST_DIR}/hsync.pio)
pico_generate_pio_header(spatial_audio_program ${CMAKE_CURRENT_LIST_DIR}/vsync.pio)
pico_generate_pio_header(spatial_audio_program ${CMAKE_CURRENT_LIST_DIR}/rgb.pio)

# must match with executable name and source file names
target_sources(spatial_audio_program PRIVATE spatial_audio_main.c vga_graphics.c vga_graphing.c fourier.c)

# must match with executable name
target_link_libraries(spatial_audio_program PRIVATE 
pico_stdlib 
pico_multicore 
pico_bootsel_via_double_reset 
hardware_pio 
hardware_dma 
hardware_adc 
hardware_irq 
hardware_sync 
hardware_spi 
hardware_i2c)

# must match with executable name
pico_add_extra_outputs(spatial_audio_program)