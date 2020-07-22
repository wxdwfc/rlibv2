include_directories(./examples/gflags/include)

add_executable(bench_client ./benchs/bench_main.cc)
add_executable(bench_server ./benchs/bench_server.cc)

target_link_libraries(bench_client gflags ibverbs pthread)
target_link_libraries(bench_server gflags ibverbs pthread)
