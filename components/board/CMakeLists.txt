find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(BOARD_DTS "${CMAKE_CURRENT_SOURCE_DIR}/board.dts")
set(DTC_LITE "${CMAKE_SOURCE_DIR}/tools/dtc-lite.py")
set(GENERATED_BOARD_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Pre-create the generated directory so INCLUDE_DIRS doesn't fail
file(MAKE_DIRECTORY "${GENERATED_BOARD_DIR}")

idf_component_register(
    SRCS
        "src/task_config.c"
        "src/task_utils.c"
        "src/board_device.c"
        "src/board_driver.c"
        "src/board_force_link.c"
    INCLUDE_DIRS
        "include"
        "${GENERATED_BOARD_DIR}"
    REQUIRES
        freertos
)

# Generated sources
set(GENERATED_BOARD_SRCS
    "${GENERATED_BOARD_DIR}/board_devtable.c"
    "${GENERATED_BOARD_DIR}/board_probe.c"
)
set(GENERATED_BOARD_HDRS
    "${GENERATED_BOARD_DIR}/board_nodes.h"
    "${GENERATED_BOARD_DIR}/board_devtable.h"
    "${GENERATED_BOARD_DIR}/board_handles.h"
)

add_custom_command(
    OUTPUT
        ${GENERATED_BOARD_SRCS}
        ${GENERATED_BOARD_HDRS}
    COMMAND ${Python3_EXECUTABLE} "${DTC_LITE}" "${BOARD_DTS}" "${GENERATED_BOARD_DIR}" "${CMAKE_SOURCE_DIR}/components/drivers" "${CMAKE_SOURCE_DIR}/components/hw_hal/src"
    DEPENDS
        "${BOARD_DTS}"
        "${DTC_LITE}"
    VERBATIM
)

add_custom_target(board_dts_generate
    DEPENDS ${GENERATED_BOARD_SRCS}
)

add_dependencies(${COMPONENT_LIB} board_dts_generate)

target_sources(${COMPONENT_LIB} PRIVATE ${GENERATED_BOARD_SRCS})
set_source_files_properties(${GENERATED_BOARD_SRCS} PROPERTIES GENERATED TRUE)
# -u flags moved to root CMakeLists.txt with BEFORE scope
# (INTERFACE appends after .a files, ld -u must precede archives)
