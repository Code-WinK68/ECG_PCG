idf_component_register(SRCS "main.c"
                            "ecg.c"
                            "pcg.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_adc driver esp_timer)
