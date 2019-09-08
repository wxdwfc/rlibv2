project (rlib)

cmake_minimum_required(VERSION 2.8)

## use C++11 features
add_definitions(-std=c++14)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_COMPILER /usr/bin/g++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

#add_executable(rnic "examples/rnic.cc")
#add_executable(simple_connect "examples/simple_connect.cc")
#add_executable(qp "examples/qp_test.cc")
#add_executable(pclient "examples/rc_pingpong/client.cc")
#add_executable(pserver "examples/rc_pingpong/server.cc")

#FOREACH(TEST ${TESTS}) 
             #add_dependencies(${TEST} gflags)
             #target_link_libraries(${TEST} -lpthread ibverbs ${RALLOC_LIB} ${gflags_DIR}/libgflags_nothreads.a)
#ENDFOREACH()

include(tests/tests.cmake)

## tests
enable_testing()

add_test(NAME test COMMAND coretest)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose
                  DEPENDS coretest)
