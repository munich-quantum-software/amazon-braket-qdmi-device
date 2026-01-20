# Set example target name
set(TARGET_NAME AMAZON_BRAKET_QDMI_example)

add_executable(${TARGET_NAME} main.cpp)

target_link_libraries(${TARGET_NAME} PRIVATE amazon-braket-qdmi-device)
