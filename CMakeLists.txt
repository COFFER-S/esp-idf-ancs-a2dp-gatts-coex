cmake_minimum_required(VERSION 3.5)

# Set target chip
set(IDF_TARGET esp32)

# Include project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ancs_a2dp_gatts_coex)