// ============================================================
//文件名：MainWindow.cpp
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
    delete buffer;
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
    // 画白色背景框（不变）
    setfillcolor(WHITE);
    setlinecolor(BLACK);
    fillrectangle(10, 60, windowWidth - 10, windowHeight - 100);

    // ---------- 以下是替换后的核心绘制代码 ----------
    settextcolor(BLACK);
    settextstyle(18, 0, _T("Consolas"));

    int lineCount = buffer->getLineCount();
    for (int i = 0; i < lineCount; i++)
    {
        std::string line = buffer->getLine(i);
        outtextxy(20, 70 + i * 20, line.c_str());
    }

    // 画光标（闪烁竖线）
    static int blink = 0;
    blink++;
    if (blink % 30 < 15)
    {
        int cx = buffer->getCursorX();
        int cy = buffer->getCursorY();
        int screenX = 20 + cx * 10;   // 每个字符约10像素宽
        int screenY = 70 + cy * 20;   // 每行20像素高
        setlinecolor(BLACK);
        line(screenX, screenY, screenX, screenY + 20);
    }
}

// ---------- 绘制状态栏 ----------
void MainWindow::drawStatusBar()
{
    int y = windowHeight - 70;

    setfillcolor(LIGHTGRAY);
    setlinecolor(BLACK);
    fillrectangle(0, y, windowWidth, windowHeight - 40);

    settextcolor(BLACK);
    settextstyle(14, 0, _T("宋体"));

    // 从buffer读取真实数据
    int row = buffer->getCursorY() + 1;   // B的是0-based，显示要+1
    int col = buffer->getCursorX() + 1;
    const char* dirtyFlag = buffer->isDirty() ? "*" : " ";
    std::string fname = buffer->getFilePath();
    if (fname.empty()) fname = "未命名.c";

    char status[256];
    sprintf_s(status, " %s  |  行:%d  列:%d  |  %s", 
              fname.c_str(), row, col, dirtyFlag);
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
    // 可打印字符（字母/数字/符号）：直接插入
    if (key >= 32 && key <= 126)
    {
        buffer->insertChar((char)key);
        return;
    }

    // 特殊按键
    switch (key)
    {
    case VK_BACK:    // 退格键
        buffer->deleteChar();
        break;
    case VK_DELETE:  // Delete键
        buffer->deleteForward();
        break;
    case VK_RETURN:  // 回车键
        buffer->enter();
        break;
    case VK_LEFT:
        buffer->moveCursor(-1, 0);
        break;
    case VK_RIGHT:
        buffer->moveCursor(1, 0);
        break;
    case VK_UP:
        buffer->moveCursor(0, -1);
        break;
    case VK_DOWN:
        buffer->moveCursor(0, 1);
        break;
    default:
        // 其他按键（如F1-F12）暂不处理
        break;
    }
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
    // 1. 从B拿到全部文本
    std::string content = buffer->saveToString();
    
    // 2. 获取保存路径
    std::string path = buffer->getFilePath();
    if (path.empty())
    {
        path = "temp.c";  // 临时文件名，后面可以加对话框
        buffer->setFilePath(path);
    }

    // 3. 写入文件（标准C++方式，不依赖E同学）
    FILE* fp = fopen(path.c_str(), "w");
    if (fp)
    {
        fwrite(content.c_str(), 1, content.size(), fp);
        fclose(fp);
        buffer->setDirty(false);
        outputPanelText = "保存成功: " + path;
    }
    else
    {
        outputPanelText = "保存失败！无法创建文件: " + path;
    }
}

void MainWindow::onCompile()
{
    onSaveFile();

    if (GetFileAttributesA("test.exe") != INVALID_FILE_ATTRIBUTES)
    {
        DeleteFileA("test.exe");
    }

    std::string srcFile = buffer->getFilePath();
    if (srcFile.empty())
    {
        srcFile = "test.c";
        buffer->setFilePath(srcFile);
    }

    char cmd[512];
    sprintf_s(cmd, "gcc \"%s\" -o test.exe 2> error.txt", srcFile.c_str());

    outputPanelText = "正在编译... 请稍候";
    drawAll();

    int ret = system(cmd);

    std::ifstream errFile("error.txt");
    std::stringstream ss;

    if (errFile.is_open())
    {
        std::string line;
        bool hasError = false;

        while (std::getline(errFile, line))
        {
            if (line.empty()) continue;
            ss << line << "\n";
            if (line.find("error:") != std::string::npos)
                hasError = true;
        }
        errFile.close();

        if (ret == 0 && !hasError)
        {
            outputPanelText = "编译成功！\n" + ss.str();
        }
        else
        {
            outputPanelText = "编译失败！请检查代码错误：\n" + ss.str();
        }
    }
    else
    {
        outputPanelText = "错误：无法获取编译输出。请确认 gcc 已安装并配置好环境变量。";
    }

    if (ret != 0 && outputPanelText.find("编译成功") == std::string::npos)
    {
        if (GetFileAttributesA("error.txt") == INVALID_FILE_ATTRIBUTES)
        {
            outputPanelText = "错误：找不到 gcc 编译器。请确认 MinGW 已安装并加入 PATH。";
        }
    }
}

void MainWindow::onRun()
{
    if (GetFileAttributesA("test.exe") == INVALID_FILE_ATTRIBUTES)
    {
        outputPanelText = "错误：没有找到 test.exe，请先点击“编译”生成可执行文件。";
        return;
    }

    outputPanelText = "正在运行程序...";
    drawAll();

    system("test.exe > output.txt 2>&1");

    std::ifstream outFile("output.txt");
    std::stringstream ss;

    if (outFile.is_open())
    {
        std::string line;
        bool hasOutput = false;

        while (std::getline(outFile, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            ss << line << "\n";
            hasOutput = true;
        }
        outFile.close();

        if (hasOutput)
        {
            outputPanelText = "程序输出：\n" + ss.str();
        }
        else
        {
            outputPanelText = "程序运行完成（没有输出到控制台）。";
        }
    }
    else
    {
        outputPanelText = "错误：无法读取程序输出。";
    }
}