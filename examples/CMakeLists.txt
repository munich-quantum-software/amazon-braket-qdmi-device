# Set example target name
set(TARGET_NAME aws_qdmi_example)

add_executable(${TARGET_NAME} main.cpp)

target_link_libraries(${TARGET_NAME} PRIVATE aws-qdmi-device)
