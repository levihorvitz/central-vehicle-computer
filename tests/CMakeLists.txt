include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
    FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(core_tests
    core/fake_clock_test.cpp
    core/scheduler_test.cpp
    core/system_clock_test.cpp
    core/spsc_ring_buffer_test.cpp
    support/allocation_counter.cpp
)
target_include_directories(core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(core_tests PRIVATE cvc_core GTest::gtest_main)

gtest_discover_tests(core_tests)

add_executable(comm_tests
    comm/crc8_test.cpp
    comm/e2e_receiver_test.cpp
    comm/e2e_sender_test.cpp
    support/allocation_counter.cpp
    comm/can_bus_contract_test.cpp
    comm/fake_can_bus_test.cpp
    fakes/fake_can_bus.cpp
    comm/can_rx_queue_test.cpp
)
target_include_directories(comm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(comm_tests PRIVATE cvc_comm GTest::gtest_main)
gtest_discover_tests(comm_tests NO_PRETTY_VALUES)

add_executable(socketcan_tests
    comm/can_bus_contract_test.cpp
    comm/socket_can_bus_test.cpp
    support/allocation_counter.cpp
)
target_include_directories(socketcan_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(socketcan_tests PRIVATE cvc_comm GTest::gtest_main)
gtest_discover_tests(socketcan_tests NO_PRETTY_VALUES PROPERTIES RESOURCE_LOCK vcan0)
