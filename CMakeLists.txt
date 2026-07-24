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
cmake_minimum_required(VERSION 3.24...4.2)

project(
  amazon-braket-qdmi-device
  VERSION 1.0.1
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

include(cmake/ExternalDependencies.cmake)

# Set target name
set(QDMI_TARGET_NAME "amazon-braket-qdmi-device")

# add main library code
if(NOT USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE)
  add_subdirectory(src)
endif()

option(BUILD_AMAZON_BRAKET_SPANK_PLUGIN "Build the Slurm SPANK integration plugin" OFF)
if(BUILD_AMAZON_BRAKET_SPANK_PLUGIN)
  if(USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE)
    message(
      FATAL_ERROR
        "The SPANK plugin embeds the Amazon Braket QDMI implementation and must be built from source"
    )
  endif()
  add_subdirectory(spank)
endif()

# add test code
if(BUILD_AMAZON_BRAKET_TESTS)
  enable_testing()
  include(GoogleTest)
  add_subdirectory(test)
endif()
