# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# Copyright (c) 2026 IQM Finland Oy
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

if(BUILD_AMAZON_BRAKET_SPANK_PLUGIN)
  find_path(SLURM_SPANK_INCLUDE_DIR slurm/spank.h REQUIRED)

  add_library(amazon-braket-qdmi-spank MODULE spank.cpp)
  target_compile_features(amazon-braket-qdmi-spank PRIVATE cxx_std_20)
  target_include_directories(amazon-braket-qdmi-spank PRIVATE ${SLURM_SPANK_INCLUDE_DIR})
  target_link_libraries(amazon-braket-qdmi-spank PRIVATE ${QDMI_TARGET_NAME}
                                                         qdmi::qdmi_project_warnings)
  set_target_properties(amazon-braket-qdmi-spank
                        PROPERTIES PREFIX "" INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
  if(APPLE)
    target_link_options(amazon-braket-qdmi-spank PRIVATE "LINKER:-undefined,dynamic_lookup")
  endif()

  set(AMAZON_BRAKET_QDMI_SPANK_INSTALL_DIR_DEFAULT "${CMAKE_INSTALL_FULL_LIBDIR}/slurm")
  find_program(AMAZON_BRAKET_QDMI_SCONTROL_EXECUTABLE scontrol)
  if(AMAZON_BRAKET_QDMI_SCONTROL_EXECUTABLE)
    execute_process(
      COMMAND ${AMAZON_BRAKET_QDMI_SCONTROL_EXECUTABLE} show config
      OUTPUT_VARIABLE AMAZON_BRAKET_QDMI_SLURM_CONFIG
      RESULT_VARIABLE AMAZON_BRAKET_QDMI_SCONTROL_RESULT
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

    if(AMAZON_BRAKET_QDMI_SCONTROL_RESULT EQUAL 0)
      string(REGEX MATCH "PluginDir[ \t]*=[ \t]*([^ \t\r\n]+)" AMAZON_BRAKET_QDMI_PLUGIN_DIR_MATCH
                   "${AMAZON_BRAKET_QDMI_SLURM_CONFIG}")
      if(CMAKE_MATCH_1)
        set(AMAZON_BRAKET_QDMI_SPANK_INSTALL_DIR_DEFAULT "${CMAKE_MATCH_1}")
      endif()
    endif()
  endif()

  set(AMAZON_BRAKET_QDMI_SPANK_INSTALL_DIR
      "${AMAZON_BRAKET_QDMI_SPANK_INSTALL_DIR_DEFAULT}"
      CACHE
        PATH
        "Install directory for the Amazon Braket SPANK plugin (override for distro-specific Slurm plugin paths)"
  )

  mark_as_advanced(AMAZON_BRAKET_QDMI_SCONTROL_EXECUTABLE)

  set(AMAZON_BRAKET_QDMI_SLURM_CONF_DIR_DEFAULT "/etc/slurm")
  if(AMAZON_BRAKET_QDMI_SCONTROL_EXECUTABLE AND AMAZON_BRAKET_QDMI_SCONTROL_RESULT EQUAL 0)
    string(REGEX MATCH "PlugStackConfig[ \t]*=[ \t]*([^ \t\r\n]+)"
                 AMAZON_BRAKET_QDMI_PLUGSTACK_MATCH "${AMAZON_BRAKET_QDMI_SLURM_CONFIG}")
    if(CMAKE_MATCH_1)
      get_filename_component(AMAZON_BRAKET_QDMI_SLURM_CONF_DIR_DEFAULT "${CMAKE_MATCH_1}" DIRECTORY)
    endif()
  endif()

  set(AMAZON_BRAKET_QDMI_SLURM_CONF_DIR
      "${AMAZON_BRAKET_QDMI_SLURM_CONF_DIR_DEFAULT}"
      CACHE PATH "Slurm configuration directory (for plugstack.conf.d drop-in)")

  configure_file(amazon-braket-qdmi.conf.in amazon-braket-qdmi.conf @ONLY)

  set(AMAZON_BRAKET_QDMI_SPANK_COMPONENT amazon-braket-qdmi-spank-plugin)
  install(TARGETS amazon-braket-qdmi-spank
          LIBRARY DESTINATION ${AMAZON_BRAKET_QDMI_SPANK_INSTALL_DIR}
                  COMPONENT ${AMAZON_BRAKET_QDMI_SPANK_COMPONENT})
  get_target_property(AMAZON_BRAKET_QDMI_DEVICE_IS_IMPORTED ${QDMI_TARGET_NAME} IMPORTED)
  if(NOT AMAZON_BRAKET_QDMI_DEVICE_IS_IMPORTED)
    install(
      TARGETS ${QDMI_TARGET_NAME}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${AMAZON_BRAKET_QDMI_SPANK_COMPONENT}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${AMAZON_BRAKET_QDMI_SPANK_COMPONENT})
  endif()
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
endif()
