project (rlib)

cmake_minimum_required(VERSION 2.8)

## use C++11 features
add_definitions(-std=c++11)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_COMPILER /usr/bin/g++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

## path to ralloc lib, which you need to build manually
set(RALLOC_LIB "${PROJECT_SOURCE_DIR}/lib/libssmalloc.a")

## include paths
include_directories(ralloc)

include( ExternalProject )
set(SSMALLOC_INSTALL_DIR ${CMAKE_SOURCE_DIR}/)
ExternalProject_Add(ralloc
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/ralloc
  CONFIGURE_COMMAND mkdir -p  ${SSMALLOC_INSTALL_DIR}/lib
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
  INSTALL_COMMAND make install
)

add_executable(rnic "examples/rnic.cc")
add_executable(simple_connect "examples/simple_connect.cc")
add_executable(qp "examples/qp_test.cc")

add_dependencies(rnic ralloc)
target_link_libraries(rnic -lpthread ibverbs ${RALLOC_LIB})

add_dependencies(simple_connect ralloc)
target_link_libraries(simple_connect -lpthread ibverbs ${RALLOC_LIB})

add_dependencies(qp ralloc)
target_link_libraries(qp -lpthread ibverbs ${RALLOC_LIB})


