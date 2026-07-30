# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program. If not, see <https://www.gnu.org/licenses/>.

include(GNUInstallDirs)

find_path(SLURM_SPANK_INCLUDE_DIR slurm/spank.h REQUIRED)

add_library(amazon-braket-qdmi-spank MODULE spank.cpp)
target_compile_features(amazon-braket-qdmi-spank PRIVATE cxx_std_20)
target_include_directories(amazon-braket-qdmi-spank PRIVATE ${SLURM_SPANK_INCLUDE_DIR})
target_link_libraries(
  amazon-braket-qdmi-spank
  PRIVATE amazon-braket-qdmi-device::internals amazon-braket-qdmi-device::parser-objects
          qdmi::qdmi_project_warnings)
set(AMAZON_BRAKET_QDMI_SPANK_VERSION_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/amazon-braket-qdmi-spank.map")
target_link_options(amazon-braket-qdmi-spank PRIVATE
                    "LINKER:--version-script=${AMAZON_BRAKET_QDMI_SPANK_VERSION_SCRIPT}")
set_property(
  TARGET amazon-braket-qdmi-spank
  APPEND
  PROPERTY LINK_DEPENDS "${AMAZON_BRAKET_QDMI_SPANK_VERSION_SCRIPT}")
set_target_properties(amazon-braket-qdmi-spank PROPERTIES PREFIX "")

set(AMAZON_BRAKET_QDMI_SPANK_INSTALL_DIR
    "${CMAKE_INSTALL_FULL_LIBDIR}/slurm"
    CACHE PATH "Install directory for the Amazon Braket SPANK plugin")
set(AMAZON_BRAKET_QDMI_SLURM_CONF_DIR
    "/etc/slurm"
    CACHE PATH "Slurm configuration directory")

configure_file(amazon-braket-qdmi.conf.in amazon-braket-qdmi.conf @ONLY)

set(AMAZON_BRAKET_QDMI_SPANK_COMPONENT amazon-braket-qdmi-spank-plugin)
install(TARGETS amazon-braket-qdmi-spank LIBRARY DESTINATION ${AMAZON_BRAKET_QDMI_SPANK_INSTALL_DIR}
                                                 COMPONENT ${AMAZON_BRAKET_QDMI_SPANK_COMPONENT})
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/amazon-braket-qdmi.conf
  DESTINATION ${AMAZON_BRAKET_QDMI_SLURM_CONF_DIR}/plugstack.conf.d
  COMPONENT ${AMAZON_BRAKET_QDMI_SPANK_COMPONENT})
install(
  FILES LICENSE.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/amazon-braket-qdmi-spank
  COMPONENT ${AMAZON_BRAKET_QDMI_SPANK_COMPONENT})
install(
  FILES ${PROJECT_SOURCE_DIR}/LICENSE
  DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/amazon-braket-qdmi-device
  COMPONENT ${AMAZON_BRAKET_QDMI_SPANK_COMPONENT})
