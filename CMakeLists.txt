project(lcc C)

cmake_minimum_required(VERSION 3.16)

include(TestBigEndian)

test_big_endian(HAVE_BIGENDIAN)

# versioning
set(LCC_VERSION_MAJOR 0)
set(LCC_VERSION_MINOR 9)
set(LCC_VERSION_PATCH 0)
set(LCC_VERSION_SUFFIX "dev0")

set(LCC_VERSION_STR "${LCC_VERSION_MAJOR}.${LCC_VERSION_MINOR}.${LCC_VERSION_PATCH}")
if(LCC_VERSION_SUFFIX)
set(LCC_VERSION_STR "${LCC_VERSION_STR}-${LCC_VERSION_SUFFIX}")
endif()

#standard settings
set(LCC_DEFAULT_PORT 3306)
set(LCC_DEFAULT_UNIX_SOCKET "/tmp/mysql.sock")

if(CMAKE_COMPILER_IS_GNUCC)
  SET(CMAKE_C_FLAGS "-Wunused -Wlogical-op -Wno-uninitialized  -Wall -Wextra -Wformat-security -Wno-init-self -Wwrite-strings -Wshift-count-overflow -Wdeclaration-after-statement -Wno-undef -Wno-unknown-pragmas")
endif()

configure_file(${CMAKE_SOURCE_DIR}/include/lcc_config.h.in
               ${CMAKE_BINARY_DIR}/include/lcc_config.h @ONLY)

include_directories(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_BINARY_DIR}/include
                    ${CMAKE_SOURCE_DIR}/external/inih)

add_subdirectory(external/inih)

set (source_files
     src/lcc_error.c
     src/lcc_protocol.c
     src/lcc_configuration.c
     src/lcc_auth.c
     src/lcc_io.c
     src/lcc_list.c
     src/lcc_mem.c
     src/lcc_result.c
     external/sha1/sha1.c
     src/lcc.c)

add_executable(lcc ${source_files})
target_link_libraries(lcc -lm -lsocket inih)

add_subdirectory(external/libtap)
add_subdirectory(test)
