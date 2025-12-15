cmake_minimum_required(VERSION 3.16)

# Test executables
add_executable(test_block_executor test_block_executor.cpp)
add_executable(test_scheduler test_scheduler.cpp)
add_executable(test_sandbox test_sandbox.cpp)
add_executable(test_observability test_observability.cpp)
add_executable(test_health_endpoint test_health_endpoint.cpp)
add_executable(test_worker_router_contract test_worker_router_contract.cpp)
add_executable(test_observability_performance test_observability_performance.cpp)

# Link with main project libraries
target_link_libraries(test_block_executor
    ${CAF_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(test_scheduler
    ${CAF_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(test_sandbox
    ${CAF_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(test_observability
    ${CAF_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(test_health_endpoint
    ${CAF_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(test_worker_router_contract
    ${CAF_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(test_observability_performance
    ${CAF_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# Add tests
add_test(NAME BlockExecutorTest COMMAND test_block_executor)
add_test(NAME SchedulerTest COMMAND test_scheduler)
add_test(NAME SandboxTest COMMAND test_sandbox)
add_test(NAME ObservabilityTest COMMAND test_observability)
add_test(NAME HealthEndpointTest COMMAND test_health_endpoint)
add_test(NAME WorkerRouterContractTest COMMAND test_worker_router_contract)
add_test(NAME ObservabilityPerformanceTest COMMAND test_observability_performance)