// ============================================================================
// 文件名：MainWindow.h
// 负责人：组长 A（骨架） + C 同学（绘制与交互，本文件在 A 的基础上【只追加】）
//
// 变更说明（给 A 看）：
//   · A 原有的数据成员、成员函数【一个都没有删、没有改名】；
//   · C 需要的东西全部追加在文件末尾的
//     "===== 以下由 C 同学追加 =====" 之后；
//   · A 的 6 个绘制函数 / 2 个事件函数 / 5 个按钮回调，
//     函数体由 C 重新实现（A 的原实现已完整保留在
//     《A文件改动对照.md》里，可随时对照），签名一个没变。
//
// 覆盖 PPT 52 页"图形交互与状态反馈模块"：
//   菜单栏 / 工具栏 / 编辑区渲染 / 查找替换 / 诊断面板 / 控制台 / 状态栏 / 弹窗
// 加分项（PPT 54–55 页）：语法高亮、撤销重做、亮暗主题切换、启动欢迎界面
// ============================================================================

#pragma once

#include <graphics.h>
#include <string>
#include <vector>

#include "EditorBuffer.h"
#include "MiniCConfig.h"
#include "CoreTypes.h"
#include "CoreApi.h"
#include "EditorExt.h"

// 前向声明（这些类由 E / D 同学后续实现）
class FileManager;
class Compiler;
class Runtime;

class MainWindow
{
private:
    // ---------- 窗口属性（A 原有） ----------
    int windowWidth;
    int windowHeight;

    // ---------- 核心模块指针（A 原有） ----------
    EditorBuffer* buffer;   // B同学负责
    FileManager*  fileMgr;  // E同学负责
    Compiler*     compiler; // D同学负责
    Runtime*      runtime;  // D同学负责

    // ---------- 界面状态（A 原有） ----------
    std::string outputPanelText;
    bool isCompiling;

    // ---------- 私有方法（A 原有，函数体由 C 实现） ----------
    void drawAll();
    void drawButtons();
    void drawEditor();
    void drawStatusBar();
    void drawOutputPanel();

    void handleMouseClick(int x, int y);
    void handleKeyPress(int key);

public:
    // ---------- 构造 / 初始化（A 原有） ----------
    MainWindow();
    ~MainWindow();

    void initWindow();
    void mainLoop();

    // ---------- 按钮事件回调（A 原有） ----------
    void onNewFile();
    void onOpenFile();
    void onSaveFile();
    void onCompile();
    void onRun();

    // ========================================================================
    //  ===== 以下由 C 同学追加 =====
    // ========================================================================
private:
    // ---------------- 命令编号（菜单 / 工具栏 / 快捷键 共用） ----------------
    enum Cmd
    {
        CMD_NONE = 0,
        // 文件
        CMD_FILE_NEW, CMD_FILE_OPEN, CMD_FILE_SAVE, CMD_FILE_SAVEAS, CMD_EXIT,
        // 编辑
        CMD_EDIT_UNDO, CMD_EDIT_REDO, CMD_EDIT_CUT, CMD_EDIT_COPY,
        CMD_EDIT_PASTE, CMD_EDIT_SELALL,
        // 查找替换
        CMD_FIND, CMD_REPLACE, CMD_FIND_NEXT, CMD_FIND_PREV,
        CMD_REPLACE_ONE, CMD_REPLACE_ALL,
        // 编译运行
        CMD_COMPILE, CMD_RUN, CMD_COMPILE_RUN, CMD_STOP,
        // 视图 / 帮助
        CMD_THEME, CMD_ABOUT
    };

    // ---------------- 焦点区域 ----------------
    enum Focus { FOCUS_EDITOR = 0, FOCUS_FIND, FOCUS_REPLACE, FOCUS_CONSOLE };

    struct MenuItem { std::string label; int cmd; std::string hot; };
    struct MenuDef  { std::string title; std::vector<MenuItem> items; };
    struct ToolBtn  { std::string label; int cmd; };
    struct FindLayout { Rect bar, fInput, rInput, chk, bPrev, bNext, bRep, bRepAll, bClose; };

    // ---------------- 主题与帧状态 ----------------
    bool        m_dark;
    const Theme* th;
    int         m_charW;        // 等宽字体单字符宽度
    int         m_tick;         // 帧计数（光标闪烁）
    int         m_mouseX, m_mouseY;

    // ---------------- 编辑器视图状态 ----------------
    int  m_topLine;             // 第一行可见行
    int  m_leftCol;             // 水平滚动偏移（显示列）
    bool m_selActive;
    int  m_selR1, m_selC1, m_selR2, m_selC2;   // 选区（已归一化）
    int  m_anchorR, m_anchorC;                 // 拖拽锚点
    bool m_dragging;
    bool m_dragV, m_dragH;
    bool m_dragInBottom;   // 当前拖的是底部面板的滚动条还是编辑区的
    int  m_dragStart;
    int  m_statusUntil;    // 状态栏临时提示显示到第几帧
    Focus m_focus;
    std::string m_dbcsPending;  // 中文双字节未凑齐的那一半
    int  m_wantCol;             // 连续按上下键时记住的原始列

    // 块注释状态缓存（避免每帧从文件头扫一遍）
    int  m_blockTopCache;
    bool m_blockValCache;
    int  m_blockFrameCache;

    // ---------------- 菜单 / 工具栏 ----------------
    std::vector<MenuDef> m_menus;
    std::vector<ToolBtn> m_tools;
    int m_openMenu;
    int m_hoverMenu, m_hoverItem, m_hoverTool;

    // ---------------- 底部面板 ----------------
    int  m_bottomTab;                   // 0=诊断 1=控制台
    std::vector<Diagnostic> m_diagnostics;
    int  m_diagTop;
    std::vector<std::string> m_console;
    int  m_consoleTop;
    std::string m_inputLine;
    bool m_progRunning;

    // ---------------- 查找 / 替换 ----------------
    bool m_findVisible;
    std::string m_findText, m_replaceText;
    bool m_caseSensitive;
    bool m_hasMatch;
    int  m_matchR, m_matchC, m_matchLen;

    // ---------------- 编译 / 运行 ----------------
    CompileState m_compileState;
    std::string  m_lastExe;
    std::string  m_statusMsg;
    COLORREF     m_statusColor;

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
    Rect rFindBar()const;
    Rect rBottomTabs()   const;
    Rect rBottomBody()   const;
    Rect rBottomScroll() const;
    Rect rConsoleBody()  const;
    Rect rConsoleInput() const;
    Rect dropRect(int mi) const;
    Rect dropItemRect(int mi, int ii) const;
    Rect toolBtnRect(int i) const;
    FindLayout findLayout() const;
    std::string tabLabel(int i) const;

    int visibleLines() const;

    // ================= 消息分发 =================
    void onMouseUp(int x, int y);
    void onMouseMove(int x, int y);
    void onWheel(int delta);
    void onKeyDown(int vk, bool ctrl, bool shift);
    void onCharInput(unsigned int code);
    void onCommand(int cmd);

    // ================= 绘制 =================
    void drawMenuBar();
    void drawDropdown();
    void drawToolbar();
    void drawCodeLine(int x, int y, int row, const Rect& clip, bool& inBlock);
    void drawFindBar();
    void drawBottom();
    void drawDiagnostics();
    void drawConsole();
    void drawScrollbar(const Rect& r, int pos, int total, int page, bool vertical);

    // ================= 业务 =================
    bool confirmDiscard();
    bool doSave();
    bool doSaveAs();
    bool doSaveTo(const std::string& path);
    void doOpen();
    void doCompile();
    void doRun();
    void doStop();
    void doExit();

    // ================= 编辑辅助 =================
    void clearSel();
    void normalizeSel();
    bool hasSel() const;
    void syncSelToBuffer();
    void deleteSel();
    void insertAtCursor(const std::string& s);
    void screenToDoc(int x, int y, int& row, int& col);
    void scrollToCursor();
    void setStatus(const std::string& msg, COLORREF c);
    void appendConsole(const std::string& text);

    // ================= 查找替换 =================
    bool searchNext(bool forward);
    void replaceOne();
    void replaceAll();

    // ================= 剪贴板 =================
    static void setClipboardText(const std::string& s);
    static std::string getClipboardText();

    void buildMenus();
    void loadWelcomeContent();
};
