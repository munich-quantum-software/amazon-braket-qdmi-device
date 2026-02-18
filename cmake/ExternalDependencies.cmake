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
set(QDMI_REV "408da3081f15978822ea77d41200ef26cb363399" # v1.2.x
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

set(BUILD_TESTING_PREV ${BUILD_TESTING})
set(BUILD_SHARED_LIBS_PREV ${BUILD_SHARED_LIBS})
if(NOT USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE)
  set(AWSSDK_VERSION
      1.11.752
      CACHE STRING "AWS SDK version")
  set(BUILD_TESTING
      OFF
      CACHE BOOL "Disable building unit and integration tests for AWS SDK" FORCE)
  set(BUILD_SHARED_LIBS
      OFF
      CACHE BOOL "Disable building shared libraries for AWS SDK" FORCE)
  set(BUILD_ONLY
      "s3;braket"
      CACHE STRING "" FORCE)
  set("AUTORUN_UNIT_TESTS"
      OFF
      CACHE BOOL "Disable autorunning unit tests for AWS SDK" FORCE)
  set(AWS_SDK_WARNINGS_ARE_ERRORS
      OFF
      CACHE BOOL "Disable warnings as errors" FORCE)
  set("CPP_STANDARD"
      20
      CACHE STRING "C++ standard to use for AWS SDK" FORCE)
  FetchContent_Declare(
    awssdk
    GIT_REPOSITORY https://github.com/aws/aws-sdk-cpp.git
    GIT_TAG ${AWSSDK_VERSION}
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS ${AWSSDK_VERSION} COMPONENTS braket s3 core)
  list(APPEND FETCH_PACKAGES awssdk)
endif()

# Make all declared dependencies available.
FetchContent_MakeAvailable(${FETCH_PACKAGES})

# Restore the original value of BUILD_TESTING and BUILD_SHARED_LIBS to avoid affecting other
# dependencies or the main project.
set(BUILD_TESTING ${BUILD_TESTING_PREV})
set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_PREV})
