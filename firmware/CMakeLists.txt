cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/components/moskill_common
    ${CMAKE_CURRENT_SOURCE_DIR}/components/kill_detector
    ${CMAKE_CURRENT_SOURCE_DIR}/components/stats_engine
    ${CMAKE_CURRENT_SOURCE_DIR}/components/ble_service
    ${CMAKE_CURRENT_SOURCE_DIR}/components/env_sensor
    ${CMAKE_CURRENT_SOURCE_DIR}/components/power_mgmt
    ${CMAKE_CURRENT_SOURCE_DIR}/components/hv_driver
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(moskill)
