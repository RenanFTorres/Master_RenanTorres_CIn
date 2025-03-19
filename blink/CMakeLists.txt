# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# COMENTADO EM 28/11/24
# set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/led_strip)
# # This example uses an extra component for common functions such as Wi-Fi and Ethernet connection.
# set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

set(EXTRA_COMPONENT_DIRS
    $ENV{IDF_PATH}/examples/common_components/led_strip
    $ENV{IDF_PATH}/examples/common_components/protocol_examples_common
    $ENV{IDF_PATH}/components/spiffs
)


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(blink)
