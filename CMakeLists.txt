project(lcc C)

cmake_minimum_required(VERSION 3.16)

include(TestBigEndian)

test_big_endian(HAVE_BIGENDIAN)

configure_file(${CMAKE_SOURCE_DIR}/include/lcc_config.h.in
               ${CMAKE_BINARY_DIR}/include/lcc_config.h @ONLY)

include_directories(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_BINARY_DIR}/include
                    ${CMAKE_SOURCE_DIR}/external/inih)

add_subdirectory(external/inih)

set (source_files
     src/lcc_error.c
     src/protocol.c
     src/lcc_configuration.c
     src/auth.c
     src/sha1.c
     src/lcc.c)

add_executable(lcc ${source_files})
target_link_libraries(lcc -lm -lsocket inih)

add_subdirectory(external/libtap)
add_subdirectory(test)
