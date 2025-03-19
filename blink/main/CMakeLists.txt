idf_component_register(
    SRCS "blink_example_main.c" "buffer_circ.c"  # Liste todos os arquivos de origem
    INCLUDE_DIRS "." "include"                   # Liste todos os diretórios de inclusão
    REQUIRES spiffs nvs_flash driver esp_adc esp_event protocol_examples_common esp_http_server led_strip fatfs vfs spiffs    # Adicione dependências, se necessário
)

# idf_component_register(SRCS "blink_example_main.c"
#                    INCLUDE_DIRS ".")

#add_library(${buffer_circ} STATIC $ENV{IDF_PATH}/examples/components/buffer_circ/include)
#add_library(${buffer_circ} UNKNOWN $C:/esp/esp_idf/examples/components/buffer_circ/include)
#add_library( buffer_circ STATIC IMPORTED [ GLOBAL ] )
#component_add_include_dirs(${buffer_circ} "${__INCLUDE_DIRS}" PUBLIC)


# COMENTADO EM 28/11/24
# idf_component_register(SRCS "buffer_circ.c"
#                       INCLUDE_DIRS "include"
#                       REQUIRES spiffs)

component_compile_options(-Wno-error=format= -Wno-format)