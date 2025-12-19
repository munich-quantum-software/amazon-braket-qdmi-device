# Set test target name
set(TARGET_NAME aws-qdmi-device-test)

add_executable(${TARGET_NAME} test_device.cpp)

target_link_libraries(${TARGET_NAME} PRIVATE aws-qdmi-device GTest::gtest
                                             GTest::gtest_main GTest::gmock)

gtest_discover_tests(${TARGET_NAME} DISCOVERY_TIMEOUT 60)
