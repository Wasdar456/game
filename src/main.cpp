/**
 * @file main.cpp
 * @brief 程序入口文件 —— 塔防对战游戏启动点
 *
 * 职责说明：
 *   1. 创建 QApplication 对象，初始化 Qt 应用程序运行环境
 *   2. 设置全局文本编码为 UTF-8，防止中文乱码（尤其 Windows）
 *   3. 创建主窗口 MainWindow 并显示
 *   4. 进入 Qt 事件循环 (app.exec())，等待用户交互
 *   本游戏采用"核心-界面"分离架构：
 *   - core/ 模块：纯 C++ 游戏逻辑（不依赖 Qt），包括 GameObject、Entity、Card、Monster、Map 等
 *   - ui/ 模块：Qt 界面层，通过 BattleSnapshot 只读快照获取核心层状态进行渲染
 *   - network/ 模块：PVP 联机通信，基于 QTcpServer/QTcpSocket
 *
 *   UI 层不直接修改 core 对象，而是通过 BattleManager 提供的接口
 *   （deployCard、upgradeCard、moveCard、recallCard）进行操作，
 *   然后从 BattleSnapshot 读取最新状态来刷新界面。
 */

#include <QApplication>        // Qt GUI 应用程序核心类
#include "ui/MainWindow.h"     // 主窗口头文件

int main(int argc, char *argv[])
{
    // ========== 第一步：创建 QApplication ==========
    // QApplication 管理整个 GUI 应用的资源、事件分发、窗口系统等
    // 每个 Qt GUI 程序有且仅有一个 QApplication 实例
    QApplication app(argc, argv);

    // ========== 第二步：设置全局编码 ==========
    // Qt6 默认使用 UTF-8 编码，无需手动设置 QTextCodec（Qt5 的方式）
    // 只需确保源文件以 UTF-8 编码保存即可

    // ========== 第三步：设置应用程序信息 ==========
    // 这些信息会出现在操作系统任务栏、窗口标题、关于对话框等位置
    app.setApplicationName("塔防对战");
    app.setApplicationVersion("0.1");
    app.setOrganizationName("GameTeam");

    // ========== 第四步：创建并显示主窗口 ==========
    // MainWindow 是所有页面的容器，内部使用 QStackedWidget 管理页面切换
    // 它持有 BattleManager 的实例，各子页面通过 MainWindow 获取核心层接口
    MainWindow window;
    window.show();  // 调用 show() 让窗口变为可见（默认创建时是隐藏的）

    // ========== 第五步：进入事件循环 ==========
    // exec() 启动 Qt 事件循环，程序在此处持续运行
    // 返回值为退出码，0 表示正常退出
    return app.exec();
}
