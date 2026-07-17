add_executable(genmacro
    genmacro.c
    )

if(CMAKE_CROSSCOMPILING)
    find_program(GENMACRO_BIN NAMES genmacro REQUIRED)
    add_executable(yasm::genmacro IMPORTED GLOBAL)
    set_property(TARGET yasm::genmacro PROPERTY IMPORTED_LOCATION "${GENMACRO_BIN}")
else()
    add_executable(yasm::genmacro ALIAS genmacro)
endif()
