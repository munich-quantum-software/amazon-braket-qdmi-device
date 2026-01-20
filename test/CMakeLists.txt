# Set test target name
set(TARGET_NAME amazon-braket-qdmi-device-tests)

add_executable(${TARGET_NAME} test_device.cpp)

target_link_libraries(
  ${TARGET_NAME} PRIVATE amazon-braket-qdmi-device GTest::gtest
                         GTest::gtest_main GTest::gmock)

gtest_discover_tests(${TARGET_NAME} DISCOVERY_TIMEOUT 60 DISCOVERY_MODE
                     PRE_TEST)
