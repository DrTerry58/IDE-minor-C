// ============================================================
// 文件名：MainWindow.h
// 职责：声明主窗口类，包含所有界面元素和模块接口
// 负责人：组长 A （C同学协助完善绘制部分）
// ============================================================

#pragma once

// 包含 EasyX 图形库（VS Code 需要配置路径，VS可直接用）
#include <graphics.h>
#include <string>
#include "EditorBuffer.h"

// 前向声明：告诉编译器这些类存在，具体实现在别的 .cpp 里
// （这些类由 B、C、D、E 同学后续实现）
class EditorBuffer;
class FileManager;
class Compiler;
class Runtime;

class MainWindow
{
private:
    // ---------- 窗口属性 ----------
    int windowWidth;
    int windowHeight;

    // ---------- 核心模块指针（组合关系） ----------
    // 注意：这里先用指针，等队友的类写好了再 new 出来
    EditorBuffer* buffer;   // B同学负责
    FileManager* fileMgr;   // E同学负责
    Compiler* compiler;     // D同学负责
    Runtime* runtime;       // D同学负责

    // ---------- 界面状态 ----------
    std::string outputPanelText;   // 输出面板显示的文字
    bool isCompiling;             // 是否正在编译（防重复点击）

    // ---------- 私有方法（内部使用） ----------
    void drawAll();              // 画全部界面
    void drawButtons();          // 画顶部按钮
    void drawEditor();           // 画编辑区（调用B的缓冲区）
    void drawStatusBar();        // 画底部状态栏
    void drawOutputPanel();      // 画输出面板

    void handleMouseClick(int x, int y);  // 处理鼠标点击
    void handleKeyPress(int key);         // 处理键盘按键

public:
    // ---------- 构造函数 / 初始化 ----------
    MainWindow();
    ~MainWindow();

    void initWindow();           // 初始化图形窗口
    void mainLoop();             // 主消息循环（死循环）

    // ---------- 按钮事件回调（由鼠标点击触发） ----------
    void onNewFile();            // 新建
    void onOpenFile();           // 打开
    void onSaveFile();           // 保存
    void onCompile();            // 编译
    void onRun();                // 运行
};
