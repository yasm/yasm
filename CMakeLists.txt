cmake_minimum_required(VERSION 3.5)

project(yasm LANGUAGES C CXX)

if(POLICY CMP0003)
    cmake_policy(SET CMP0003 NEW)
endif()

include(GNUInstallDirs)

if(UNIX AND NOT APPLE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)

# Where to look first for cmake modules
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

INCLUDE(YasmMacros)
INCLUDE(VersionGen)

OPTION(ENABLE_NLS "Enable message translations" OFF)
OPTION(YASM_BUILD_TESTS "Enable building of tests" ON)

IF(YASM_BUILD_TESTS)
    ENABLE_TESTING()
ENDIF()

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
ENDIF()

VERSION_GEN(PACKAGE_VERSION "${CMAKE_BINARY_DIR}/YASM-VERSION-FILE" "1.3.0")

set(PACKAGE_STRING "yasm ${PACKAGE_VERSION}")

INCLUDE_DIRECTORIES(AFTER
    ${CMAKE_BINARY_DIR}
    ${yasm_SOURCE_DIR}
)

INCLUDE(ConfigureChecks.cmake)

ADD_SUBDIRECTORY(tools)
ADD_SUBDIRECTORY(libyasm)
ADD_SUBDIRECTORY(modules)
ADD_SUBDIRECTORY(frontends)

INSTALL(FILES
    libyasm.h
    ${CMAKE_BINARY_DIR}/libyasm-stdint.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
