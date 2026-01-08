# set required cmake version
cmake_minimum_required(VERSION 3.24...4.0)

project(
  aws-qdmi
  LANGUAGES C CXX
  DESCRIPTION "AWS Braket QDMI Implementation")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(INSTALL "Enable installation" ON)

# Add path for custom modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(cmake/ExternalDependencies.cmake)

# set the include directory for the build tree
set(INCLUDE_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

include(GNUInstallDirs)

if(INSTALL)
  if(APPLE)
    set(BASEPOINT @loader_path)
  else()
    set(BASEPOINT $ORIGIN)
  endif()
  set(CMAKE_INSTALL_RPATH ${BASEPOINT} ${BASEPOINT}/${CMAKE_INSTALL_LIBDIR}
                          ${BASEPOINT}/../${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# add main library code
add_subdirectory(src/qdmi/aws)

# add test code
if(BUILD_TESTS)
  enable_testing()
  include(GoogleTest)
  add_subdirectory(test)
endif()

# add example code
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
