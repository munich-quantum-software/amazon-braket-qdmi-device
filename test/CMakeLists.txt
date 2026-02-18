# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

# ------------------------------------------------------------------------------
# Non-Functional Tests
# ------------------------------------------------------------------------------

generate_device_defs_executable(${QDMI_PREFIX} TARGET ${QDMI_TARGET_NAME})

# ------------------------------------------------------------------------------
# Functional Tests
# ------------------------------------------------------------------------------

set(QDMI_TEST_TARGET_NAME "${QDMI_TARGET_NAME}-test")

# create an executable in which the tests will be stored
add_executable(${QDMI_TEST_TARGET_NAME} test_device.cpp)

# link the Google test infrastructure to the test executable.
target_link_libraries(
  ${QDMI_TEST_TARGET_NAME} PRIVATE GTest::gtest_main GTest::gmock ${QDMI_TARGET_NAME}
                                   qdmi::qdmi_project_warnings)

# set c++ standard
target_compile_features(${QDMI_TEST_TARGET_NAME} PRIVATE cxx_std_20)

add_dependencies(${QDMI_TEST_TARGET_NAME} qdmi_test_amazon_braket_device_defs)

# only discover tests if this is the main project
if(NOT IS_TOP_LEVEL)
  return()
endif()

gtest_discover_tests(
  ${QDMI_TEST_TARGET_NAME}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" DISCOVERY_TIMEOUT 60)
