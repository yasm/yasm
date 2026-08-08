YASM_ADD_UNIT_TEST(bitvect_test bitvect_test.c)
TARGET_LINK_LIBRARIES(bitvect_test PRIVATE libyasm)

YASM_ADD_UNIT_TEST(floatnum_test floatnum_test.c)
TARGET_LINK_LIBRARIES(floatnum_test PRIVATE libyasm)

YASM_ADD_UNIT_TEST(leb128_test leb128_test.c)
TARGET_LINK_LIBRARIES(leb128_test PRIVATE libyasm)

YASM_ADD_UNIT_TEST(combpath_test combpath_test.c)
TARGET_LINK_LIBRARIES(combpath_test PRIVATE libyasm)

YASM_ADD_UNIT_TEST(uncstring_test uncstring_test.c)
TARGET_LINK_LIBRARIES(uncstring_test PRIVATE libyasm)

ADD_YASM_TESTS(libyasm "${CMAKE_CURRENT_SOURCE_DIR}" "" ARGS -f bin)
