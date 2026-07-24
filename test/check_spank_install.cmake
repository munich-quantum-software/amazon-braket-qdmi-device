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

foreach(REQUIRED_VARIABLE BUILD_DIR STAGE_DIR INSTALL_PREFIX INSTALL_DATADIR)
  if(NOT DEFINED ${REQUIRED_VARIABLE})
    message(FATAL_ERROR "${REQUIRED_VARIABLE} must be defined")
  endif()
endforeach()

file(REMOVE_RECURSE "${STAGE_DIR}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env "DESTDIR=${STAGE_DIR}" "${CMAKE_COMMAND}" --install
          "${BUILD_DIR}" --component amazon-braket-qdmi-spank-plugin
  RESULT_VARIABLE INSTALL_RESULT
  OUTPUT_VARIABLE INSTALL_OUTPUT
  ERROR_VARIABLE INSTALL_ERROR)
if(NOT INSTALL_RESULT EQUAL 0)
  message(FATAL_ERROR "SPANK component installation failed:\n${INSTALL_OUTPUT}\n${INSTALL_ERROR}")
endif()

set(LICENSE_ROOT "${STAGE_DIR}${INSTALL_PREFIX}/${INSTALL_DATADIR}/licenses")
set(PLUGIN_LICENSE "${LICENSE_ROOT}/amazon-braket-qdmi-spank/LICENSE.md")
set(CORE_LICENSE "${LICENSE_ROOT}/amazon-braket-qdmi-device/LICENSE")
foreach(LICENSE_FILE IN ITEMS "${PLUGIN_LICENSE}" "${CORE_LICENSE}")
  if(NOT EXISTS "${LICENSE_FILE}")
    message(FATAL_ERROR "SPANK component did not install ${LICENSE_FILE}")
  endif()
endforeach()

file(READ "${PLUGIN_LICENSE}" PLUGIN_LICENSE_TEXT)
if(NOT PLUGIN_LICENSE_TEXT MATCHES "GNU General Public License")
  message(FATAL_ERROR "Installed SPANK plugin license is not the GPL license text")
endif()

file(READ "${CORE_LICENSE}" CORE_LICENSE_TEXT)
if(NOT CORE_LICENSE_TEXT MATCHES "Apache License")
  message(FATAL_ERROR "Installed core library license is not the Apache license text")
endif()
