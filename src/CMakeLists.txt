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

include(GNUInstallDirs)

# Generate QDMI headers with the specified prefix
generate_prefixed_qdmi_headers(${QDMI_PREFIX})

# Collect source files
set(DEVICE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Device.cpp")
# Private header files (internal implementation only)
set(PRIVATE_HEADER_FILES "${PROJECT_SOURCE_DIR}/include/amazon-braket-qdmi-device/Device.hpp"
                         "${PROJECT_SOURCE_DIR}/include/amazon-braket-qdmi-device/DeviceParser.hpp")
# Public header files (user-facing API): Generated QDMI headers (device.h),
file(GLOB_RECURSE PUBLIC_HEADER_FILES "${CMAKE_CURRENT_BINARY_DIR}/include/*.h")
# and constants.hpp (semantic parameter names for user convenience)
list(APPEND PUBLIC_HEADER_FILES
     "${PROJECT_SOURCE_DIR}/include/amazon-braket-qdmi-device/constants.hpp")

get_target_property(qdmi_library_VERSION qdmi::qdmi VERSION)

# Internal object library for DeviceParser. Both the shared library and the unit-test target consume
# it, so private parser classes need not be exported from the shared library ABI.
set(QDMI_PARSER_OBJECTS "${QDMI_TARGET_NAME}-parser-objects")
add_library(${QDMI_PARSER_OBJECTS} OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/DeviceParser.cpp")
add_library(amazon-braket-qdmi-device::parser-objects ALIAS ${QDMI_PARSER_OBJECTS})
target_compile_features(${QDMI_PARSER_OBJECTS} PRIVATE cxx_std_20)
set_target_properties(${QDMI_PARSER_OBJECTS} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(
  ${QDMI_PARSER_OBJECTS}
  PRIVATE QDMI_VERSION="${qdmi_library_VERSION}"
          ${QDMI_PREFIX}_QDMI_DEVICE_VERSION="${PROJECT_VERSION}"
          ${QDMI_PREFIX}_QDMI_device_EXPORTS)
target_include_directories(${QDMI_PARSER_OBJECTS} PUBLIC ${PROJECT_SOURCE_DIR}/include
                                                         ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(${QDMI_PARSER_OBJECTS} PRIVATE AWS::aws-cpp-sdk-core
                                                     qdmi::qdmi_project_warnings)

# Compile the device implementation once for the shared library and other build-tree consumers.
set(QDMI_DEVICE_OBJECTS "${QDMI_TARGET_NAME}-internals")
add_library(${QDMI_DEVICE_OBJECTS} OBJECT ${DEVICE_SRC})
add_library(amazon-braket-qdmi-device::internals ALIAS ${QDMI_DEVICE_OBJECTS})
target_compile_features(${QDMI_DEVICE_OBJECTS} PRIVATE cxx_std_20)
set_target_properties(
  ${QDMI_DEVICE_OBJECTS}
  PROPERTIES POSITION_INDEPENDENT_CODE ON
             C_VISIBILITY_PRESET hidden
             CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN 1)
target_compile_definitions(
  ${QDMI_DEVICE_OBJECTS}
  PRIVATE QDMI_VERSION="${qdmi_library_VERSION}"
          ${QDMI_PREFIX}_QDMI_DEVICE_VERSION="${PROJECT_VERSION}"
          ${QDMI_PREFIX}_QDMI_device_EXPORTS)
target_include_directories(${QDMI_DEVICE_OBJECTS} PUBLIC ${PROJECT_SOURCE_DIR}/include
                                                         ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(
  ${QDMI_DEVICE_OBJECTS}
  PUBLIC AWS::aws-cpp-sdk-core AWS::aws-cpp-sdk-s3 AWS::aws-cpp-sdk-braket
  PRIVATE qdmi::qdmi_project_warnings)

add_library(${QDMI_TARGET_NAME} SHARED)
target_compile_features(${QDMI_TARGET_NAME} PRIVATE cxx_std_20)
set_target_properties(
  ${QDMI_TARGET_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
             C_VISIBILITY_PRESET hidden
             CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN 1)

target_sources(
  ${QDMI_TARGET_NAME}
  PRIVATE FILE_SET
          private_headers
          TYPE
          HEADERS
          BASE_DIRS
          ${PROJECT_SOURCE_DIR}/include
          FILES
          ${PRIVATE_HEADER_FILES}
  PUBLIC FILE_SET
         public_headers
         TYPE
         HEADERS
         BASE_DIRS
         ${CMAKE_CURRENT_BINARY_DIR}/include
         ${PROJECT_SOURCE_DIR}/include
         FILES
         ${PUBLIC_HEADER_FILES})

target_link_libraries(${QDMI_TARGET_NAME} PRIVATE qdmi::qdmi_project_warnings
                                                  ${QDMI_DEVICE_OBJECTS} ${QDMI_PARSER_OBJECTS})

option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
if(ENABLE_COVERAGE)
  target_link_libraries(${QDMI_DEVICE_OBJECTS} PUBLIC qdmi::qdmi_coverage_flags)
endif()

set(${QDMI_PREFIX}_QDMI_CONFIG_INSTALL_DIR
    "${CMAKE_INSTALL_DATADIR}/cmake/${QDMI_TARGET_NAME}"
    CACHE INTERNAL "Config install directory")

include(CMakePackageConfigHelpers)
configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/${QDMI_TARGET_NAME}-config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}-config.cmake"
  INSTALL_DESTINATION ${${QDMI_PREFIX}_QDMI_CONFIG_INSTALL_DIR}
  NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO)
write_basic_package_version_file(
  "${QDMI_TARGET_NAME}-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMinorVersion)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}-config-version.cmake"
  DESTINATION ${${QDMI_PREFIX}_QDMI_CONFIG_INSTALL_DIR}
  COMPONENT ${QDMI_TARGET_NAME}_Development)

install(
  TARGETS ${QDMI_TARGET_NAME}
  EXPORT ${QDMI_TARGET_NAME}-targets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${QDMI_TARGET_NAME}_Runtime
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
          COMPONENT ${QDMI_TARGET_NAME}_Runtime
          NAMELINK_COMPONENT ${QDMI_TARGET_NAME}_Development
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
          COMPONENT ${QDMI_TARGET_NAME}_Development
          FILE_SET public_headers
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          COMPONENT ${QDMI_TARGET_NAME}_Development)

install(
  EXPORT ${QDMI_TARGET_NAME}-targets
  FILE ${QDMI_TARGET_NAME}-targets.cmake
  DESTINATION ${${QDMI_PREFIX}_QDMI_CONFIG_INSTALL_DIR}
  COMPONENT ${QDMI_TARGET_NAME}_Development)
