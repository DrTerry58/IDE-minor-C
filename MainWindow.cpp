// ============================================================
文件名：MainWindow.cpp
// 职责：实现主窗口的绘制和消息循环
// 负责人：组长 A （C同学协助完善绘制细节）
// ============================================================

#include "MainWindow.h"
#include <cstdio>   // 用于 sprintf
#include <string>
#include <windows.h>

// ---------- 构造函数：初始化默认值 ----------
MainWindow::MainWindow()
    : windowWidth(800), windowHeight(600),
      buffer(new EditorBuffer()),     // 等B同学写好了再 new
      fileMgr(nullptr),     // 等E同学写好了再 new
      compiler(nullptr),    // 等D同学写好了再 new
      runtime(nullptr),     // 等D同学写好了再 new
      outputPanelText("欢迎使用 Mini-C-Studio！"),
      isCompiling(false)
{
    // 目前什么都不做，等队友的模块完成了再初始化
}

MainWindow::~MainWindow()
{
    // 释放内存（暂时空着，等队友的模块完成再补）
}

// ---------- 初始化图形窗口 ----------
void MainWindow::initWindow()
{
    // 创建 800x600 的图形窗口，右下角显示控制台（方便调试打印）
    initgraph(windowWidth, windowHeight, EW_SHOWCONSOLE);

    // 设置窗口标题
    SetConsoleTitle("Mini-C-Studio - 调试控制台");
}

// ---------- 主消息循环（死循环） ----------
void MainWindow::mainLoop()
{
    ExMessage msg;  // EasyX 的消息结构体

    while (true)
    {
        // 1. 处理消息（非阻塞，有消息才处理，没消息就继续）
        if (peekmessage(&msg, EX_MOUSE | EX_KEY))
        {
            if (msg.message == WM_LBUTTONDOWN)
            {
                // 鼠标左键按下
                handleMouseClick(msg.x, msg.y);
            }
            else if (msg.message == WM_KEYDOWN)
            {
                // 键盘按键按下（只处理普通字符和方向键）
                handleKeyPress(msg.vkcode);
            }
        }

        // 2. 绘制整个界面
        drawAll();

        // 3. 让 CPU 休息 20 毫秒，避免占用过高
        Sleep(20);
    }
}

// ---------- 绘制全部界面 ----------
void MainWindow::drawAll()
{
    // 清屏为白色背景
    setbkcolor(WHITE);
    cleardevice();

    // 按顺序绘制各个部分（图层从下往上）
    drawButtons();      // 画顶部按钮
    drawEditor();       // 画编辑区
    drawStatusBar();    // 画状态栏
    drawOutputPanel();  // 画输出面板

    // 刷新绘图（EasyX 自动刷新，不需要手动）
}

// ---------- 绘制顶部按钮 ----------
void MainWindow::drawButtons()
{
    // 设置按钮样式：灰色填充，黑色边框
    setfillcolor(LIGHTGRAY);
    setlinecolor(BLACK);
    settextcolor(BLACK);
    settextstyle(16, 0, _T("宋体"));

    // 按钮坐标和文字（x, y, 宽, 高）
    struct Button { int x, y, w, h; const char* text; };
    Button btns[] = {
        {10, 10, 70, 35, "新建"},
        {90, 10, 70, 35, "打开"},
        {170, 10, 70, 35, "保存"},
        {250, 10, 70, 35, "编译"},
        {330, 10, 70, 35, "运行"}
    };

    for (int i = 0; i < 5; i++)
    {
        // 画矩形按钮
        fillrectangle(btns[i].x, btns[i].y,
                      btns[i].x + btns[i].w, btns[i].y + btns[i].h);
        // 画文字（居中粗略处理）
        outtextxy(btns[i].x + 10, btns[i].y + 8, btns[i].text);
    }

    // 画一条分割线，把按钮区和编辑区隔开
    setlinecolor(BLACK);
    line(10, 55, windowWidth - 10, 55);
}

// ---------- 绘制编辑区（当前是占位符） ----------
void MainWindow::drawEditor()
{
    // 先画一个白色矩形作为编辑区背景
    setfillcolor(WHITE);
    setlinecolor(BLACK);
    fillrectangle(10, 60, windowWidth - 10, windowHeight - 100);

    // ---------- 等待 B 同学完成 EditorBuffer 后替换此处 ----------
    // 目前硬编码显示几行示例文字，让界面不那么空
    settextcolor(BLACK);
    settextstyle(18, 0, _T("Consolas"));

    // 显示占位提示
    outtextxy(20, 70, _T("// 这里将显示代码内容"));
    outtextxy(20, 95, _T("// 等待 B 同学实现 EditorBuffer 后接入"));
    outtextxy(20, 120, _T("// 目前是 组长 A 画的骨架占位符"));

    // 显示一个模拟光标（闪烁竖线）
    static int blink = 0;
    blink++;
    if (blink % 30 < 15)  // 简单闪烁
    {
        setlinecolor(BLACK);
        line(20, 145, 20, 165);  // 在第二行末尾画竖线
    }
}

// ---------- 绘制状态栏 ----------
void MainWindow::drawStatusBar()
{
    int y = windowHeight - 70;

    // 灰色背景条
    setfillcolor(LIGHTGRAY);
    setlinecolor(BLACK);
    fillrectangle(0, y, windowWidth, windowHeight - 40);

    settextcolor(BLACK);
    settextstyle(14, 0, _T("宋体"));

    // 显示信息：文件名 | 行:列 | 修改状态
    char status[256];
    sprintf_s(status, " 未命名.c  |  行:1  列:1  |  已保存");
    outtextxy(10, y + 8, status);
}

// ---------- 绘制输出面板 ----------
void MainWindow::drawOutputPanel()
{
    int y = windowHeight - 40;

    // 黑色背景的输出面板
    setfillcolor(BLACK);
    setlinecolor(BLACK);
    fillrectangle(0, y, windowWidth, windowHeight);

    settextcolor(LIGHTGRAY);
    settextstyle(14, 0, _T("Consolas"));

    // 显示 outputPanelText 的内容
    outtextxy(10, y + 8, outputPanelText.c_str());
}

// ---------- 处理鼠标点击 ----------
void MainWindow::handleMouseClick(int x, int y)
{
    // 判断点击了哪个按钮（坐标范围匹配）
    // 按钮坐标: 新建(10,10,70,35)  打开(90,10,70,35) ...
    if (x >= 10 && x <= 80 && y >= 10 && y <= 45)
    {
        onNewFile();
    }
    else if (x >= 90 && x <= 160 && y >= 10 && y <= 45)
    {
        onOpenFile();
    }
    else if (x >= 170 && x <= 240 && y >= 10 && y <= 45)
    {
        onSaveFile();
    }
    else if (x >= 250 && x <= 320 && y >= 10 && y <= 45)
    {
        onCompile();
    }
    else if (x >= 330 && x <= 400 && y >= 10 && y <= 45)
    {
        onRun();
    }
    else
    {
        // 点击编辑区：由 C 同学实现光标定位
        // 当前暂不处理
    }
}

// ---------- 处理键盘按键 ----------
void MainWindow::handleKeyPress(int key)
{
    // 当前只是占位，等 B 和 C 同学完成编辑逻辑后接入
    char msg[256];
    sprintf_s(msg, "按下了按键: %d (等待B/C同学接入编辑)", key);
    outputPanelText = msg;

    // 注意：EasyX 的按键码 VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN
    // 字母直接传 char 即可
}

// ---------- 按钮事件回调（空壳，等待后续填充） ----------
void MainWindow::onNewFile()
{
    outputPanelText = "新建文件 (等待E同学实现文件管理)";
}

void MainWindow::onOpenFile()
{
    outputPanelText = "打开文件 (等待E同学实现文件管理)";
}

void MainWindow::onSaveFile()
{
    outputPanelText = "保存文件 (等待E同学实现文件管理)";
}

void MainWindow::onCompile()
{
    outputPanelText = "编译中... (等待D同学实现编译调度)";
}

void MainWindow::onRun()
{
    outputPanelText = "运行中... (等待D同学实现运行时托管)";
}
