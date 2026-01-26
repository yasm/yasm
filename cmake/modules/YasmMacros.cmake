# Portions based on kdelibs KDE4Macros.cmake:
#
# Copyright (c) 2006, 2007, Alexander Neundorf
# Copyright (c) 2006, 2007, Laurent Montel
# Copyright (c) 2007 Matthias Kretz
# Changes for Yasm Copyright (c) 2007 Peter Johnson
#
# Modernized for CMake ≥ 3.5

# -------------------------
# Unit tests
# -------------------------

macro(YASM_ADD_UNIT_TEST _test_NAME)
    set(_srcList ${ARGN})
    set(_targetName ${_test_NAME})

    if(${ARGV1} STREQUAL "TESTNAME")
        set(_targetName ${ARGV2})
        list(REMOVE_AT _srcList 0 1)
    endif()

    yasm_add_test_executable(${_test_NAME} ${_srcList})
    add_test(NAME ${_targetName} COMMAND $<TARGET_FILE:${_test_NAME}>)
endmacro()

macro(YASM_ADD_TEST_EXECUTABLE _target_NAME)
    set(_add_executable_param)

    if(NOT YASM_BUILD_TESTS)
        set(_add_executable_param EXCLUDE_FROM_ALL)
    endif()

    set(_SRCS ${ARGN})
    add_executable(${_target_NAME} ${_add_executable_param} ${_SRCS})

    set_target_properties(${_target_NAME} PROPERTIES
        SKIP_BUILD_RPATH FALSE
        BUILD_WITH_INSTALL_RPATH FALSE
    )
endmacro()

# -------------------------
# Module list
# -------------------------
macro(YASM_ADD_MODULE _module_NAME)
    list(APPEND YASM_MODULES_SRC ${ARGN})
    list(APPEND YASM_MODULES ${_module_NAME})
endmacro()

# -------------------------
# Code generators
# -------------------------
macro(YASM_GENPERF _in_NAME _out_NAME)
    add_custom_command(
        OUTPUT ${_out_NAME}
        COMMAND $<TARGET_FILE:genperf> ${_in_NAME} ${_out_NAME}
        DEPENDS genperf
        MAIN_DEPENDENCY ${_in_NAME}
    )
endmacro()

macro(YASM_RE2C _in_NAME _out_NAME)
    add_custom_command(
        OUTPUT ${_out_NAME}
        COMMAND $<TARGET_FILE:re2c> ${ARGN} -o ${_out_NAME} ${_in_NAME}
        DEPENDS re2c
        MAIN_DEPENDENCY ${_in_NAME}
    )
endmacro()

macro(YASM_GENMACRO _in_NAME _out_NAME _var_NAME)
    add_custom_command(
        OUTPUT ${_out_NAME}
        COMMAND $<TARGET_FILE:genmacro> ${_out_NAME} ${_var_NAME} ${_in_NAME}
        DEPENDS genmacro
        MAIN_DEPENDENCY ${_in_NAME}
    )
endmacro()
