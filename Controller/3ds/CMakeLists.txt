cmake_minimum_required(VERSION 3.13)

project(3ds-remote VERSION 1.0.0)

if(NOT NINTENDO_3DS)
    message(FATAL_ERROR "This project must be built with the 3DS toolchain.\n"
        "Run: arm-none-eabi-cmake -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake ..")
endif()

add_executable(3ds-remote)

target_compile_features(3ds-remote PRIVATE cxx_std_17)

target_compile_definitions(3ds-remote PRIVATE CLASSIC NDEBUG)

target_compile_options(3ds-remote PRIVATE -Wall -Wextra)

target_include_directories(3ds-remote PRIVATE include)

target_sources(3ds-remote PRIVATE
    include/log.h
    include/platform.h
    source/log.cpp
    source/platform.cpp
    source/main.cpp
)

# --------------------------------------------------------------------------
# 3DSX (homebrew launcher)
# --------------------------------------------------------------------------

ctr_generate_smdh(3ds-remote.smdh
    NAME        "3DS Remote"
    DESCRIPTION "v${PROJECT_VERSION} - Wireless controller"
    AUTHOR      "mjw"
    ICON        ${CMAKE_CURRENT_SOURCE_DIR}/meta/icon.png
)

ctr_create_3dsx(3ds-remote
    SMDH 3ds-remote.smdh
)

# --------------------------------------------------------------------------
# CIA (installable title) — requires bannertool and makerom on PATH
# --------------------------------------------------------------------------

find_program(BANNERTOOL bannertool)
find_program(MAKEROM    makerom)

if(BANNERTOOL AND MAKEROM)
    set(META_DIR ${CMAKE_CURRENT_SOURCE_DIR}/meta)
    set(CIA_DIR  ${CMAKE_CURRENT_BINARY_DIR})

    add_custom_command(
        OUTPUT ${CIA_DIR}/banner.bin
        COMMAND ${BANNERTOOL} makebanner
            -i ${META_DIR}/banner.png
            -a ${META_DIR}/audio.wav
            -o ${CIA_DIR}/banner.bin
        DEPENDS ${META_DIR}/banner.png ${META_DIR}/audio.wav
        COMMENT "Building CIA banner"
    )

    add_custom_command(
        OUTPUT ${CIA_DIR}/icon.bin
        COMMAND ${BANNERTOOL} makesmdh
            -s "3DS Remote"
            -l "3DS Remote Controller"
            -p "mjw"
            -i ${META_DIR}/icon.png
            -o ${CIA_DIR}/icon.bin
        DEPENDS ${META_DIR}/icon.png
        COMMENT "Building CIA icon"
    )

    add_custom_target(3ds-remote.cia
        COMMAND ${MAKEROM} -f cia
            -o ${CIA_DIR}/3ds-remote.cia
            -rsf ${META_DIR}/cia.rsf
            -elf ${CIA_DIR}/3ds-remote.elf
            -icon  ${CIA_DIR}/icon.bin
            -banner ${CIA_DIR}/banner.bin
        DEPENDS 3ds-remote ${CIA_DIR}/banner.bin ${CIA_DIR}/icon.bin
        COMMENT "Building CIA package"
    )
else()
    if(NOT BANNERTOOL)
        message(STATUS "bannertool not found — CIA target disabled")
    endif()
    if(NOT MAKEROM)
        message(STATUS "makerom not found — CIA target disabled")
    endif()
endif()
