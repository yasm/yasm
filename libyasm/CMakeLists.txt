SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

ADD_LIBRARY(libyasm
    assocdat.c
    bitvect.c
    bc-align.c
    bc-data.c
    bc-incbin.c
    bc-org.c
    bc-reserve.c
    bytecode.c
    cmake-module.c
    errwarn.c
    expr.c
    file.c
    floatnum.c
    hamt.c
    insn.c
    intnum.c
    inttree.c
    linemap.c
    md5.c
    mergesort.c
    phash.c
    section.c
    strcasecmp.c
    strsep.c
    symrec.c
    valparam.c
    value.c
    xmalloc.c
    xstrdup.c
    )
IF(BUILD_SHARED_LIBS)
    SET_TARGET_PROPERTIES(libyasm PROPERTIES
        OUTPUT_NAME "yasm"
        DEFINE_SYMBOL "YASM_LIB_SOURCE"
        )
    if(WIN32)
        SET_PROPERTY(TARGET libyasm PROPERTY PREFIX "")
    endif()
ELSE(BUILD_SHARED_LIBS)
    TARGET_COMPILE_DEFINITIONS(libyasm PUBLIC LIBYASM_STATIC)
ENDIF(BUILD_SHARED_LIBS)

INSTALL(TARGETS libyasm
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    )

INSTALL(FILES
    arch.h
    assocdat.h
    bitvect.h
    bytecode.h
    compat-queue.h
    coretype.h
    dbgfmt.h
    errwarn.h
    expr.h
    file.h
    floatnum.h
    hamt.h
    insn.h
    intnum.h
    inttree.h
    libyasm_decl.h
    linemap.h
    listfmt.h
    md5.h
    module.h
    objfmt.h
    parser.h
    phash.h
    preproc.h
    section.h
    symrec.h
    valparam.h
    value.h
    DESTINATION include/libyasm
    )
