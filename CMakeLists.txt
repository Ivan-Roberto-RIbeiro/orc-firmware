cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(tiresias-sensor-hub)

target_sources(app PRIVATE src/main.c)

zephyr_library_include_directories(
    include
    src/bluetooth
    src/modules
    src/drivers
    src/utils
    src/utils/macros
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/cs47l63
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/cs47l63/config
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/cs47l63/generated
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/cs47l63/bsp
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/common
)

add_subdirectory(src/bluetooth)
add_subdirectory(src/modules)
add_subdirectory(src/drivers)
add_subdirectory(src/utils)