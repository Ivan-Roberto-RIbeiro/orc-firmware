target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cs47l63_comm.c
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/cs47l63/cs47l63.c
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/cs47l63/cs47l63_ext.c
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/cs47l63/generated/cs47l63_syscfg_regs.c
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/common/regmap.c
)

zephyr_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/cs47l63
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/cs47l63/config
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/cs47l63/generated
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/cs47l63/bsp
    ${ZEPHYR_BASE}/../modules/hal/cirrus-logic/common
)