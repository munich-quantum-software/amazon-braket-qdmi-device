# Set test target name
set(TARGET_NAME amazon-braket-qdmi-device-tests)

add_executable(${TARGET_NAME} test_device.cpp)

target_link_libraries(
  ${TARGET_NAME} PRIVATE amazon-braket-qdmi-device GTest::gtest
                         GTest::gtest_main GTest::gmock)

# On Windows we need all runtime DLLs next to the test executable
if(WIN32)
  add_custom_command(
    TARGET ${TARGET_NAME}
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:${TARGET_NAME}> $<TARGET_FILE_DIR:${TARGET_NAME}>
    COMMAND_EXPAND_LISTS)
endif()

gtest_discover_tests(${TARGET_NAME} DISCOVERY_TIMEOUT 60 DISCOVERY_MODE
                     PRE_TEST)
