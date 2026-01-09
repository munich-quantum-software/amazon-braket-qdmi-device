# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM Copyright (c) 2025
# Munich Quantum Software Company GmbH All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

# Declare all external dependencies and make sure that they are available.

include(FetchContent)
include(CMakeDependentOption)
set(FETCH_PACKAGES "")

if(BUILD_TESTS)
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
  set(GTEST_URL
      https://github.com/google/googletest/archive/refs/tags/v${GTEST_VERSION}.tar.gz
  )
  FetchContent_Declare(googletest URL ${GTEST_URL} FIND_PACKAGE_ARGS
                                      ${GTEST_VERSION} NAMES GTest)
  list(APPEND FETCH_PACKAGES googletest)
endif()

# cmake-format: off
set(QDMI_VERSION 1.2.0
        CACHE STRING "QDMI version")
set(QDMI_REV "3da94506d365963db8b1e419488858cfa6371d2d" # v1.2.0
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

if(WIN32 AND NOT DEFINED AWSSDK_FLAGS_SET)
  # Ensure MSVC uses shared CRT and unity build
  set(USE_SHARED_CRT
      ON
      CACHE BOOL "" FORCE)
  set(ENABLE_UNITY_BUILD
      ON
      CACHE BOOL "" FORCE)
  set(AWSSDK_FLAGS_SET
      TRUE
      CACHE INTERNAL "" FORCE)
endif()

# Try to find system-installed AWS SDK first
find_package(AWSSDK COMPONENTS braket s3 core)

# If not found, fetch from GitHub
if(NOT AWSSDK_FOUND)
  set(AWSSDK_VERSION
      1.11.714
      CACHE STRING "AWS SDK version")

  set(BUILD_ONLY
      "core;s3;braket"
      CACHE STRING "" FORCE)
  set(ENABLE_TESTING
      OFF
      CACHE BOOL "Disable building unit and integration tests" FORCE)
  set(AUTORUN_UNIT_TESTS
      OFF
      CACHE BOOL "Disable automatically running unit tests after building"
            FORCE)
  set(AWS_SDK_WARNINGS_ARE_ERRORS
      OFF
      CACHE BOOL "Disable warnings as errors" FORCE)

  FetchContent_Declare(
    awssdk
    GIT_REPOSITORY https://github.com/aws/aws-sdk-cpp.git
    GIT_TAG ${AWSSDK_VERSION})

  # Downloads and builds the SDK in-tree if not found system-wide
  FetchContent_MakeAvailable(awssdk)
endif()

# Make all declared dependencies available.
FetchContent_MakeAvailable(${FETCH_PACKAGES})
