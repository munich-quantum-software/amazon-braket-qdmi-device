# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

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

option(USE_INSTALLED_AWS_SDK "Whether to try to use an installed AWS SDK" ON)

# Force static builds for dependencies to ensure robustness on Windows/Mac
if(NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS
      OFF
      CACHE BOOL "Build static libraries by default" FORCE)
endif()
# Required for linking static AWS libs into the shared qdmi-device library
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(AWSSDK_VERSION
    1.11.714
    CACHE STRING "AWS SDK version")

set(AWS_COMPONENTS braket s3 core)

if(USE_INSTALLED_AWS_SDK)
  find_package(AWSSDK ${AWSSDK_VERSION} QUIET COMPONENTS ${AWS_COMPONENTS})
endif()

if(AWSSDK_FOUND)
  message(STATUS "Found installed AWS SDK: ${AWSSDK_VERSION}")
else()
  message(STATUS "AWS SDK not found, fetching from source...")
  # Configuration for Source Build
  set(BUILD_ONLY
      "core;s3;braket"
      CACHE STRING "" FORCE)
  set(AWS_SDK_ENABLE_TESTING
      OFF
      CACHE BOOL "Disable building unit and integration tests" FORCE)
  set(ENABLE_TESTING ${AWS_SDK_ENABLE_TESTING})
  set(AUTORUN_UNIT_TESTS
      OFF
      CACHE BOOL "" FORCE)
  set(AWS_SDK_WARNINGS_ARE_ERRORS
      OFF
      CACHE BOOL "" FORCE)

  if(WIN32 AND NOT DEFINED ENABLE_UNITY_BUILD)
    set(ENABLE_UNITY_BUILD
        ON
        CACHE BOOL "" FORCE)
  endif()

  FetchContent_Declare(
    awssdk
    GIT_REPOSITORY https://github.com/aws/aws-sdk-cpp.git
    GIT_TAG ${AWSSDK_VERSION})
  list(APPEND FETCH_PACKAGES awssdk)
endif()

# Make all declared dependencies available.
FetchContent_MakeAvailable(${FETCH_PACKAGES})

# Unset the local override so it doesn't affect the rest of the project
unset(ENABLE_TESTING)

# Define/Fix Targets for Project Usage
if(TARGET AWS::aws-cpp-sdk-core)
  set(AMAZON_BRAKET_QDMI_AWS_DEPS AWS::aws-cpp-sdk-core AWS::aws-cpp-sdk-s3 AWS::aws-cpp-sdk-braket)
  set(AMAZON_BRAKET_QDMI_AWS_INCLUDE_DIRS "")

  # [Patch] Fix missing system links in static AWS SDK (similar to mqt-core's spdlog patch) This
  # fixes linker errors (LNK2019/Undefined symbols) on Windows/Mac
  if(NOT BUILD_SHARED_LIBS)
    if(WIN32)
      set_property(
        TARGET AWS::aws-cpp-sdk-core
        APPEND
        PROPERTY INTERFACE_LINK_LIBRARIES
                 winhttp
                 wininet
                 bcrypt
                 userenv
                 version
                 ws2_32)
    elseif(APPLE)
      find_library(COREFOUNDATION_FW CoreFoundation)
      find_library(SECURITY_FW Security)
      find_package(CURL REQUIRED)
      set_property(
        TARGET AWS::aws-cpp-sdk-core
        APPEND
        PROPERTY INTERFACE_LINK_LIBRARIES ${COREFOUNDATION_FW} ${SECURITY_FW} CURL::libcurl)
    elseif(UNIX)
      find_package(CURL REQUIRED)
      find_package(OpenSSL REQUIRED)
      set_property(
        TARGET AWS::aws-cpp-sdk-core
        APPEND
        PROPERTY INTERFACE_LINK_LIBRARIES CURL::libcurl OpenSSL::SSL OpenSSL::Crypto)
    endif()
  endif()

else()
  # Legacy Fallback
  set(AMAZON_BRAKET_QDMI_AWS_DEPS ${AWSSDK_LIBRARIES})
  set(AMAZON_BRAKET_QDMI_AWS_INCLUDE_DIRS ${AWSSDK_INCLUDE_DIRS})
endif()
