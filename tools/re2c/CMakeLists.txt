add_executable(re2c
    main.c
    code.c
    dfa.c
    parser.c
    actions.c
    scanner.c
    mbo_getopt.c
    substr.c
    translate.c
    )

if(CMAKE_CROSSCOMPILING)
    find_program(RE2C_BIN NAMES re2c REQUIRED)
    add_executable(yasm::re2c IMPORTED GLOBAL)
    set_property(TARGET yasm::re2c PROPERTY IMPORTED_LOCATION "${RE2C_BIN}")
else()
    add_executable(yasm::re2c ALIAS re2c)
endif()
