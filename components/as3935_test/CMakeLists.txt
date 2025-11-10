idf_component_register(SRCS "test_as3935.c" "test_validate_sync.c" "test_validate_success.c" "test_web_index.c"
                       INCLUDE_DIRS "../main/include" "../main"
                       REQUIRES unity nvs_flash)
