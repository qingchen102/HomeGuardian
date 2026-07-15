# LD6002 Sensor Component CMakeLists.txt
idf_component_register(
    SRCS
        "ld6002.c"
        "tinyframe.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        driver
        esp_timer
        freertos
        esp_log
)
