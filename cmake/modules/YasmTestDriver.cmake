# Inputs:
# - [required] YASM_BIN: path of yasm binary
# - [optional] YASM_ARGUMENTS: additional arguments
# - [required] TEST_HD_BIN: path of test_hd binary
# - [required] INPUT_SOURCE: path of input assembly source
# - [required] OUTPUT_DIR: path of output directory
# - [required] OUTPUT_SUFFIX: suffix of output binary
# - [required] TEST_MAPFILE: add and verify --mapfile=

if(NOT DEFINED YASM_BIN)
    message(FATAL_ERROR "YASM_BIN has not been defined")
endif()
if(NOT EXISTS "${YASM_BIN}")
    message(FATAL_ERROR "YASM_BIN=${YASM_BIN} does not exist")
endif()

if(NOT DEFINED TEST_HD_BIN)
    message(FATAL_ERROR "TEST_HD_BIN has not been defined")
endif()
if(NOT EXISTS "${TEST_HD_BIN}")
    message(FATAL_ERROR "TEST_HD_BIN=${TEST_HD_BIN} does not exist")
endif()

if(NOT DEFINED INPUT_SOURCE)
    message(FATAL_ERROR "INPUT_SOURCE has not been defined")
endif()
if(NOT EXISTS "${INPUT_SOURCE}")
    message(FATAL_ERROR "INPUT_SOURCE=${INPUT_SOURCE} does not exist")
endif()

if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "INPUT_SOURCE has not been defined")
endif()
if(NOT IS_DIRECTORY "${OUTPUT_DIR}")
    message(FATAL_ERROR "INPUT_SOURCE=${INPUT_SOURCE} does not exist")
endif()

if(NOT DEFINED OUTPUT_SUFFIX)
    message(FATAL_ERROR "OUTPUT_SUFFIX has not been defined")
endif()

get_filename_component(yasm_dir "${YASM_BIN}" DIRECTORY)
get_filename_component(yasm_name "${YASM_BIN}" NAME)
get_filename_component(input_name_we "${INPUT_SOURCE}" NAME_WE)
get_filename_component(input_dir "${INPUT_SOURCE}" DIRECTORY)
set(file_out "${OUTPUT_DIR}/${input_name_we}${OUTPUT_SUFFIX}")
set(file_outhex "${OUTPUT_DIR}/${input_name_we}.hx")
set(file_outhex_ref "${input_dir}/${input_name_we}.hex")
set(file_stderr "${OUTPUT_DIR}/${input_name_we}.ew")
set(file_stderr_ref "${input_dir}/${input_name_we}.errwarn")
set(file_mapfile "${OUTPUT_DIR}/${input_name_we}.map")
set(file_mapfile_ref "${input_dir}/${input_name_we}.map")

set(yasm_command "./${yasm_name}")
list(APPEND yasm_command ${YASM_ARGUMENTS})
if(TEST_MAPFILE)
    list(APPEND yasm_command "--mapfile=${file_mapfile}")
endif()
list(APPEND yasm_command -o "${file_out}" -)

message(STATUS "Input: ${INPUT_SOURCE}")

string(REPLACE ";" " " command_string "${yasm_command}")
message(STATUS "Yasm command:      ${command_string}")
message(STATUS "Working directory: ${yasm_dir}")
execute_process(
    COMMAND ${yasm_command}
    RESULT_VARIABLE yasm_result
    WORKING_DIRECTORY "${yasm_dir}"
    INPUT_FILE "${INPUT_SOURCE}"
    ERROR_FILE "${file_stderr}"
    )

message(STATUS "Result:            ${yasm_result}")

if(yasm_result GREATER 128)
    message(FATAL_ERROR "FAIL: yasm crashed ($? > 128)")
elseif(yasm_result GREATER 0)
    if (NOT text_stderr MATCHES ".*err.*")
        # YASM detected errors but shouldn't have!
        message(FATAL_ERROR "FAIL: yasm returned a code > 0 without emiting a error code")
    else()
        # We got errors, check to see if they match
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol "${file_stderr}" "${file_stderr_ref}"
            RESULT_VARIABLE errwarn_diff_result
            )
        if (NOT errwarn_diff_result EQUAL 0)
            message(FATAL_ERROR "FAIL: stderr output does not match expected\nOutput: ${file_stderr}\nReference: ${file_stderr_ref}")
        endif()
        message(STATUS "Error/warnings OK")
    endif()
else()
    # Convert yasm output binary to hexdump
    execute_process(
        COMMAND ${TEST_HD_BIN} "${file_out}"
            RESULT_VARIABLE test_hd_result
            OUTPUT_FILE "${file_outhex}"
#            COMMAND_ERROR_IS_FATAL ANY
        )
    if (NOT test_hd_result EQUAL 0)
        message(FATAL_ERROR "FAIL: ./test_hd failed")
    endif()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol "${file_outhex}" "${file_outhex_ref}"
        RESULT_VARIABLE hex_diff_result
    )

    # error/warnings
    if(NOT hex_diff_result EQUAL 0)
        message(FATAL_ERROR "FAIL: binary output does not match\n Output: ${file_outhex}\nReference: ${file_outhex_ref}")
    endif()
    message(STATUS "Binary output OK")

    if(TEST_MAPFILE)
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol "${file_mapfile}" "${file_mapfile_ref}"
            RESULT_VARIABLE errmap_diff_result
        )
        if (NOT errmap_diff_result EQUAL 0)
            message(FATAL_ERROR "FAIL: mapfile output does not match expected\nOutput: ${file_mapfile}\nReference: ${file_mapfile_ref}")
        endif()
        message(STATUS "Map file OK")
    endif()

    if(EXISTS "${file_stderr_ref}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol "${file_stderr}" "${file_stderr_ref}"
            RESULT_VARIABLE errwarn_diff_result
        )
        if (NOT errwarn_diff_result EQUAL 0)
            message(FATAL_ERROR "FAIL: stderr output does not match expected\nOutput: ${file_stderr}\nReference: ${file_stderr_ref}")
        endif()
        message(STATUS "Warnings OK")
    endif()
endif()

message(STATUS "OK")
