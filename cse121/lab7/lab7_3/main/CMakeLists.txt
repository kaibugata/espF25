idf_component_register(SRCS "post_part3_main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_driver_i2c esp_driver_gpio esp_timer esp_wifi nvs_flash
		    )

