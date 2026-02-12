# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

include(GNUInstallDirs)

# Generate QDMI headers with the specified prefix
generate_prefixed_qdmi_headers(${QDMI_PREFIX})

# Collect source files
file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
# cmake-format: off
# Collect private header files (Device.hpp)
file(GLOB_RECURSE PRIVATE_HEADER_FILES "${PROJECT_SOURCE_DIR}/include/*.hpp")
# cmake-format: on
file(GLOB_RECURSE PUBLIC_HEADER_FILES "${CMAKE_CURRENT_BINARY_DIR}/include/*.h")

add_library(${QDMI_TARGET_NAME} SHARED)
set_target_properties(${QDMI_TARGET_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_compile_features(${QDMI_TARGET_NAME} PRIVATE cxx_std_20)
get_target_property(qdmi_library_VERSION qdmi::qdmi VERSION)
target_compile_definitions(
  ${QDMI_TARGET_NAME} PRIVATE QDMI_VERSION="${qdmi_library_VERSION}"
                              AMAZON_BRAKET_QDMI_VERSION="${PROJECT_VERSION}")
target_sources(${QDMI_TARGET_NAME} PRIVATE ${SRC_FILES})

target_sources(
  ${QDMI_TARGET_NAME}
  # cmake-format: off
  PRIVATE FILE_SET
      private_headers
      TYPE
      HEADERS
      BASE_DIRS
      ${PROJECT_SOURCE_DIR}/include
      FILES
      ${PRIVATE_HEADER_FILES}
  # cmake-format: on
  PUBLIC FILE_SET
         public_headers
         TYPE
         HEADERS
         BASE_DIRS
         ${CMAKE_CURRENT_BINARY_DIR}/include
         FILES
         ${PUBLIC_HEADER_FILES})

target_link_libraries(
  ${QDMI_TARGET_NAME}
  PUBLIC qdmi::qdmi
  PRIVATE AWS::aws-cpp-sdk-core AWS::aws-cpp-sdk-s3 AWS::aws-cpp-sdk-braket
          qdmi::qdmi_project_warnings)

# Set RPATH for installed library so it can find its dependencies in the same install prefix on
# macOS and Linux. Skipped on Windows, where DLL lookup uses the executable folder or PATH.
if(NOT WIN32)
  set(RPATH_PREFIX $<IF:$<PLATFORM_ID:Darwin>,@loader_path,$ORIGIN>)
  set_target_properties(
    ${QDMI_TARGET_NAME}
    PROPERTIES INSTALL_RPATH "${RPATH_PREFIX}/../lib:${RPATH_PREFIX}/../lib64"
               INSTALL_RPATH_USE_LINK_PATH FALSE
               MACOSX_RPATH TRUE)
endif()

option(AMAZON_BRAKET_QDMI_COVERAGE "Enable coverage instrumentation" OFF)

if(AMAZON_BRAKET_QDMI_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Building with coverage instrumentation")
    target_compile_options(${QDMI_TARGET_NAME} PUBLIC --coverage -O0 -g)
    target_link_options(${QDMI_TARGET_NAME} PUBLIC --coverage)
  else()
    message(WARNING "Coverage instrumentation is only supported with GCC/Clang")
  endif()
endif()

set(AMAZON_BRAKET_QDMI_CONFIG_INSTALL_DIR
    "${CMAKE_INSTALL_DATADIR}/cmake/${QDMI_TARGET_NAME}"
    CACHE INTERNAL "Config install directory")

include(CMakePackageConfigHelpers)
configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/${QDMI_TARGET_NAME}-config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}-config.cmake"
  INSTALL_DESTINATION ${AMAZON_BRAKET_QDMI_CONFIG_INSTALL_DIR}
  NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO)
write_basic_package_version_file(
  "${QDMI_TARGET_NAME}-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMinorVersion)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}-config-version.cmake"
        DESTINATION ${AMAZON_BRAKET_QDMI_CONFIG_INSTALL_DIR})

# cmake-format: off
install(
    TARGETS ${QDMI_TARGET_NAME}
    EXPORT ${QDMI_TARGET_NAME}-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${QDMI_TARGET_NAME}_Runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT ${QDMI_TARGET_NAME}_Runtime
    NAMELINK_COMPONENT ${QDMI_TARGET_NAME}_Development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT ${QDMI_TARGET_NAME}_Development
    FILE_SET public_headers
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# cmake-format: on

install(
  EXPORT ${QDMI_TARGET_NAME}-targets
  FILE ${QDMI_TARGET_NAME}-targets.cmake
  DESTINATION ${AMAZON_BRAKET_QDMI_CONFIG_INSTALL_DIR}
  COMPONENT ${QDMI_TARGET_NAME}_Development)
