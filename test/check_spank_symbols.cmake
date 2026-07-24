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

execute_process(
  COMMAND "${NM}" -g "${MODULE}"
  RESULT_VARIABLE NM_RESULT
  OUTPUT_VARIABLE NM_OUTPUT
  ERROR_VARIABLE NM_ERROR)
if(NOT NM_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to inspect SPANK module symbols: ${NM_ERROR}")
endif()

string(REPLACE "\n" ";" NM_LINES "${NM_OUTPUT}")
foreach(
  REQUIRED_SYMBOL IN
  ITEMS plugin_name
        plugin_type
        plugin_version
        spank_plugin_version
        slurm_spank_init
        slurm_spank_init_post_opt
        slurm_spank_user_init
        slurm_spank_task_init)
  set(SYMBOL_FOUND FALSE)
  foreach(NM_LINE IN LISTS NM_LINES)
    if(NM_LINE MATCHES "[ \t]_?${REQUIRED_SYMBOL}$")
      set(SYMBOL_FOUND TRUE)
      break()
    endif()
  endforeach()
  if(NOT SYMBOL_FOUND)
    message(FATAL_ERROR "SPANK module does not export '${REQUIRED_SYMBOL}' with C linkage")
  endif()
endforeach()
