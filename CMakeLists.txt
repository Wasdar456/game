cmake_minimum_required(VERSION 3.16)
project(DffenseAndAttack VERSION 0.1 LANGUAGES CXX)

# ═══════════════════════════════════════════════════════════════
# C++ 标准 & Qt 自动化工具
# ═══════════════════════════════════════════════════════════════
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ═══════════════════════════════════════════════════════════════
# 查找 Qt6
# ═══════════════════════════════════════════════════════════════
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Multimedia
)

# ═══════════════════════════════════════════════════════════════
# 添加子模块（按依赖顺序：core → data_manager → network → ui → app）
# ═══════════════════════════════════════════════════════════════
add_subdirectory(core)           # 纯 C++ 静态库（不依赖 Qt）
add_subdirectory(data_manager)   # 数据读写静态库（依赖 core）
add_subdirectory(network)        # Qt Network 静态库（依赖 core）
add_subdirectory(ui)             # Qt Widgets 静态库（依赖 core, network, data_manager）
add_subdirectory(resources)      # Qt 资源打包静态库
add_subdirectory(app)            # 生成最终可执行文件

# ═══════════════════════════════════════════════════════════════
# 打印构建信息
# ═══════════════════════════════════════════════════════════════
message(STATUS "")
message(STATUS "========================================")
message(STATUS "DffenseAndAttack - Build Configuration")
message(STATUS "========================================")
message(STATUS "  Qt6 Version:        ${Qt6_VERSION}")
message(STATUS "  C++ Standard:       C++17")
message(STATUS "  Build Type:         ${CMAKE_BUILD_TYPE}")
message(STATUS "========================================")
message(STATUS "")
