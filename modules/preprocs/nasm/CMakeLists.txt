add_executable(genversion preprocs/nasm/genversion.c)
if(CMAKE_CROSSCOMPILING)
    find_program(GENVERSION_BIN NAMES genversion REQUIRED)
    add_executable(yasm::genversion IMPORTED GLOBAL)
    set_property(TARGET yasm::genversion PROPERTY IMPORTED_LOCATION "${GENVERSION_BIN}")
else()
    add_executable(yasm::genversion ALIAS genversion)
endif()

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.mac
    COMMAND yasm::genversion ${CMAKE_CURRENT_BINARY_DIR}/version.mac
    )

YASM_GENMACRO(
    ${CMAKE_CURRENT_BINARY_DIR}/version.mac
    ${CMAKE_CURRENT_BINARY_DIR}/nasm-version.c
    nasm_version_mac
    )

SET_SOURCE_FILES_PROPERTIES(preprocs/nasm/nasm-preproc.c PROPERTIES
    OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/nasm-version.c
    )

YASM_ADD_MODULE(preproc_nasm
    preprocs/nasm/nasm-preproc.c
    preprocs/nasm/nasm-pp.c
    preprocs/nasm/nasmlib.c
    preprocs/nasm/nasm-eval.c
    )

