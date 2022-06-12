cmake_minimum_required(VERSION 3.20)
project(PzemMonitor C)

set(CMAKE_C_STANDARD 11)

include_directories(Source)
include_directories(zstack-lib
        ../Source
        ../../ZMain/TI2530DB
        ../../../../Components/hal/include
        ../../../../Components/hal/target/CC2530EB
        ../../../../Components/mac/include
        ../../../../Components/mac/high_level
        ../../../../Components/mac/low_level/srf04
        ../../../../Components/mac/low_level/srf04/single_chip
        ../../../../Components/mt
        ../../../../Components/osal/include
        ../../../../Components/services/saddr
        ../../../../Components/services/sdata
        ../../../../Components/stack/af
        ../../../../Components/stack/bdb
        ../../../../Components/stack/gp
        ../../../../Components/stack/nwk
        ../../../../Components/stack/sapi
        ../../../../Components/stack/sec
        ../../../../Components/stack/sys
        ../../../../Components/stack/zcl
        ../../../../Components/stack/zdo
        ../../../../Components/zmac
        ../../../../Components/zmac/f8w
        "E:/zp/IAR Systems/Embedded Workbench 8.1/8051/inc"
        )

add_definitions(-D__IAR_SYSTEMS_ICC__ -DHAL_BOARD_CHDTECH_DEV)
add_definitions(-include preinclude.h)

file(GLOB_RECURSE SOURCES
        "Source/*.*"
        "../../../../Components/*.*"
        "../../../../Components/hal/include/*.*"
        "../../../../Components/hal/target/CC2530EB/*.*"
        "../../../../Components/mac/include/*.*"
        "../../../../Components/mac/high_level/*.*"
        "../../../../Components/mac/low_level/srf04/*.*"
        "../../../../Components/mac/low_level/srf04/single_chip/*.*"
        "../../../../Components/mt/*.*"
        "../../../../Components/osal/include/*.*"
        "../../../../Components/services/saddr/*.*"
        "../../../../Components/services/sdata/*.*"
        "../../../../Components/stack/af/*.*"
        "../../../../Components/stack/bdb/*.*"
        "../../../../Components/stack/gp/*.*"
        "../../../../Components/stack/nwk/*.*"
        "../../../../Components/stack/sapi/*.*"
        "../../../../Components/stack/sec/*.*"
        "../../../../Components/stack/sys/*.*"
        "../../../../Components/stack/zcl/*.*"
        "../../../../Components/stack/zdo/*.*"
        "../../../../Components/zmac/*.*"
        "../../../../Components/zmac/f8w/*.*"
        )

add_executable(PzemMonitor
        Source/preinclude.h
        Source/zcl_app.c
        Source/pzem.c
        Source/hal_lcd.c
        Source/zcl_app_data.c
        Source/OSAL_App.c
        zstack-lib/battery.h
        zstack-lib/bettery.c
        zstack-lib/commissioning.c
        zstack-lib/commissioning.h
        zstack-lib/Debug.c
        zstack-lib/Debug.h
        zstack-lib/factory_reset.c
        zstack-lib/factory_reset.h
        zstack-lib/hal_i2c.c
        zstack-lib/hal_i2c.h
        zstack-lib/hal_key.c
        zstack-lib/hal_key.h
        zstack-lib/stdint.h
        zstack-lib/tl_resetter.c
        zstack-lib/tl_resetter.h
        zstack-lib/utils.c
        zstack-lib/utils.h
        zstack-lib/hal_lcd.c)
