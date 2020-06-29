include_directories(./examples/gflags/include)

add_executable(bench_naive ./benchs/bench_main.cc)

target_link_libraries(bench_naive gflags ibverbs pthread)
