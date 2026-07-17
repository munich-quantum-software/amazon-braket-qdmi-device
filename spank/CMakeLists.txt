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

if(BUILD_AMAZON_BRAKET_SPANK_PLUGIN)
  find_path(SLURM_SPANK_INCLUDE_DIR slurm/spank.h REQUIRED)

  add_library(amazon-braket-qdmi-spank MODULE spank.cpp)
  target_compile_features(amazon-braket-qdmi-spank PRIVATE cxx_std_20)
  target_include_directories(amazon-braket-qdmi-spank PRIVATE ${SLURM_SPANK_INCLUDE_DIR})
  target_link_libraries(amazon-braket-qdmi-spank PRIVATE ${QDMI_TARGET_NAME}
                                                         qdmi::qdmi_project_warnings)
  set_target_properties(amazon-braket-qdmi-spank PROPERTIES PREFIX "")

  install(TARGETS amazon-braket-qdmi-spank LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/slurm
                                                   COMPONENT ${QDMI_TARGET_NAME}_Runtime)
endif()
