# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM Copyright (c) 2025
# Munich Quantum Software Company GmbH All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

# Set target name
set(TARGET_NAME aws-qdmi-device)

# Set prefix for QDMI
set(QDMI_PREFIX "AWS")

# If the target is not already defined
if(NOT TARGET ${TARGET_NAME})
  add_library(${TARGET_NAME} STATIC)

  # Generate prefixed QDMI headers
  generate_prefixed_qdmi_headers(${QDMI_PREFIX})
  file(GLOB_RECURSE QDMI_HDRS ${CMAKE_CURRENT_BINARY_DIR}/include/aws_qdmi/**.h)
  # Add sources to target
  target_sources(${TARGET_NAME} PRIVATE Device.cpp)

  # Add headers using file sets
  target_sources(
    ${TARGET_NAME}
    PUBLIC FILE_SET
           HEADERS
           BASE_DIRS
           ${INCLUDE_BUILD_DIR}
           ${CMAKE_CURRENT_BINARY_DIR}/include
           FILES
           ${INCLUDE_BUILD_DIR}/aws-qdmi/qdmi/aws/Device.hpp
           ${QDMI_HDRS})

  # Add link libraries
  target_link_libraries(
    ${TARGET_NAME}
    PUBLIC qdmi::qdmi
    PRIVATE ${AWSSDK_LINK_LIBRARIES})

  # Always compile with position independent code such that the library can be
  # used in shared libraries
  set_target_properties(${TARGET_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

  # Add to list of MQT Core targets set(MQT_CORE_TARGETS ${MQT_CORE_TARGETS}
  # ${TARGET_NAME} PARENT_SCOPE)

  # Make QDMI version available
  target_compile_definitions(${TARGET_NAME}
                             PRIVATE QDMI_VERSION="${QDMI_VERSION}")
  target_compile_definitions(${TARGET_NAME} PRIVATE AWS_QDMI_VERSION="1.0.0")

  # Generate additional alias for the target required for
  # generate_device_defs_executable function in the tests
  add_library(qdmi::aws_device ALIAS ${TARGET_NAME})
endif()
