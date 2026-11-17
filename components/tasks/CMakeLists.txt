idf_component_register(
    SRCS
        "task_sensores.cpp"
        "task_controle.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        hal_reservatorio
        application
        freertos
        esp_timer
)

