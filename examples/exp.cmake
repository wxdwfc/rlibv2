include_directories(./examples/gflags/include)

add_executable(pclient "${CMAKE_SOURCE_DIR}/examples/rc_pingpong/client.cc")
add_executable(pserver "${CMAKE_SOURCE_DIR}/examples/rc_pingpong/server.cc")

target_link_libraries(pclient -lpthread ibverbs gflags)
target_link_libraries(pserver -lpthread ibverbs  gflags)
