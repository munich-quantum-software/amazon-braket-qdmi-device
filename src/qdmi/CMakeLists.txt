# Set target name
set(TARGET_NAME amazon-braket-qdmi-device)

# Set prefix for QDMI
set(QDMI_PREFIX "AMAZON_BRAKET")

# If the target is not already defined
if(NOT TARGET ${TARGET_NAME})

  add_library(${TARGET_NAME} STATIC)

  # Generate prefixed QDMI headers
  generate_prefixed_qdmi_headers(${QDMI_PREFIX})
  file(GLOB_RECURSE QDMI_HDRS
       ${CMAKE_CURRENT_BINARY_DIR}/include/amazon_braket_qdmi/**.h)

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
           ${INCLUDE_BUILD_DIR}/amazon-braket-qdmi-device/qdmi/Device.hpp
           ${INCLUDE_BUILD_DIR}/amazon-braket-qdmi-device/qdmi/constants.h
           ${QDMI_HDRS})

  # Add link libraries
  target_link_libraries(
    ${TARGET_NAME}
    PUBLIC qdmi::qdmi
    PRIVATE AWS::aws-cpp-sdk-core AWS::aws-cpp-sdk-s3 AWS::aws-cpp-sdk-braket)

  # Always compile with position independent code such that the library can be
  # used in shared libraries
  set_target_properties(${TARGET_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

  # Make QDMI version available
  target_compile_definitions(${TARGET_NAME}
                             PRIVATE QDMI_VERSION="${QDMI_VERSION}")
  target_compile_definitions(
    ${TARGET_NAME} PRIVATE AMAZON_BRAKET_QDMI_VERSION="${PROJECT_VERSION}")
  # Generate additional alias for the target required for
  # generate_device_defs_executable function in the tests
  add_library(qdmi::aws_device ALIAS ${TARGET_NAME})
endif()
