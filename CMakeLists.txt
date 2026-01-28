# set required cmake version
cmake_minimum_required(VERSION 3.25...4.2)

project(
  amazon-braket-qdmi-device
  VERSION 0.1.0
  LANGUAGES C CXX
  DESCRIPTION "Amazon Braket QDMI Device Implementation")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
                                               "MinSizeRel" "RelWithDebInfo")
endif()

option(
  USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE
  "Use an installed version of the Amazon Braket QDMI Device to build tests"
  OFF)

if(USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE)
  find_package(amazon-braket-qdmi-device ${PROJECT_VERSION} REQUIRED)
endif()

# Generate compile_commands.json to make it easier to work with clang based
# tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERIFY_INTERFACE_HEADER_SETS ON)

set(QDMI_PREFIX "AMAZON_BRAKET")

# check if this is the master project or used via add_subdirectory
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(AMAZON_BRAKET_QDMI_MASTER_PROJECT ON)
else()
  set(AMAZON_BRAKET_QDMI_MASTER_PROJECT OFF)
endif()

option(BUILD_AMAZON_BRAKET_TESTS
       "Build tests for the Amazon Braket QDMI device"
       ${AMAZON_BRAKET_QDMI_MASTER_PROJECT})
option(INSTALL "Enable installation" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add path for custom modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(cmake/ExternalDependencies.cmake)

# set the include directory for the build tree
set(INCLUDE_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Set target name
set(QDMI_TARGET_NAME "amazon-braket-qdmi-device")

# add main library code
if(NOT USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE)
  add_subdirectory(src)
endif()

# add test code
if(BUILD_AMAZON_BRAKET_TESTS)
  enable_testing()
  include(GoogleTest)
  add_subdirectory(test)
endif()
