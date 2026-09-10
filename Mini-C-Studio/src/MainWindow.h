// ============================================================================
// 文件名：MainWindow.h
// 职责：主窗口类 —— Mini-C Studio 的全部界面绘制与交互控制
// 负责人：C（GUI 界面 + 交互控制）
//
// 覆盖 PPT 52 页"图形交互与状态反馈模块"：
//   1. 菜单栏：新建/打开/保存/编辑/查找替换/编译/运行/帮助
//   2. 状态栏：文件名、行号列号、修改状态、编译状态
//   3. 弹窗提示：文件读写失败、编译器未找到、源码为空、运行超时等
// 加分项（PPT 54 页）：语法高亮、撤销重做、亮/暗主题切换
// ============================================================================

#pragma once
#ifndef MINIC_MAINWINDOW_H
#define MINIC_MAINWINDOW_H

#include "MiniCConfig.h"
#include "CoreApi.h"
#include <string>
#include <vector>

class MainWindow
{
public:
    // 命令编号（菜单栏 / 工具栏 / 快捷键 共用同一套）
    enum Cmd
    {
        CMD_NONE = 0,
        // 文件
        CMD_FILE_NEW, CMD_FILE_OPEN, CMD_FILE_SAVE, CMD_FILE_SAVEAS, CMD_FILE_CLOSE, CMD_EXIT,
        // 编辑
        CMD_EDIT_UNDO, CMD_EDIT_REDO, CMD_EDIT_CUT, CMD_EDIT_COPY, CMD_EDIT_PASTE, CMD_EDIT_SELALL,
        // 查找替换
        CMD_FIND, CMD_REPLACE, CMD_FIND_NEXT, CMD_FIND_PREV, CMD_REPLACE_ONE, CMD_REPLACE_ALL,
        // 编译运行
        CMD_COMPILE, CMD_RUN, CMD_COMPILE_RUN, CMD_STOP,
        // 视图 / 帮助
        CMD_THEME, CMD_ABOUT
    };

    MainWindow();
    ~MainWindow();

    bool initWindow(int w, int h);
    void run();
    int  width()  const { return m_w; }
    int  height() const { return m_h; }

private:
    // ---------------- 焦点区域 ----------------
    enum Focus { FOCUS_EDITOR = 0, FOCUS_FIND, FOCUS_REPLACE, FOCUS_CONSOLE };

    // ---------------- 菜单结构 ----------------
    struct MenuItem { std::string label; int cmd; std::string hot; };
    struct MenuDef  { std::string title; std::vector<MenuItem> items; };
    struct ToolBtn  { std::string label; int cmd; };
    struct FindLayout { Rect bar, fInput, rInput, chk, bPrev, bNext, bRep, bRepAll, bClose; };

    // ---------------- 窗口与主题 ----------------
    int  m_w, m_h;
    bool m_running;
    bool m_dark;
    const Theme* th;
    int  m_charW;            // 等宽字体单个字符宽度
    int  m_tick;             // 帧计数（光标闪烁用）
    int  m_mouseX, m_mouseY; // 鼠标位置（悬停效果用）

    // ---------------- 编辑器视图状态 ----------------
    int  m_topLine;          // 第一行可见行的行号
    int  m_leftCol;          // 水平滚动偏移（显示列）
    bool m_selActive;        // 是否存在选区
    int  m_selR1, m_selC1, m_selR2, m_selC2;   // 选区（已归一化）
    int  m_anchorR, m_anchorC;                 // 选区锚点（鼠标拖拽用）
    bool m_dragging;
    bool m_dragV, m_dragH;   // 是否正在拖动滚动条
    int  m_dragStart;
    Focus m_focus;
    std::string m_dbcsPending;

    // 块注释状态缓存（避免每帧都从文件头扫描一遍）
    int  m_blockTopCache;
    bool m_blockValCache;
    int  m_blockFrameCache;

    // ---------------- 菜单 / 工具栏状态 ----------------
    std::vector<MenuDef> m_menus;
    std::vector<ToolBtn> m_tools;
    int  m_openMenu;         // 当前展开的菜单下标，-1 表示无
    int  m_hoverMenu;
    int  m_hoverItem;
    int  m_hoverTool;

    // ---------------- 底部面板 ----------------
    int  m_bottomTab;                    // 0 = 诊断, 1 = 控制台
    std::vector<Diagnostic> m_diagnostics;
    int  m_diagTop;
    std::vector<std::string> m_console;  // 控制台输出行
    int  m_consoleTop;
    std::string m_inputLine;             // 控制台输入行
    bool m_progRunning;

    // ---------------- 查找 / 替换 ----------------
    bool m_findVisible;
    std::string m_findText, m_replaceText;
    bool m_caseSensitive;
    bool m_hasMatch;
    int  m_matchR, m_matchC, m_matchLen;

    // ---------------- 编译 / 运行状态 ----------------
    CompileState m_compileState;
    bool  m_compiling;
    std::string m_lastExe;
    std::string m_statusMsg;
    COLORREF    m_statusColor;

    // ================= 布局（唯一坐标来源，绘制与命中测试共用） =================
    Rect rMenu()   const;
    Rect rTool()   const;
    Rect rEdit()   const;
    Rect rBottom() const;
    Rect rStatus() const;
    Rect rGutter() const;
    Rect rText()   const;
    Rect rVScroll()const;
    Rect rHScroll()const;

    // 水平滚动条辅助：total = 最长行的显示列数；page = 一屏能显示的列数
    int  maxLineCols();
    int  hPageCols();
    void clampScroll();   // 每帧把 m_topLine / m_leftCol 钳回合法范围
    Rect rFindBar()const;
    Rect rBottomTabs()  const;
    Rect rBottomBody()  const;
    Rect rBottomScroll()const;
    Rect rConsoleBody() const;
    Rect rConsoleInput()const;
    Rect dropRect(int mi) const;
    Rect dropItemRect(int mi, int ii) const;
    Rect toolBtnRect(int i) const;
    FindLayout findLayout() const;
    std::string tabLabel(int i) const;   // 底部面板标签文字（绘制与命中测试共用）

    int  visibleLines() const;

    // ================= 消息 =================
    void pumpMessages();
    void onMouseDown(int x, int y);
    void onMouseUp(int x, int y);
    void onMouseMove(int x, int y);
    void onWheel(int delta);
    void onKeyDown(int vk, bool ctrl, bool shift, bool alt);
    void onCharInput(unsigned int code);

    // ================= 绘制 =================
    void render();
    void drawMenuBar();
    void drawDropdown();
    void drawToolbar();
    void drawEditor();
    void drawCodeLine(int x, int y, int row, const Rect& clip, bool inBlock);
    void drawFindBar();
    void drawBottom();
    void drawDiagnostics();
    void drawConsole();
    void drawStatusBar();
    void drawScrollbar(const Rect& r, int pos, int total, int page, bool vertical);

    // ================= 命令与业务 =================
    void execCmd(int cmd);
    bool confirmDiscard();
    bool doSave();
    bool doSaveAs();
    bool doSaveTo(const std::string& path);
    void doOpen();
    void doCloseDoc();
    void doCompile();
    void doRun();
    void doStop();
    void doExit();

    // ================= 编辑辅助 =================
    void clearSelection();
    void normalizeSel();
    bool hasSelection() const;
    std::string selectedText() const;
    void deleteSelection();
    void insertTextAtCursor(const std::string& s);
    void screenToDoc(int x, int y, int& row, int& col);
    void scrollToCursor();
    void setStatus(const std::string& msg, COLORREF c);
    void setStatus(const std::string& msg);
    void appendConsole(const std::string& text);

    // ================= 查找替换 =================
    bool searchNext(bool forward);
    void replaceOne();
    void replaceAll();

    // ================= 剪贴板 =================
    static void setClipboardText(const std::string& s);
    static std::string getClipboardText();

    // ================= 输入法（IME）支持（中文输入） =================
    void imeCommit(const std::string& gbk);   // 把一段 GBK 文本提交到当前焦点
    void updateImePosition();                 // 让 IME 候选窗跟随光标
    static MainWindow* s_self;                // 供窗口子类回调
    static WNDPROC     s_oldProc;             // EasyX 原有的窗口过程
    static LRESULT CALLBACK imeWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    DWORD m_imeGuardUntil;                 // IME 提交后的去重保护窗口

    void buildMenus();
    void update();
    void loadWelcomeContent();
};

#endif // MINIC_MAINWINDOW_H
