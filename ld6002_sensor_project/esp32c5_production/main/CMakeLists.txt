# Main component CMakeLists.txt
idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        nvs_flash
        ld6002_sensor
)
