# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://llvm.org/LICENSE.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# ------------------------------------------------------------------------------
# Non-Functional Tests
# ------------------------------------------------------------------------------

generate_device_defs_executable(${QDMI_PREFIX} TARGET ${QDMI_TARGET_NAME})

# ------------------------------------------------------------------------------
# Functional Tests
# ------------------------------------------------------------------------------

set(QDMI_TEST_TARGET_NAME "${QDMI_TARGET_NAME}-test")

# create an executable in which the tests will be stored
if(USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE)
  # Install mode: only test the public C API against the installed library
  add_executable(${QDMI_TEST_TARGET_NAME} test_device.cpp)
else()
  # Build-from-source: also compile offline unit tests for internal parser classes.
  add_executable(${QDMI_TEST_TARGET_NAME} test_device.cpp test_device_unit.cpp test_measurement.cpp)
  target_link_libraries(${QDMI_TEST_TARGET_NAME} PRIVATE ${QDMI_TARGET_NAME}-parser-objects)
endif()

# link the Google test infrastructure to the test executable.
target_link_libraries(
  ${QDMI_TEST_TARGET_NAME} PRIVATE GTest::gtest_main GTest::gmock ${QDMI_TARGET_NAME} qdmi::qdmi
                                   qdmi::qdmi_project_warnings)

if(NOT USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE)
  target_link_libraries(
    ${QDMI_TEST_TARGET_NAME} PRIVATE AWS::aws-cpp-sdk-braket AWS::aws-cpp-sdk-s3
                                     AWS::aws-cpp-sdk-sts AWS::aws-cpp-sdk-core)
endif()

# set c++ standard
target_compile_features(${QDMI_TEST_TARGET_NAME} PRIVATE cxx_std_20)

add_dependencies(${QDMI_TEST_TARGET_NAME} qdmi_test_amazon_braket_device_defs)

set(QDMI_LIVE_TEST_FILTER
    "AmazonBraketQDMISpecificationTest.*:AmazonBraketQDMIJobSpecificationTest.*:AmazonBraketQDMIPerJobS3Test.*:DeviceParsingTestFixture.*:AmazonBraketQDMIWaitTimeoutTest.JobWaitTimeout"
)
set(QDMI_OFFLINE_TEST_FILTER "-${QDMI_LIVE_TEST_FILTER}")

if(BUILD_AMAZON_BRAKET_LIVE_TESTS)
  set(QDMI_LIVE_TEST_TARGET_NAME "${QDMI_TARGET_NAME}-live-test")
  add_executable(${QDMI_LIVE_TEST_TARGET_NAME} test_live.cpp)
  target_link_libraries(
    ${QDMI_LIVE_TEST_TARGET_NAME} PRIVATE GTest::gtest_main ${QDMI_TARGET_NAME} qdmi::qdmi
                                          qdmi::qdmi_project_warnings)
  target_compile_features(${QDMI_LIVE_TEST_TARGET_NAME} PRIVATE cxx_std_20)
  add_dependencies(${QDMI_LIVE_TEST_TARGET_NAME} qdmi_test_amazon_braket_device_defs)
endif()

if(WIN32)
  # On Windows, copy the DLL to the test executable directory
  add_custom_command(
    TARGET ${QDMI_TEST_TARGET_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${QDMI_TARGET_NAME}>
            $<TARGET_FILE_DIR:${QDMI_TEST_TARGET_NAME}>
    COMMENT "Copying ${QDMI_TARGET_NAME} DLL to test directory")

  if(BUILD_AMAZON_BRAKET_LIVE_TESTS)
    add_custom_command(
      TARGET ${QDMI_LIVE_TEST_TARGET_NAME}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${QDMI_TARGET_NAME}>
              $<TARGET_FILE_DIR:${QDMI_LIVE_TEST_TARGET_NAME}>
      COMMENT "Copying ${QDMI_TARGET_NAME} DLL to live test directory")
  endif()

  # Add the directory containing the DLL to the test's PATH environment
  set(DLL_PATH "$<TARGET_FILE_DIR:${QDMI_TARGET_NAME}>")

  # Offline AWS SDK construction must not probe the EC2 Instance Metadata Service. This keeps
  # discovery hermetic and avoids network-dependent delays.
  gtest_discover_tests(
    ${QDMI_TEST_TARGET_NAME} TEST_FILTER "${QDMI_OFFLINE_TEST_FILTER}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY
               "${CMAKE_CURRENT_BINARY_DIR}"
               ENVIRONMENT
               "PATH=${DLL_PATH};$ENV{PATH};AWS_EC2_METADATA_DISABLED=true"
               DISCOVERY_TIMEOUT
               60
               DISCOVERY_MODE
               PRE_TEST)
else()
  # Offline AWS SDK construction must not probe the EC2 Instance Metadata Service. This keeps
  # discovery hermetic and avoids network-dependent delays.
  gtest_discover_tests(
    ${QDMI_TEST_TARGET_NAME} TEST_FILTER "${QDMI_OFFLINE_TEST_FILTER}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" ENVIRONMENT
               "AWS_EC2_METADATA_DISABLED=true" DISCOVERY_TIMEOUT 60)
endif()

if(BUILD_AMAZON_BRAKET_LIVE_TESTS)
  gtest_discover_tests(
    ${QDMI_TEST_TARGET_NAME} TEST_FILTER "${QDMI_LIVE_TEST_FILTER}"
    TEST_PREFIX "live::"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES RUN_SERIAL TRUE LABELS amazon-braket-live DISCOVERY_TIMEOUT 60)

  gtest_discover_tests(
    ${QDMI_LIVE_TEST_TARGET_NAME}
    TEST_PREFIX "live::"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES RUN_SERIAL TRUE LABELS amazon-braket-live DISCOVERY_TIMEOUT 60)
endif()
