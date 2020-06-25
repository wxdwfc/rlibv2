include_directories(./tests/googletest/googletest/include)

set(ggtest_DIR "${CMAKE_SOURCE_DIR}/tests/googletest")
add_subdirectory(${ggtest_DIR})

file(GLOB TSOURCES  "tests/*.cc" )
add_executable(coretest ${TSOURCES} )

target_link_libraries(coretest gtest gtest_main ibverbs)
