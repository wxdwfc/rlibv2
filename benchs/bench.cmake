include_directories(./examples/gflags/include)

add_executable(bench_client ./benchs/bench_client.cc)
add_executable(bench_server ./benchs/bench_server.cc)
add_executable(fly_client ./benchs/fly_bench/client.cc)
add_executable(or_client ./benchs/or_bench/client.cc)
add_executable(db_client ./benchs/db_bench/client.cc)
# DC benchs
add_executable(dc_client ./benchs/dc_bench/client.cc)
add_executable(dc_server ./benchs/dc_bench/dc_server.cc)


set(benchs
bench_client bench_server
fly_client or_client db_client
dc_client dc_server
)

foreach(b ${benchs})
 target_link_libraries(${b} pthread ibverbs gflags)
endforeach(b)