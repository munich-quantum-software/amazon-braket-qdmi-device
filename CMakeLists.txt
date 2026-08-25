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

# set required cmake version
cmake_minimum_required(VERSION 3.24...4.4)

project(
  amazon-braket-qdmi-device
  VERSION 1.1.1
  LANGUAGES C CXX
  DESCRIPTION "Amazon Braket QDMI Device Implementation")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel"
                                               "RelWithDebInfo")
endif()

option(USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE
       "Use an installed version of the Amazon Braket QDMI Device to build tests" OFF)

if(USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE)
  find_package(amazon-braket-qdmi-device ${PROJECT_VERSION} REQUIRED CONFIG)
endif()

# Generate compile_commands.json to make it easier to work with clang based tools
set(CMAKE_EXPORT_COMPILE_COMMANDS
    ON
    CACHE BOOL "Export compile commands" FORCE)

set(CMAKE_VERIFY_INTERFACE_HEADER_SETS
    ON
    CACHE BOOL "Verify interface header sets" FORCE)

set(CMAKE_POSITION_INDEPENDENT_CODE
    ON
    CACHE BOOL "Generate position independent code" FORCE)

set(QDMI_PREFIX "AMAZON_BRAKET")

# Check if this is the master project or used via add_subdirectory
option(BUILD_AMAZON_BRAKET_TESTS "Build tests for the Amazon Braket QDMI device"
       ${PROJECT_IS_TOP_LEVEL})
option(BUILD_AMAZON_BRAKET_LIVE_TESTS
       "Build tests that access AWS and may submit paid Amazon Braket tasks" OFF)

if(BUILD_AMAZON_BRAKET_LIVE_TESTS AND NOT BUILD_AMAZON_BRAKET_TESTS)
  message(FATAL_ERROR "BUILD_AMAZON_BRAKET_LIVE_TESTS requires BUILD_AMAZON_BRAKET_TESTS")
endif()

include(cmake/ExternalDependencies.cmake)

# Set target name
set(QDMI_TARGET_NAME "amazon-braket-qdmi-device")

# add main library code
if(NOT USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE)
  add_subdirectory(src)
endif()

option(BUILD_AMAZON_BRAKET_SPANK_PLUGIN "Build the Slurm SPANK integration plugin" OFF)
if(BUILD_AMAZON_BRAKET_SPANK_PLUGIN)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The Amazon Braket SPANK plugin is supported only on Linux")
  endif()
  add_subdirectory(spank)
endif()

# add test code
if(BUILD_AMAZON_BRAKET_TESTS)
  get_target_property(AMAZON_BRAKET_QDMI_DEVICE_ID ${QDMI_TARGET_NAME} QDMI_DEVICE_ID)
  get_target_property(AMAZON_BRAKET_QDMI_DEVICE_PREFIX ${QDMI_TARGET_NAME} QDMI_DEVICE_PREFIX)
  get_target_property(AMAZON_BRAKET_QDMI_MANIFEST_NAME ${QDMI_TARGET_NAME} QDMI_MANIFEST_NAME)
  if(NOT AMAZON_BRAKET_QDMI_DEVICE_ID STREQUAL "amazon.braket.default")
    message(FATAL_ERROR "The Amazon Braket QDMI target does not export its stable device ID")
  endif()
  if(NOT AMAZON_BRAKET_QDMI_DEVICE_PREFIX STREQUAL "${QDMI_PREFIX}")
    message(FATAL_ERROR "The Amazon Braket QDMI target does not export its symbol prefix")
  endif()
  if(NOT AMAZON_BRAKET_QDMI_MANIFEST_NAME STREQUAL "${QDMI_TARGET_NAME}.qdmi.json")
    message(FATAL_ERROR "The Amazon Braket QDMI target does not export its device catalog")
  endif()

  enable_testing()
  include(GoogleTest)
  add_subdirectory(test)
endif()
