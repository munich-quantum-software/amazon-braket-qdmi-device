# ============================================================================
# AWS QDMI Device - CMake Build Configuration
# ============================================================================
# This project implements the QDMI specification for AWS Braket.
# 
# QDMI Project: https://github.com/munich-quantum-software/QDMI
# 
# QDMI defines a standardized C API for quantum devices. This library provides
# the AWS Braket implementation of that API specification.
#
# Build Options:
#   -DBUILD_EXAMPLES=ON/OFF  : Build example programs
#   -DBUILD_TESTS=ON/OFF     : Build test suite
#   -DQDMI_DIR=<path>        : Path to QDMI project (required)
#   -DAWSSDK_DIR=<path>      : Path to AWS SDK installation (optional)
#
# Without AWS SDK:
#   The library builds in stub mode for development/testing without AWS credentials
#
# With AWS SDK:
#   Full AWS Braket integration for real quantum hardware and simulators
# ============================================================================

cmake_minimum_required(VERSION 3.10)
project(aws-qdmi-device LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_EXAMPLES "Build example executable" ON)
option(BUILD_TESTS "Build tests" ON)

# ============================================================================
# QDMI Dependency
# ============================================================================
# QDMI Dependency (REQUIRED)
# ============================================================================
# This library implements the QDMI specification for AWS Braket.
# QDMI headers are required to build.
# Set QDMI_DIR to point to the QDMI project location.
# Example: cmake .. -DQDMI_DIR=/Users/patrickhopf/Code/QDMI
# ============================================================================
if(NOT DEFINED QDMI_DIR)
    set(QDMI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../QDMI" CACHE PATH "Path to QDMI project")
endif()

if(EXISTS "${QDMI_DIR}/include/qdmi/device.h")
    message(STATUS "QDMI found at: ${QDMI_DIR}")
    set(QDMI_INCLUDE_DIR "${QDMI_DIR}/include")
else()
    message(FATAL_ERROR "QDMI not found at ${QDMI_DIR}. This library requires QDMI.\n"
                        "Please set QDMI_DIR: cmake -DQDMI_DIR=/path/to/QDMI ..")
endif()

# ============================================================================
# AWS SDK Detection
# ============================================================================
# Try to find the AWS SDK for C++ with Braket support.
# If not found, the library will build in stub/mock mode.
# 
# To specify AWS SDK location: 
#   cmake -DAWSSDK_ROOT=/Users/patrickhopf/Code/aws-sdk-cpp ..
# 
# The Braket SDK is typically at:
#   /Users/patrickhopf/Code/aws-sdk-cpp/generated/src/aws-cpp-sdk-braket
# ============================================================================

# First try standard find_package
find_package(AWSSDK COMPONENTS braket core)

# If not found, try to find manually using AWSSDK_ROOT
if(NOT AWSSDK_FOUND AND DEFINED AWSSDK_ROOT)
    message(STATUS "Searching for AWS SDK at: ${AWSSDK_ROOT}")
    
    # Look for braket SDK
    set(BRAKET_INCLUDE_DIR "${AWSSDK_ROOT}/generated/src/aws-cpp-sdk-braket/include")
    set(CORE_INCLUDE_DIR "${AWSSDK_ROOT}/generated/src/aws-cpp-sdk-core/include")
    set(BRAKET_LIB_DIR "${AWSSDK_ROOT}/build/generated/src/aws-cpp-sdk-braket")
    set(CORE_LIB_DIR "${AWSSDK_ROOT}/build/generated/src/aws-cpp-sdk-core")
    
    if(EXISTS "${BRAKET_INCLUDE_DIR}" AND EXISTS "${CORE_INCLUDE_DIR}")
        message(STATUS "Found AWS Braket SDK headers at: ${BRAKET_INCLUDE_DIR}")
        set(AWS_SDK_FOUND TRUE)
        set(AWS_SDK_INCLUDE_DIRS "${BRAKET_INCLUDE_DIR}" "${CORE_INCLUDE_DIR}")
        
        # Find libraries
        find_library(AWS_BRAKET_LIB NAMES aws-cpp-sdk-braket PATHS "${BRAKET_LIB_DIR}" NO_DEFAULT_PATH)
        find_library(AWS_CORE_LIB NAMES aws-cpp-sdk-core PATHS "${CORE_LIB_DIR}" NO_DEFAULT_PATH)
        
        if(AWS_BRAKET_LIB AND AWS_CORE_LIB)
            message(STATUS "Found AWS Braket library: ${AWS_BRAKET_LIB}")
            message(STATUS "Found AWS Core library: ${AWS_CORE_LIB}")
            set(AWS_SDK_LIBRARIES "${AWS_BRAKET_LIB}" "${AWS_CORE_LIB}")
        endif()
    endif()
endif()

# ============================================================================
# Main Library Target
# ============================================================================
add_library(aws_qdmi_device
  src/aws_qdmi_device_impl.cpp
)

target_include_directories(aws_qdmi_device PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${QDMI_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)

# Link AWS SDK - always required for this library
if(TARGET AWSSDK::braket)
  target_link_libraries(aws_qdmi_device PRIVATE AWSSDK::braket AWSSDK::core)
  message(STATUS "AWS Braket SDK found via find_package.")
elseif(TARGET aws-cpp-sdk-braket)
  target_link_libraries(aws_qdmi_device PRIVATE aws-cpp-sdk-braket aws-cpp-sdk-core)
  message(STATUS "AWS Braket SDK found via find_package.")
elseif(AWS_SDK_FOUND)
  target_include_directories(aws_qdmi_device PRIVATE ${AWS_SDK_INCLUDE_DIRS})
  target_link_libraries(aws_qdmi_device PRIVATE ${AWS_SDK_LIBRARIES})
  message(STATUS "AWS Braket SDK found manually.")
else()
  message(WARNING "AWS Braket SDK not found. Library will compile but may fail at runtime without proper AWS SDK installation.")
  message(STATUS "  To link AWS SDK: cmake .. -DAWSSDK_ROOT=/path/to/aws-sdk-cpp")
  # Try to find headers at least
  if(EXISTS "${AWSSDK_ROOT}/generated/src/aws-cpp-sdk-braket/include")
    target_include_directories(aws_qdmi_device PRIVATE 
      "${AWSSDK_ROOT}/generated/src/aws-cpp-sdk-braket/include"
      "${AWSSDK_ROOT}/src/aws-cpp-sdk-core/include"
      "${AWSSDK_ROOT}/generated/src/aws-cpp-sdk-core/include"
    )
    message(STATUS "AWS SDK headers found at ${AWSSDK_ROOT}")
  endif()
endif()

if(BUILD_EXAMPLES)
  add_executable(aws_qdmi_example examples/main.cpp)
  target_link_libraries(aws_qdmi_example PRIVATE aws_qdmi_device)

  # Link AWS SDK to example if available, to support direct SDK calls in example
  if(TARGET AWSSDK::braket)
    target_link_libraries(aws_qdmi_example PRIVATE AWSSDK::braket AWSSDK::core)
  elseif(TARGET aws-cpp-sdk-braket)
    target_link_libraries(aws_qdmi_example PRIVATE aws-cpp-sdk-braket aws-cpp-sdk-core)
  elseif(AWS_SDK_FOUND)
    target_include_directories(aws_qdmi_example PRIVATE ${AWS_SDK_INCLUDE_DIRS})
    target_link_libraries(aws_qdmi_example PRIVATE ${AWS_SDK_LIBRARIES})
  endif()
endif()

# ============================================================================
# Installation Configuration
# ============================================================================
# This allows other projects to find and use aws-qdmi-device via CMake's
# find_package(aws-qdmi-device) mechanism.
# ============================================================================

install(TARGETS aws_qdmi_device 
  EXPORT aws_qdmi_deviceTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include  # Important: tells CMake where headers are
)

# Install only PUBLIC headers (not the private implementation header)
install(DIRECTORY include/aws_qdmi 
  DESTINATION include
  FILES_MATCHING PATTERN "*.h"
)

# Generate and install CMake config files so other projects can use:
# find_package(aws_qdmi_device)
include(CMakePackageConfigHelpers)

install(EXPORT aws_qdmi_deviceTargets
  FILE aws_qdmi_deviceTargets.cmake
  NAMESPACE aws_qdmi_device::
  DESTINATION lib/cmake/aws_qdmi_device
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/aws_qdmi_device-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/aws_qdmi_device-config.cmake
  INSTALL_DESTINATION lib/cmake/aws_qdmi_device
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/aws_qdmi_device-config.cmake
  DESTINATION lib/cmake/aws_qdmi_device
)

# ============================================================================
# Testing Configuration
# ============================================================================
# Builds integration tests using Google Test framework.
# Tests work in both stub mode (without AWS) and full mode (with AWS credentials).
# ============================================================================
if(BUILD_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
  include(GoogleTest)
  
  add_executable(test_aws_qdmi_device_integration
    test/test_aws_qdmi_device_integration.cpp
  )
  
  target_link_libraries(test_aws_qdmi_device_integration 
    PRIVATE 
    aws_qdmi_device
    GTest::gtest
    GTest::gtest_main
  )
  
  gtest_discover_tests(test_aws_qdmi_device_integration)
endif()
