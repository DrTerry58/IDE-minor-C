// ============================================================================
// 文件名：main.cpp
// 职责：程序入口 —— 先显示启动欢迎界面（2 秒），再进入主窗口
// 负责人：C（GUI 界面 + 交互控制）
//
// 与 A 同学原版 main.cpp 的关系：
//   原版：MainWindow app; app.initWindow(); app.mainLoop();
//   新版：多了一步 showSplashScreen()，并把 mainLoop 更名为 run（语义不变）。
//   如果组长希望保持 mainLoop 这个名字，在 MainWindow.h 里加一行
//   `void mainLoop() { run(); }` 即可，无需改其它代码。
// ============================================================================

#include "MiniCConfig.h"
#include "SplashScreen.h"
#include "MainWindow.h"
#include <windows.h>

// 根据屏幕大小自适应选择一个合适的窗口尺寸
static void pickWindowSize(int& w, int& h)
{
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    w = MINIC_DEF_W;
    h = MINIC_DEF_H;

    if (w > sw - 60)  w = sw - 60;
    if (h > sh - 100) h = sh - 100;
    if (w < 800) w = 800;      // 再小就放不下工具栏了
    if (h < 560) h = 560;
}

int main()
{
    // 1. 启动欢迎界面：LOGO + 进度条，默认 2 秒（点击或按键可跳过）
    showSplashScreen(MINIC_SPLASH_MS);

    // 2. 创建并初始化主窗口
    int w = 0, h = 0;
    pickWindowSize(w, h);

    MainWindow app;
    if (!app.initWindow(w, h))
        return -1;

    // 设置调试控制台窗口标题（必须在 initgraph 创建控制台之后）
    SetConsoleTitleW(s2w("Mini-C-Studio - 调试控制台").c_str());

    // 3. 进入主消息循环（内部是死循环，直到选择"退出"或关闭窗口）
    app.run();

    closegraph();
    return 0;
}
