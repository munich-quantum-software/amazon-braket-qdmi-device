if(BUILD_AMAZON_BRAKET_SPANK_PLUGIN)
  find_path(SLURM_SPANK_INCLUDE_DIR slurm/spank.h REQUIRED)

  add_library(amazon-braket-qdmi-spank MODULE spank.cpp)
  target_compile_features(amazon-braket-qdmi-spank PRIVATE cxx_std_20)
  target_include_directories(amazon-braket-qdmi-spank
                             PRIVATE ${SLURM_SPANK_INCLUDE_DIR})
  target_link_libraries(amazon-braket-qdmi-spank
                        PRIVATE ${QDMI_TARGET_NAME} qdmi::qdmi_project_warnings)
  set_target_properties(amazon-braket-qdmi-spank PROPERTIES PREFIX "")

  install(TARGETS amazon-braket-qdmi-spank
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/slurm
          COMPONENT ${QDMI_TARGET_NAME}_Runtime)
endif()