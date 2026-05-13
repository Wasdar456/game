cmake_minimum_required(VERSION 3.16)
project(NetworkTest VERSION 0.1 LANGUAGES CXX)

# ═══════════════════════════════════════════════════════════════
# C++ 标准 & Qt 自动化工具
# ═══════════════════════════════════════════════════════════════
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)   # 处理 Q_OBJECT 宏

# ═══════════════════════════════════════════════════════════════
# Qt 路径（你的 Qt 安装路径）
# ═══════════════════════════════════════════════════════════════
set(CMAKE_PREFIX_PATH "/Users/wangzihan/Desktop/Study/C++/Qt/6.11.0/macos")

find_package(Qt6 REQUIRED COMPONENTS Core Network)

# ═══════════════════════════════════════════════════════════════
# 收集 network 模块所有源文件 + 头文件（头文件必须加入，AUTOMOC 才会处理）
# ═══════════════════════════════════════════════════════════════
file(GLOB_RECURSE NETWORK_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../network/src/*.cpp"
)
file(GLOB_RECURSE NETWORK_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/../network/include/network/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../network/include/network/*.hpp"
)

# ═══════════════════════════════════════════════════════════════
# 生成测试可执行文件
# ═══════════════════════════════════════════════════════════════
add_executable(NetworkTest
    main.cpp
    ${NETWORK_SOURCES}
    ${NETWORK_HEADERS}
)

target_include_directories(NetworkTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../network/include"
    # AUTOMOC 生成的 .moc 文件路径，必须加入搜索才能被 #include "ClassName.moc" 找到
    "${CMAKE_CURRENT_BINARY_DIR}/NetworkTest_autogen/include_Debug"
)

target_link_libraries(NetworkTest PRIVATE
    Qt6::Core
    Qt6::Network
)

# ═══════════════════════════════════════════════════════════════
# 打印提示
# ═══════════════════════════════════════════════════════════════
message(STATUS "")
message(STATUS "网络模块独立测试程序")
message(STATUS "  运行方式：./NetworkTest server   启动服务器 (Host)")
message(STATUS "           ./NetworkTest client   启动客户端 (Client)")
message(STATUS "")
