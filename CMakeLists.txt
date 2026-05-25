cmake_minimum_required(VERSION 3.16)
project(DffenseAndAttack VERSION 0.1 LANGUAGES CXX)

# ─────────────────────────────────────────────
# C++ 标准
# ─────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─────────────────────────────────────────────
# Qt 自动化工具（必须在 find_package 之前打开）
# CMAKE_AUTOMOC : 自动处理 Q_OBJECT 宏（生成 moc_xxx.cpp）
# CMAKE_AUTORCC : 自动处理 .qrc 资源文件
# CMAKE_AUTOUIC : 自动处理 .ui 拖拽设计器文件（如有）
# ─────────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ─────────────────────────────────────────────
# 查找 Qt6（需要安装时勾选这几个模块）
# Core / Gui / Widgets : 窗口、绘图、控件
# Network              : PVP 联机 QTcpServer / QTcpSocket
# Multimedia           : 音效（如不需要音效可删掉）
# ─────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Multimedia
)

# ─────────────────────────────────────────────
# 收集所有源文件
# GLOB_RECURSE 会递归扫描 src/ 下的 .cpp 和 .h
# 注意：新增文件后需要重新运行 cmake .. 才能被识别
# ─────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.h"
)

# ─────────────────────────────────────────────
# 生成可执行文件
# resources/resources.qrc 是贴图/音效打包文件（后续创建）
# 如果还没建 .qrc 文件，先注释掉那一行
# ─────────────────────────────────────────────
add_executable(DffenseAndAttack
    ${SOURCES}
    # resources/resources.qrc   ← 等 resources.qrc 建好后取消注释
)

# ─────────────────────────────────────────────
# 链接 Qt 模块
# ─────────────────────────────────────────────
target_link_libraries(DffenseAndAttack PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
)

# ─────────────────────────────────────────────
# 头文件搜索路径（让 #include "core/Card.h" 这样写可以找到）
# ─────────────────────────────────────────────
target_include_directories(DffenseAndAttack PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ─────────────────────────────────────────────
# Windows 专用：发布时不弹黑色命令行窗口
# ─────────────────────────────────────────────
if(WIN32)
    set_target_properties(DffenseAndAttack PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
