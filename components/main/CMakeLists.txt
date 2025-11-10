set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Generate web_index.h from web/index.html at CMake configure time. Using
# execute_process here avoids non-scriptable add_custom_command issues when
# ESP-IDF inspects component CMake files.
set(WEB_HTML "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html")
set(WEB_HEADER "${GENERATED_DIR}/web_index.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})
if(NOT DEFINED Python3_EXECUTABLE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/embed_web.py ${WEB_HTML} ${WEB_HEADER}
    RESULT_VARIABLE _embed_result
    OUTPUT_VARIABLE _embed_out
    ERROR_VARIABLE _embed_err
)
if(NOT _embed_result EQUAL 0)
    message(WARNING "embed_web.py failed: ${_embed_err}")
else()
    message(STATUS "Generated web header: ${WEB_HEADER}")
endif()

# Register component and include generated directory
idf_component_register(SRCS "ota.c" "events.c" "app_main.c" "as3935.c" "web_files.c" "settings.c" "mqtt_client.c" "wifi_prov.c" "http_helpers.c"
                                             INCLUDE_DIRS "include" "${GENERATED_DIR}")
                       
