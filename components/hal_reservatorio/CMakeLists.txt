idf_component_register(
    SRCS
        "driver_indicador.cpp"
        "driver_atuacao.cpp"
        "driver_ultrassom.cpp"
        "driver_botoes.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES
        esp_driver_gpio
        esp_driver_ledc
        esp_timer
)