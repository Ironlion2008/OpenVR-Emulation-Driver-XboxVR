cmake_minimum_required(VERSION 3.16)

project("OpenVR_Emulation_Driver")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# This driver has no vcpkg dependency. Disable VS AppLocal integration so
# the build stays independent of optional vcpkg tooling.
set(CMAKE_VS_GLOBALS "VcpkgEnabled=false")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PROCESSOR_ARCH "64")
else()
    set(PROCESSOR_ARCH "32")
endif()

if(WIN32)
    set(PLATFORM_NAME "win")
else()
    message(FATAL_ERROR "OpenVR-Emulation-Driver-Xbox360 is Windows-only.")
endif()

# ------------------------------------------------------------
# OpenVR SDK
# ------------------------------------------------------------
# The driver only needs the OpenVR SDK. No Conan dependency is
# required to build the driver.
#
# CI passes -DOPENVR_ROOT=<workspace>/openvr-sdk.
# Local builds may use libraries/openvr after running
# scripts/fetch_openvr.ps1.
# ------------------------------------------------------------
if(DEFINED OPENVR_ROOT)
    set(OPENVR_INCLUDE_DIR "${OPENVR_ROOT}/headers")
    set(OPENVR_LIB_DIR "${OPENVR_ROOT}/lib/win${PROCESSOR_ARCH}")
else()
    set(OPENVR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libraries/openvr" CACHE PATH "OpenVR SDK root")
    set(OPENVR_INCLUDE_DIR "${OPENVR_ROOT}/headers")
    set(OPENVR_LIB_DIR "${OPENVR_ROOT}/lib/win${PROCESSOR_ARCH}")
endif()

if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr_driver.h")
    message(FATAL_ERROR
        "OpenVR headers not found: ${OPENVR_INCLUDE_DIR}/openvr_driver.h\n"
        "Run scripts\\fetch_openvr.ps1 or configure with -DOPENVR_ROOT=<OpenVR SDK>.")
endif()

if(NOT DEFINED OPENVR_LIB)
    find_library(OPENVR_LIB
        NAMES openvr_api
        HINTS "${OPENVR_LIB_DIR}"
        NO_DEFAULT_PATH)
endif()

if(NOT OPENVR_LIB)
    message(FATAL_ERROR
        "OpenVR library openvr_api.lib not found in ${OPENVR_LIB_DIR}.")
endif()

message(STATUS "OpenVR include: ${OPENVR_INCLUDE_DIR}")
message(STATUS "OpenVR library: ${OPENVR_LIB}")

# ------------------------------------------------------------
# Driver
# ------------------------------------------------------------
set(DRIVER_NAME "openvr-emulator")
set(DRIVER_PROJECT "driver_${DRIVER_NAME}")

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src/*.hpp")
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src/*.cpp")

add_library("${DRIVER_PROJECT}" SHARED ${HEADERS} ${SOURCES})

target_include_directories("${DRIVER_PROJECT}" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src")
target_include_directories("${DRIVER_PROJECT}" SYSTEM PRIVATE
    "${OPENVR_INCLUDE_DIR}")

target_link_libraries("${DRIVER_PROJECT}" PRIVATE
    "${OPENVR_LIB}"
    xinput)

target_compile_features("${DRIVER_PROJECT}" PRIVATE cxx_std_17)

# Windows defines min/max macros unless NOMINMAX is set. The driver uses
# std::min/std::max and DirectXMath, so make the Windows header environment
# deterministic for every source file.
target_compile_definitions("${DRIVER_PROJECT}" PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)
set_target_properties("${DRIVER_PROJECT}" PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options("${DRIVER_PROJECT}" PRIVATE
        /W4
        /permissive-
        /external:W0)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src"
    PREFIX "Source Files" FILES ${SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src"
    PREFIX "Header Files" FILES ${HEADERS})

# ------------------------------------------------------------
# Optional preview app
# ------------------------------------------------------------
option(BUILD_PREVIEW_APP "Build the preview application" OFF)
if(BUILD_PREVIEW_APP)
    add_subdirectory(preview_app)
endif()

# ------------------------------------------------------------
# Optional tests
# ------------------------------------------------------------
# Kept separate from the driver build so a missing test dependency can
# never block the actual Windows driver compilation.
option(BUILD_TESTING "Build GTest/GMock unit tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# ------------------------------------------------------------
# Driver resource copy / optional deployment
# ------------------------------------------------------------
add_custom_command(TARGET ${DRIVER_PROJECT} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/driver_files/driver/"
        "$<TARGET_FILE_DIR:${DRIVER_PROJECT}>")

add_custom_command(TARGET ${DRIVER_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:${DRIVER_PROJECT}>"
        "$<TARGET_FILE_DIR:${DRIVER_PROJECT}>/${DRIVER_NAME}/bin/${PLATFORM_NAME}${PROCESSOR_ARCH}/$<TARGET_FILE_NAME:${DRIVER_PROJECT}>")

option(OPENVR_EMULATOR_SKIP_DEPLOY
    "Skip automatic deployment to the local SteamVR drivers folder" ON)

if(NOT OPENVR_EMULATOR_SKIP_DEPLOY)
    set(STEAMVR_DRIVERS_DIR
        "C:/Program Files (x86)/Steam/steamapps/common/SteamVR/drivers")

    add_custom_command(TARGET ${DRIVER_PROJECT} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDRIVER_NAME=${DRIVER_NAME}
            -DSRC_DIR=$<TARGET_FILE_DIR:${DRIVER_PROJECT}>
            -DDST_DIR=${STEAMVR_DRIVERS_DIR}
            -P "${CMAKE_SOURCE_DIR}/scripts/deploy_driver.cmake"
        COMMENT "Deploying ${DRIVER_NAME} driver to SteamVR")
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT "${DRIVER_PROJECT}")

set_target_properties("${DRIVER_PROJECT}" PROPERTIES
    VS_DEBUGGER_COMMAND
    "C:/Program Files (x86)/Steam/steamapps/common/SteamVR/bin/win64/vrstartup.exe")
