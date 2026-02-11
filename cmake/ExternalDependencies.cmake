# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

# Declare all external dependencies and make sure that they are available.

include(FetchContent)
include(CMakeDependentOption)
include(GNUInstallDirs)
set(FETCH_PACKAGES "")

if(BUILD_AMAZON_BRAKET_TESTS)
  set(gtest_force_shared_crt
      ON
      CACHE BOOL "" FORCE)
  # Disable the install instructions for GTest, as we do not need them.
  set(INSTALL_GTEST
      OFF
      CACHE BOOL "" FORCE)
  set(GTEST_VERSION
      1.17.0
      CACHE STRING "Google Test version")
  set(GTEST_URL https://github.com/google/googletest/archive/refs/tags/v${GTEST_VERSION}.tar.gz)
  FetchContent_Declare(googletest URL ${GTEST_URL} FIND_PACKAGE_ARGS ${GTEST_VERSION} NAMES GTest)
  list(APPEND FETCH_PACKAGES googletest)
endif()

# cmake-format: off
set(QDMI_VERSION 1.2.1
        CACHE STRING "QDMI version")
set(QDMI_REV "70b815615475598c6194096a29c1b2340dd54a6c" # v1.2.x
        CACHE STRING "QDMI identifier (tag, branch or commit hash)")
set(QDMI_REPO_OWNER "Munich-Quantum-Software-Stack"
        CACHE STRING "QDMI repository owner (change when using a fork)")
cmake_dependent_option(QDMI_INSTALL "Install QDMI library" ON "INSTALL" OFF)
# cmake-format: on
FetchContent_Declare(
  qdmi
  GIT_REPOSITORY https://github.com/${QDMI_REPO_OWNER}/qdmi.git
  GIT_TAG ${QDMI_REV}
  FIND_PACKAGE_ARGS ${QDMI_VERSION})
list(APPEND FETCH_PACKAGES qdmi)

set(AWSSDK_VERSION
    1.11.714
    CACHE STRING "AWS SDK version")

set(BUILD_ONLY
    "core;s3;braket"
    CACHE STRING "" FORCE)
set(AWS_SDK_ENABLE_TESTING
    OFF
    CACHE BOOL "Disable building unit and integration tests" FORCE)
set(ENABLE_TESTING ${AWS_SDK_ENABLE_TESTING})

set(AUTORUN_UNIT_TESTS
    OFF
    CACHE BOOL "Disable automatically running unit tests after building" FORCE)
set(AWS_SDK_WARNINGS_ARE_ERRORS
    OFF
    CACHE BOOL "Disable warnings as errors" FORCE)

FetchContent_Declare(
  awssdk
  GIT_REPOSITORY https://github.com/aws/aws-sdk-cpp.git
  GIT_TAG ${AWSSDK_VERSION}
  FIND_PACKAGE_ARGS ${AWSSDK_VERSION} COMPONENTS braket s3 core)
list(APPEND FETCH_PACKAGES awssdk)

# Make all declared dependencies available.
FetchContent_MakeAvailable(${FETCH_PACKAGES})

# AWS SDK C++ adds a "DebugOpt" config on Windows which is often broken (missing linker flags). We
# remove it to prevent "Missing variable is: CMAKE_SHARED_LINKER_FLAGS_DEBUGOPT" errors.
if(CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Configurations: ${CMAKE_CONFIGURATION_TYPES}")
endif()
if(WIN32 AND CMAKE_CONFIGURATION_TYPES)
  list(REMOVE_ITEM CMAKE_CONFIGURATION_TYPES "DebugOpt")
  list(REMOVE_DUPLICATES CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES
      "${CMAKE_CONFIGURATION_TYPES}"
      CACHE STRING "Supported configuration types" FORCE)
endif()

# Unset the local override so it doesn't affect the rest of the project
unset(ENABLE_TESTING)
