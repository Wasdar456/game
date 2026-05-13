# ═══════════════════════════════════════════════════════════════
# network 模块 - Qt Network 静态库（PVP 联机）
# ═══════════════════════════════════════════════════════════════

file(GLOB_RECURSE NETWORK_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(game_network STATIC ${NETWORK_SOURCES})

# game_core 暂时未实现，等 Phase 1 完成后再加上
# target_link_libraries(game_network PUBLIC game_core)
target_link_libraries(game_network PUBLIC
    Qt6::Core
    Qt6::Network
)

target_include_directories(game_network PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_options(game_network PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

set_target_properties(game_network PROPERTIES
    OUTPUT_NAME "game_network"
    POSITION_INDEPENDENT_CODE ON
)
