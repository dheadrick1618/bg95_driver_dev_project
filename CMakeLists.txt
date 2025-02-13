# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VERSION "0.1.0")

if(TEST_MODE)
   set(PROJECT_NAME "bg95_driver_test")
# this tells cmake to build the test directory INSTEAD of the main dir 
# only one of either can be built as they both feature an 'app main' 
   set(EXTRA_COMPONENT_DIRS "test")
   set(COMPONENTS "test")

else()
   set(PROJECT_NAME "bg95_driver_main")
   set(COMPONENTS "main")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(${PROJECT_NAME})
