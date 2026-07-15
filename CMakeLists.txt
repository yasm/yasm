CMAKE_MINIMUM_REQUIRED(VERSION 2.4...4.0)
PROJECT(yasm)
if (COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif (COMMAND cmake_policy)

ENABLE_TESTING()

OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)

# Where to look first for cmake modules
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

INCLUDE(YasmMacros)
INCLUDE(VersionGen)

OPTION(ENABLE_NLS "Enable message translations" OFF)

OPTION(YASM_BUILD_TESTS "Enable building of tests" ON)

IF(YASM_BUILD_TESTS)
    ENABLE_TESTING()
ENDIF(YASM_BUILD_TESTS)

# Default build type to debug if not set
IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Debug CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

VERSION_GEN(PACKAGE_VERSION "${CMAKE_BINARY_DIR}/YASM-VERSION-FILE" "1.3.0")

set (PACKAGE_STRING "yasm ${PACKAGE_VERSION}")

INCLUDE_DIRECTORIES(AFTER ${CMAKE_BINARY_DIR} ${yasm_SOURCE_DIR})

INCLUDE(ConfigureChecks.cmake)

ADD_SUBDIRECTORY(tools)
ADD_SUBDIRECTORY(libyasm)
ADD_SUBDIRECTORY(modules)
ADD_SUBDIRECTORY(frontends)

INSTALL(FILES
    libyasm.h
    ${CMAKE_BINARY_DIR}/libyasm-stdint.h
    DESTINATION include
    )

add_executable(test_hd test_hd.c)
if(CMAKE_CROSSCOMPILING)
    find_program(TEST_HD_BIN NAMES test_hd REQUIRED)
    add_executable(yasm::test_hd IMPORTED GLOBAL)
    set_property(TARGET yasm::test_hd PROPERTY IMPORTED_LOCATION "${TEST_HD_BIN}")
else()
    add_executable(yasm::test_hd ALIAS test_hd)
endif()


ADD_YASM_TESTS(x86_test "${CMAKE_CURRENT_SOURCE_DIR}/modules/arch/x86/tests" "" ARGS -f bin -Wsegreg-in-64bit)
ADD_YASM_TESTS(x86_gas32 "${CMAKE_CURRENT_SOURCE_DIR}/modules/arch/x86/tests/gas32" ".o" ARGS -f elf32 -p gas)
ADD_YASM_TESTS(x86_gas64 "${CMAKE_CURRENT_SOURCE_DIR}/modules/arch/x86/tests/gas64" ".o" ARGS -f elf -m amd64 -p gas)
ADD_YASM_TESTS(lc3b-arch "${CMAKE_CURRENT_SOURCE_DIR}/modules/arch/lc3b/tests" "" ARGS -a lc3b -f bin)
ADD_YASM_TESTS(gas-compat-parser "${CMAKE_CURRENT_SOURCE_DIR}/modules/parsers/gas/tests" ".o" ARGS -f elf -p gas)
ADD_YASM_TESTS(gas-compat-bin "${CMAKE_CURRENT_SOURCE_DIR}/modules/parsers/gas/tests/bin" "" ARGS -f bin -p gas)
ADD_YASM_TESTS(nasm-compat-parser "${CMAKE_CURRENT_SOURCE_DIR}/modules/parsers/nasm/tests" "" ARGS -f bin)
ADD_YASM_TESTS(nasm-compat-worphan-parser "${CMAKE_CURRENT_SOURCE_DIR}/modules/parsers/nasm/tests/worphan" "" ARGS -Worphan-labels -f bin)
ADD_YASM_TESTS(tasm-compat-parser "${CMAKE_CURRENT_SOURCE_DIR}/modules/parsers/tasm/tests" "" ARGS -f bin -p tasm)
ADD_YASM_TESTS(tasm-compat-parser-exe "${CMAKE_CURRENT_SOURCE_DIR}/modules/parsers/tasm/tests/exe" "" ARGS -f dosexe -p tasm)
ADD_YASM_TESTS(tasmpp "${CMAKE_CURRENT_SOURCE_DIR}/modules/preprocs/tasm/tests" "" ARGS -f bin -p tasm)
ADD_YASM_TESTS(nasmpp "${CMAKE_CURRENT_SOURCE_DIR}/modules/preprocs/nasm/tests" "" ARGS -f bin)
ADD_YASM_TESTS(rawpp "${CMAKE_CURRENT_SOURCE_DIR}/modules/preprocs/raw/tests" "" ARGS -f bin -r raw)
ADD_YASM_TESTS(drawf2-dbgfmt-gen64 "${CMAKE_CURRENT_SOURCE_DIR}/modules/dbgfmts/dwarf2/tests/gen64" ".o" ARGS -f elf64 -g dwarf2)
ADD_YASM_TESTS(dwarf2-pass32 "${CMAKE_CURRENT_SOURCE_DIR}/modules/dbgfmts/dwarf2/tests/pass32" ".o" ARGS -f elf -p gas -g dwarf2)
ADD_YASM_TESTS(dwarf2-pass64 "${CMAKE_CURRENT_SOURCE_DIR}/modules/dbgfmts/dwarf2/tests/pass64" ".o" ARGS -f elf64 -p gas -g dwarf2)
ADD_YASM_TESTS(dwarf2-dbgfmt-passwin64 "${CMAKE_CURRENT_SOURCE_DIR}/modules/dbgfmts/dwarf2/tests/passwin64" ".o" ARGS -f win64 -p gas -g dwarf2)
ADD_YASM_TESTS(stabs-dgbfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/dbgfmts/stabs/tests" ".o" ARGS -f elf -g stabs)
ADD_YASM_TESTS(bin-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/bin/tests" "" ARGS -f bin)
ADD_YASM_TESTS(bin-multisect "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/bin/tests/multisect" "" MAPFILE ARGS -f bin)
ADD_YASM_TESTS(elf-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/elf/tests/" ".o" ARGS -f elf)
ADD_YASM_TESTS(elf-amd64-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/elf/tests/amd64" ".o" ARGS -m amd64 -f elf)
ADD_YASM_TESTS(elf-x32-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/elf/tests/x32" ".o" ARGS -f elf -m x32)
ADD_YASM_TESTS(elf-x86-gas-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/elf/tests/gas32" ".o" ARGS -f elf32 -p gas)
ADD_YASM_TESTS(elf-amd64-gas-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/elf/tests/gas64" ".o" ARGS -f elf64 -p gas)
ADD_YASM_TESTS(elf-x32-gas-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/elf/tests/gasx32" ".o" ARGS -f elf -m x32 -p gas)
ADD_YASM_TESTS(coff-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/coff/tests" ".o" ARGS -f coff)
ADD_YASM_TESTS(gas32-macho-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/macho/tests/gas32" ".o" ARGS -f macho32 -p gas)
ADD_YASM_TESTS(gas64-macho-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/macho/tests/gas64" ".o" ARGS -f macho64 -p gas)
ADD_YASM_TESTS(nasm32-macho-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/macho/tests/nasm32" ".o" ARGS -f macho32)
ADD_YASM_TESTS(nasm64-macho-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/macho/tests/nasm64" ".o" ARGS -f macho64)
ADD_YASM_TESTS(rdf-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/rdf/tests" ".rdf" ARGS -f rdf)
ADD_YASM_TESTS(win32-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/win32/tests" ".obj" ARGS -f win32)
ADD_YASM_TESTS(win32-objfmt-gas "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/win32/tests/gas" ".obj" ARGS -f win32 -p gas)
ADD_YASM_TESTS(win64-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/win64/tests" ".obj" ARGS -f win64)
ADD_YASM_TESTS(win64-objfmt-gas "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/win64/tests/gas" ".obj" ARGS -f win64 -p gas -r nasm)
ADD_YASM_TESTS(xdf-objfmt "${CMAKE_CURRENT_SOURCE_DIR}/modules/objfmts/xdf/tests" ".xdf" ARGS -f xdf)
