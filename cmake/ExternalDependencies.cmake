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

if(WIN32)
  if(NOT DEFINED ENABLE_UNITY_BUILD)
    set(ENABLE_UNITY_BUILD
        ON
        CACHE
          BOOL
          "The AWS SDK will be built using a single unified .cpp file for each service library. Reduces the size of static library binaries."
          FORCE)
  endif()
  # On Windows, LEGACY_BUILD=ON (required for Braket) adds a broken "DebugOpt" configuration that
  # causes CMAKE_SHARED_LINKER_FLAGS_DEBUGOPT errors during generation. Forcing static libraries and
  # enabling unity build avoids generating targets that rely on DebugOpt, preventing this error even
  # in Release builds.
  if(NOT DEFINED BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS
        OFF
        CACHE BOOL "All AWS libraries will be built as static objects" FORCE)
  endif()
endif()

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

# Unset the local override so it doesn't affect the rest of the project
unset(ENABLE_TESTING)

# After attempting to make awssdk available (either by finding or fetching), check how it has been
# provided. Modern/FetchContent builds provide namespaced targets `AWS::*`, while older or static
# installations may only provide variables. This logic abstracts away the difference, providing a
# consistent set of variables for the rest of the project to use.
if(TARGET AWS::aws-cpp-sdk-core)
  # The modern target-based approach is available.
  set(AMAZON_BRAKET_QDMI_AWS_DEPS AWS::aws-cpp-sdk-core AWS::aws-cpp-sdk-s3 AWS::aws-cpp-sdk-braket)
  # Include directories are handled by the targets themselves, so this is empty.
  set(AMAZON_BRAKET_QDMI_AWS_INCLUDE_DIRS "")
else()
  # Fallback to the legacy variable-based approach.
  set(AMAZON_BRAKET_QDMI_AWS_DEPS ${AWSSDK_LIBRARIES})
  set(AMAZON_BRAKET_QDMI_AWS_INCLUDE_DIRS ${AWSSDK_INCLUDE_DIRS})
endif()
