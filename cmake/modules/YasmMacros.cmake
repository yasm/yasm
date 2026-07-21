# Portions based on kdelibs KDE4Macros.cmake:
#
# Copyright (c) 2006, 2007, Alexander Neundorf, <neundorf@kde.org>
# Copyright (c) 2006, 2007, Laurent Montel, <montel@kde.org>
# Copyright (c) 2007 Matthias Kretz <kretz@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
#
# Changes for Yasm Copyright (c) 2007 Peter Johnson

include(CMakeParseArguments)

# add a unit test, which is executed when running make test
# it will be built with RPATH pointing to the build dir
# The targets are always created, but only built for the "all"
# target if the option YASM_BUILD_TESTS is enabled. Otherwise the rules for
# the target are created but not built by default. You can build them by
# manually building the target.
# The name of the target can be specified using TESTNAME <testname>, if it is
# not given the macro will default to the <name>
macro (YASM_ADD_UNIT_TEST _test_NAME)
    set(_srcList ${ARGN})
    set(_targetName ${_test_NAME})
    if( ${ARGV1} STREQUAL "TESTNAME" )
        set(_targetName ${ARGV2})
        LIST(REMOVE_AT _srcList 0 1)
    endif( ${ARGV1} STREQUAL "TESTNAME" )
    yasm_add_test_executable( ${_test_NAME} ${_srcList} )
    add_test( NAME ${_targetName} COMMAND ${_targetName} )
endmacro (YASM_ADD_UNIT_TEST)

# add an test executable
# it will be built with RPATH pointing to the build dir
# The targets are always created, but only built for the "all"
# target if the option YASM_BUILD_TESTS is enabled. Otherwise the rules for
# the target are created but not built by default. You can build them by
# manually building the target.
macro (YASM_ADD_TEST_EXECUTABLE _target_NAME)

   set(_add_executable_param)

   if (NOT YASM_BUILD_TESTS)
      set(_add_executable_param EXCLUDE_FROM_ALL)
   endif (NOT YASM_BUILD_TESTS)

   set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

   set(_SRCS ${ARGN})
   add_executable(${_target_NAME} ${_add_executable_param} ${_SRCS})

   set_target_properties(${_target_NAME} PROPERTIES
                         SKIP_BUILD_RPATH FALSE
                         BUILD_WITH_INSTALL_RPATH FALSE)

endmacro (YASM_ADD_TEST_EXECUTABLE)

function(ADD_YASM_TESTS NAME DIRECTORY OBJ_SUFFIX)

    cmake_parse_arguments(ARG "MAPFILE" "" "ARGS" ${ARGN})

    set(results_dir "${CMAKE_CURRENT_BINARY_DIR}/results")
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${results_dir}")
    if(NOT IS_DIRECTORY "${DIRECTORY}")
        message(FATAL_ERROR "${DIRECTORY} does not exist")
    endif()
    file(GLOB TESTS_ASM_SOURCES RELATIVE "${DIRECTORY}" "${DIRECTORY}/*.asm")
    if(NOT TESTS_ASM_SOURCES)
        message(FATAL_ERROR "No assembly sources found in ${DIRECTORY}")
    endif()
    set(extra_test_command)
    if(YASM_TEST_TRACE)
        list(APPEND extra_test_command "--trace-expand")
    endif()
    foreach(SOURCE ${TESTS_ASM_SOURCES})
        set(test_name ${NAME}_${SOURCE})
        add_test(NAME ${test_name}
            COMMAND "${CMAKE_COMMAND}"
                "-DTEST_MAPFILE=${ARG_MAPFILE}"
                "-DYASM_BIN=$<TARGET_FILE:yasm>"
                "-DYASM_ARGUMENTS=${ARG_ARGS}"
                "-DTEST_HD_BIN=$<TARGET_FILE:yasm::test_hd>"
                "-DINPUT_SOURCE=${DIRECTORY}/${SOURCE}"
                "-DOUTPUT_DIR=${results_dir}"
                "-DOUTPUT_SUFFIX=${OBJ_SUFFIX}"
                "-P" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/YasmTestDriver.cmake"
                ${extra_test_command}
            )
        set_property(TEST ${test_name} APPEND PROPERTY LABEL ${NAME})
        set_property(TEST ${test_name} PROPERTY TIMEOUT 1)
        set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT YASM_TEST_SUITE=)
    endforeach()
endfunction()

macro (YASM_ADD_MODULE _module_NAME)
    list(APPEND YASM_MODULES_SRC ${ARGN})
    list(APPEND YASM_MODULES ${_module_NAME})
endmacro (YASM_ADD_MODULE)

macro (YASM_GENPERF _in_NAME _out_NAME)
    add_custom_command(
        OUTPUT ${_out_NAME}
        COMMAND yasm::genperf ${_in_NAME} ${_out_NAME}
        MAIN_DEPENDENCY ${_in_NAME}
        )
endmacro (YASM_GENPERF)

macro (YASM_RE2C _in_NAME _out_NAME)
    add_custom_command(
        OUTPUT ${_out_NAME}
        COMMAND yasm::re2c ${ARGN} -o ${_out_NAME} ${_in_NAME}
        )
endmacro (YASM_RE2C)

macro (YASM_GENMACRO _in_NAME _out_NAME _var_NAME)
    add_custom_command(
        OUTPUT ${_out_NAME}
        COMMAND yasm::genmacro ${_out_NAME} ${_var_NAME} ${_in_NAME}
        MAIN_DEPENDENCY ${_in_NAME}
        )
endmacro (YASM_GENMACRO)

