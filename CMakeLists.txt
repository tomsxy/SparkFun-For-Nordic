cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sparkfun_icm20948_examples C)

set(ICM20948_EXAMPLE "quat6" CACHE STRING
    "要构建的示例")
set(ICM20948_EXAMPLES quat6 quat9 raw_accel multiple_sensors)
set_property(CACHE ICM20948_EXAMPLE PROPERTY STRINGS ${ICM20948_EXAMPLES})

if(NOT "${ICM20948_EXAMPLE}" IN_LIST ICM20948_EXAMPLES)
    message(FATAL_ERROR
        "未知示例：${ICM20948_EXAMPLE}；可选值：${ICM20948_EXAMPLES}")
endif()

target_sources(app PRIVATE
    examples/${ICM20948_EXAMPLE}/main.c
    icm20948_port.c
    icm20948_dmp_init.c
    third_party/icm20948/ICM_20948_C.c
)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    third_party/icm20948
)

# DMP 固件约占 14 KiB，只在明确需要 DMP 的应用中启用。
target_compile_definitions(app PRIVATE ICM_20948_USE_DMP=1)
