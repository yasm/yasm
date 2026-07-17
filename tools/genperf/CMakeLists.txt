add_executable(genperf
    genperf.c
    perfect.c
    ../../libyasm/phash.c
    ../../libyasm/xmalloc.c
    ../../libyasm/xstrdup.c
    )
target_compile_definitions(genperf PRIVATE YASM_LIB_DECL=)

if(CMAKE_CROSSCOMPILING)
    find_program(GENPERF_BIN NAMES genperf REQUIRED)
    add_executable(yasm::genperf IMPORTED GLOBAL)
    set_property(TARGET yasm::genperf PROPERTY IMPORTED_LOCATION "${GENPERF_BIN}")
else()
    add_executable(yasm::genperf ALIAS genperf)
endif()
