enable_testing()

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/test/include)
include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/external/libtap)

set(ALL_TESTS "sys1")


foreach(API_TEST ${ALL_TESTS})
  add_executable(${API_TEST} ${API_TEST}.c)
  target_link_libraries(${API_TEST} tap)
endforeach()

