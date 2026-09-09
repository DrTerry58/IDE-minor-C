// ============================================================================
// 文件名：MainWindow.cpp
// 负责人：组长 A（骨架） + C 同学（绘制与交互）
//
// 本文件里 A 原有的函数【签名全部保留】，函数体由 C 重新实现，
// 用来完成 PPT 52 页的"图形交互与状态反馈模块"。
// A 的原始实现逐条保存在 《A文件改动对照.md》 中，可随时比对。
// ============================================================================

#include "MainWindow.h"
#include "UiDialog.h"
#include "CoreApi.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cmath>
#include <algorithm>

// 小工具：int -> string（不用非标准的 itoa）
static std::string itos(int v)
{
    char buf[32];
    sprintf_s(buf, "%d", v);
    return std::string(buf);
}

// 判断两个坐标的先后顺序：返回 -1 表示 a<b，0 相等，1 表示 a>b
static int cmpPos(int ar, int ac, int br, int bc)
{
    if (ar != br) return ar < br ? -1 : 1;
    if (ac != bc) return ac < bc ? -1 : 1;
    return 0;
}

// 把 D 同学返回的 CompileResult 映射成 C 的 UI 状态机 CompileState
// （D 的类型用 success / errorCount / warningCount 表达，这里转成 C 的枚举）
static CompileState compileStateOf(const CompileResult& r)
{
    if (r.success)
        return r.warningCount > 0 ? CS_WARNING : CS_OK;
    if (r.errorCount > 0)
        return CS_ERROR;
    return CS_NOCOMPILER;   // 没成功也没错误：通常是找不到 gcc（D 在 rawOutput 里说明）
}

// ============================================================================
//  构造 / 析构（A 原有）
// ============================================================================
MainWindow::MainWindow()
    : windowWidth(800), windowHeight(600),
      buffer(new EditorBuffer()),
      fileMgr(nullptr),
      compiler(nullptr),
      runtime(nullptr),
      outputPanelText("欢迎使用 Mini-C-Studio！"),
      isCompiling(false),
      m_dark(false), th(&themeLight()),
      m_charW(8), m_tick(0), m_mouseX(-1), m_mouseY(-1),
      m_topLine(0), m_leftCol(0),
      m_selActive(false), m_selR1(0), m_selC1(0), m_selR2(0), m_selC2(0),
      m_anchorR(0), m_anchorC(0), m_dragging(false),
      m_dragV(false), m_dragH(false), m_dragInBottom(false), m_dragStart(0),
      m_statusUntil(0),
      m_focus(FOCUS_EDITOR), m_wantCol(-1),
      m_blockTopCache(0), m_blockValCache(false), m_blockFrameCache(-1),
      m_openMenu(-1), m_hoverMenu(-1), m_hoverItem(-1), m_hoverTool(-1),
      m_bottomTab(0), m_diagTop(0), m_consoleTop(0), m_progRunning(false),
      m_findVisible(false), m_caseSensitive(false),
      m_hasMatch(false), m_matchR(0), m_matchC(0), m_matchLen(0),
      m_compileState(CS_NONE), m_statusColor(RGB(0, 0, 0))
{
    buildMenus();

    m_tools.push_back(ToolBtn()); m_tools.back().label = "新建"; m_tools.back().cmd = CMD_FILE_NEW;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "打开"; m_tools.back().cmd = CMD_FILE_OPEN;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "保存"; m_tools.back().cmd = CMD_FILE_SAVE;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "|";    m_tools.back().cmd = CMD_NONE;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "撤销"; m_tools.back().cmd = CMD_EDIT_UNDO;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "重做"; m_tools.back().cmd = CMD_EDIT_REDO;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "|";    m_tools.back().cmd = CMD_NONE;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "剪切"; m_tools.back().cmd = CMD_EDIT_CUT;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "复制"; m_tools.back().cmd = CMD_EDIT_COPY;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "粘贴"; m_tools.back().cmd = CMD_EDIT_PASTE;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "|";    m_tools.back().cmd = CMD_NONE;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "查找"; m_tools.back().cmd = CMD_FIND;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "替换"; m_tools.back().cmd = CMD_REPLACE;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "|";    m_tools.back().cmd = CMD_NONE;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "编译"; m_tools.back().cmd = CMD_COMPILE;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "运行"; m_tools.back().cmd = CMD_RUN;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "停止"; m_tools.back().cmd = CMD_STOP;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "|";    m_tools.back().cmd = CMD_NONE;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "主题"; m_tools.back().cmd = CMD_THEME;
    m_tools.push_back(ToolBtn()); m_tools.back().label = "关于"; m_tools.back().cmd = CMD_ABOUT;

    loadWelcomeContent();
}

MainWindow::~MainWindow()
{
    delete buffer;
    // fileMgr / compiler / runtime 目前是 nullptr，等 E/D 交付后由 A 决定何时 new/delete
}

// ============================================================================
//  菜单结构
// ============================================================================
void MainWindow::buildMenus()
{
    m_menus.clear();

    MenuDef m;
    m.title = "文件"; m.items.clear();
    m.items.push_back(MenuItem()); m.items.back().label = "新建";         m.items.back().cmd = CMD_FILE_NEW;   m.items.back().hot = "Ctrl+N";
    m.items.push_back(MenuItem()); m.items.back().label = "打开...";      m.items.back().cmd = CMD_FILE_OPEN;  m.items.back().hot = "Ctrl+O";
    m.items.push_back(MenuItem()); m.items.back().label = "保存";         m.items.back().cmd = CMD_FILE_SAVE;  m.items.back().hot = "Ctrl+S";
    m.items.push_back(MenuItem()); m.items.back().label = "另存为...";    m.items.back().cmd = CMD_FILE_SAVEAS;m.items.back().hot = "Ctrl+Shift+S";
    m.items.push_back(MenuItem()); m.items.back().label = "退出";         m.items.back().cmd = CMD_EXIT;       m.items.back().hot = "Alt+F4";
    m_menus.push_back(m);

    m.title = "编辑"; m.items.clear();
    m.items.push_back(MenuItem()); m.items.back().label = "撤销";   m.items.back().cmd = CMD_EDIT_UNDO;   m.items.back().hot = "Ctrl+Z";
    m.items.push_back(MenuItem()); m.items.back().label = "重做";   m.items.back().cmd = CMD_EDIT_REDO;   m.items.back().hot = "Ctrl+Y";
    m.items.push_back(MenuItem()); m.items.back().label = "剪切";   m.items.back().cmd = CMD_EDIT_CUT;    m.items.back().hot = "Ctrl+X";
    m.items.push_back(MenuItem()); m.items.back().label = "复制";   m.items.back().cmd = CMD_EDIT_COPY;   m.items.back().hot = "Ctrl+C";
    m.items.push_back(MenuItem()); m.items.back().label = "粘贴";   m.items.back().cmd = CMD_EDIT_PASTE;  m.items.back().hot = "Ctrl+V";
    m.items.push_back(MenuItem()); m.items.back().label = "全选";   m.items.back().cmd = CMD_EDIT_SELALL; m.items.back().hot = "Ctrl+A";
    m_menus.push_back(m);

    m.title = "查找"; m.items.clear();
    m.items.push_back(MenuItem()); m.items.back().label = "查找";       m.items.back().cmd = CMD_FIND;         m.items.back().hot = "Ctrl+F";
    m.items.push_back(MenuItem()); m.items.back().label = "替换";       m.items.back().cmd = CMD_REPLACE;      m.items.back().hot = "Ctrl+H";
    m.items.push_back(MenuItem()); m.items.back().label = "查找下一个"; m.items.back().cmd = CMD_FIND_NEXT;    m.items.back().hot = "F3";
    m.items.push_back(MenuItem()); m.items.back().label = "查找上一个"; m.items.back().cmd = CMD_FIND_PREV;    m.items.back().hot = "Shift+F3";
    m.items.push_back(MenuItem()); m.items.back().label = "替换一处";   m.items.back().cmd = CMD_REPLACE_ONE;  m.items.back().hot = "";
    m.items.push_back(MenuItem()); m.items.back().label = "全部替换";   m.items.back().cmd = CMD_REPLACE_ALL;  m.items.back().hot = "Ctrl+Shift+H";
    m_menus.push_back(m);

    m.title = "编译"; m.items.clear();
    m.items.push_back(MenuItem()); m.items.back().label = "编译";        m.items.back().cmd = CMD_COMPILE;      m.items.back().hot = "F9";
    m.items.push_back(MenuItem()); m.items.back().label = "运行";        m.items.back().cmd = CMD_RUN;          m.items.back().hot = "F5";
    m.items.push_back(MenuItem()); m.items.back().label = "编译并运行";  m.items.back().cmd = CMD_COMPILE_RUN;  m.items.back().hot = "Ctrl+F5";
    m.items.push_back(MenuItem()); m.items.back().label = "停止";        m.items.back().cmd = CMD_STOP;         m.items.back().hot = "";
    m_menus.push_back(m);

    m.title = "视图"; m.items.clear();
    m.items.push_back(MenuItem()); m.items.back().label = "切换亮/暗主题"; m.items.back().cmd = CMD_THEME; m.items.back().hot = "";
    m_menus.push_back(m);

    m.title = "帮助"; m.items.clear();
    m.items.push_back(MenuItem()); m.items.back().label = "关于 Mini-C Studio"; m.items.back().cmd = CMD_ABOUT; m.items.back().hot = "";
    m_menus.push_back(m);
}

// 启动时给一段示例代码，方便直接看到行号 / 高亮 / 光标效果
void MainWindow::loadWelcomeContent()
{
    std::string demo =
        "#include <stdio.h>\r\n"
        "\r\n"
        "// Mini-C Studio —— 图形交互与状态反馈模块（角色 C）\r\n"
        "int add(int a, int b)\r\n"
        "{\r\n"
        "    int sum = a + b;   /* 试试在这里打字 */\r\n"
        "    return sum;\r\n"
        "}\r\n"
        "\r\n"
        "int main(void)\r\n"
        "{\r\n"
        "    printf(\"1 + 2 = %d\\n\", add(1, 2));\r\n"
        "    return 0;\r\n"
        "}\r\n";
    buffer->loadFromString(demo);
    buffer->setDirty(false);
    outputPanelText = "欢迎使用 Mini-C Studio！F9 编译，F5 运行。";
}

// ============================================================================
//  初始化窗口（A 原有，C 追加了自适应尺寸与双缓冲）
// ============================================================================
void MainWindow::initWindow()
{
    // --- C 追加：按屏幕大小选一个合适的窗口尺寸（A 原来固定 800x600）---
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    windowWidth  = MINIC_DEF_W;
    windowHeight = MINIC_DEF_H;
    if (windowWidth  > sw - 80) windowWidth  = sw - 80;
    if (windowHeight > sh - 80) windowHeight = sh - 80;
    if (windowWidth  < 640) windowWidth  = 640;
    if (windowHeight < 480) windowHeight = 480;

    // --- A 原有 ---
    initgraph(windowWidth, windowHeight, EW_SHOWCONSOLE);
    SetConsoleTitle(_T("Mini-C-Studio - 调试控制台"));   // A 原有（加了 _T，兼容 Unicode 工程）

    // --- C 追加：双缓冲，消除闪烁 ---
    BeginBatchDraw();
    setbkmode(TRANSPARENT);

    // 量一下等宽字体单个字符的宽度（后面所有列 → 像素换算都用它）
    settextstyle(UI_FONT_H, 0, _T("Consolas"));
    m_charW = textwidth(_T("M"));
    if (m_charW <= 0) m_charW = 8;

    // 启动时检测编译器，缺 gcc 就先在控制台提示一句（不弹窗，不打断启动）
    if (!CoreApi::inst().compilerAvailable())
        appendConsole("[提示] 未检测到 gcc 编译器，暂时无法编译。请安装 MinGW 并加入 PATH。");
}

// ============================================================================
//  主消息循环（A 原有框架，C 追加了鼠标移动/抬起/滚轮与字符输入）
// ============================================================================
void MainWindow::mainLoop()
{
    ExMessage msg;

    while (true)
    {
        while (peekmessage(&msg, EX_MOUSE | EX_KEY | EX_CHAR))
        {
            switch (msg.message)
            {
            case WM_LBUTTONDOWN:
                handleMouseClick(msg.x, msg.y);
                break;
            case WM_LBUTTONUP:                       // C 追加
                onMouseUp(msg.x, msg.y);
                break;
            case WM_MOUSEMOVE:                       // C 追加
                onMouseMove(msg.x, msg.y);
                break;
            case WM_MOUSEWHEEL:                      // C 追加
                onWheel(msg.wheel);
                break;
            case WM_KEYDOWN:                         // A 原有：只留这一个入口
                handleKeyPress(msg.vkcode);
                break;
            case WM_CHAR:                            // C 追加：可打印字符 / 中文
                onCharInput((unsigned int)msg.ch);
                break;
            default:
                break;
            }
        }

        drawAll();

        // 运行中的程序：每帧轮询一次输出
        if (m_progRunning)
        {
            std::string chunk; int code = 0; bool fin = false;
            if (CoreApi::inst().runPoll(chunk, code, fin))
            {
                if (!chunk.empty()) appendConsole(chunk);
                if (fin)
                {
                    m_progRunning = false;
                    if (code == 0) setStatus("程序运行结束，退出码 0", th->ok);
                    else           setStatus("程序异常结束，退出码 " + itos(code), th->err);
                }
            }
        }

        m_tick++;
        FlushBatchDraw();
        Sleep(16);
    }
}

// ============================================================================
//  绘制全部界面（A 原有，函数体重写）
// ============================================================================
void MainWindow::drawAll()
{
    setbkcolor(th->bg);
    cleardevice();
    setbkmode(TRANSPARENT);

    drawEditor();      // 编辑区（含行号、高亮、光标、滚动条）
    drawFindBar();     // 查找/替换条
    drawBottom();      // 底部诊断 / 控制台面板
    drawStatusBar();   // 状态栏
    drawToolbar();     // 工具栏
    drawMenuBar();     // 菜单栏（最上层）
    drawDropdown();    // 展开的菜单
}

// ============================================================================
//  布局：所有矩形都在这里算，绘制与鼠标命中测试共用同一份
// ============================================================================
Rect MainWindow::rMenu()   const { return Rect(0, 0, windowWidth, UI_MENU_H); }
Rect MainWindow::rTool()   const { return Rect(0, UI_MENU_H, windowWidth, UI_MENU_H + UI_TOOL_H); }
Rect MainWindow::rStatus() const { return Rect(0, windowHeight - UI_STATUS_H, windowWidth, windowHeight); }
Rect MainWindow::rBottom() const
{
    return Rect(0, windowHeight - UI_STATUS_H - UI_BOTTOM_H,
                windowWidth, windowHeight - UI_STATUS_H);
}
Rect MainWindow::rFindBar() const
{
    int y = rTool().y2;
    return Rect(0, y, windowWidth, y + UI_FINDBAR_H);
}
Rect MainWindow::rEdit() const
{
    int top = m_findVisible ? rFindBar().y2 : rTool().y2;
    return Rect(0, top, windowWidth, rBottom().y1);
}
Rect MainWindow::rGutter()  const { Rect e = rEdit(); return Rect(e.x1, e.y1, e.x1 + UI_GUTTER_W, e.y2); }
Rect MainWindow::rVScroll() const
{
    Rect e = rEdit();
    return Rect(e.x2 - UI_SCROLL_W, e.y1, e.x2, e.y2 - UI_SCROLL_W);
}
Rect MainWindow::rHScroll() const
{
    Rect e = rEdit();
    return Rect(e.x1, e.y2 - UI_SCROLL_W, e.x2 - UI_SCROLL_W, e.y2);
}
Rect MainWindow::rText() const
{
    return Rect(rGutter().x2, rEdit().y1, rVScroll().x1, rHScroll().y1);
}
Rect MainWindow::rBottomTabs() const
{
    Rect b = rBottom();
    return Rect(b.x1, b.y1, b.x2, b.y1 + UI_TAB_H);
}
Rect MainWindow::rBottomScroll() const
{
    Rect b = rBottom();
    return Rect(b.x2 - UI_SCROLL_W, rBottomTabs().y2, b.x2, b.y2);
}
Rect MainWindow::rBottomBody() const
{
    return Rect(rBottom().x1, rBottomTabs().y2, rBottomScroll().x1, rBottom().y2);
}
Rect MainWindow::rConsoleInput() const
{
    Rect b = rBottomBody();
    return Rect(b.x1, b.y2 - UI_CONSOLE_INPUT_H, b.x2, b.y2);
}
Rect MainWindow::rConsoleBody() const
{
    Rect b = rBottomBody();
    return Rect(b.x1, b.y1, b.x2, b.y2 - UI_CONSOLE_INPUT_H);
}
Rect MainWindow::dropRect(int mi) const
{
    int x = mi * UI_MENU_TITLE_W;
    int n = (int)m_menus[mi].items.size();
    return Rect(x, rMenu().y2, x + UI_DROP_W, rMenu().y2 + n * UI_DROP_ITEM_H + 6);
}
Rect MainWindow::dropItemRect(int mi, int ii) const
{
    Rect d = dropRect(mi);
    int y = d.y1 + 3 + ii * UI_DROP_ITEM_H;
    return Rect(d.x1 + 2, y, d.x2 - 2, y + UI_DROP_ITEM_H);
}
Rect MainWindow::toolBtnRect(int i) const
{
    Rect t = rTool();
    int x = t.x1 + 6, y = t.y1 + 5, h = t.h() - 10;
    for (int k = 0; k < i; k++)
    {
        if (m_tools[k].cmd == CMD_NONE) x += 10;
        else                            x += strWidth(m_tools[k].label) + 20;
    }
    int w = (m_tools[i].cmd == CMD_NONE) ? 2 : strWidth(m_tools[i].label) + 18;
    return Rect(x, y, x + w, y + h);
}

MainWindow::FindLayout MainWindow::findLayout() const
{
    FindLayout L;
    Rect f = rFindBar();
    int y = f.y1 + 5, h = f.h() - 10;
    int x = f.x1 + 8;
    L.bar     = f;
    L.fInput  = Rect(x, y, x + 200, y + h);          x += 208;
    L.bPrev   = Rect(x, y, x + 28,  y + h);          x += 32;
    L.bNext   = Rect(x, y, x + 28,  y + h);          x += 36;
    L.rInput  = Rect(x, y, x + 200, y + h);          x += 208;
    L.bRep    = Rect(x, y, x + 56,  y + h);          x += 60;
    L.bRepAll = Rect(x, y, x + 76,  y + h);          x += 84;
    L.chk     = Rect(x, y, x + 76,  y + h);          x += 84;
    L.bClose  = Rect(f.x2 - 30, y, f.x2 - 8, y + h);
    return L;
}

std::string MainWindow::tabLabel(int i) const
{
    if (i == 0)
    {
        int e = 0, w = 0;
        for (size_t k = 0; k < m_diagnostics.size(); k++)
        {
            if (m_diagnostics[k].level == DIAG_ERROR) e++;
            else if (m_diagnostics[k].level == DIAG_WARNING) w++;
        }
        char buf[64];
        sprintf_s(buf, "诊断  %d 错误 / %d 警告", e, w);
        return std::string(buf);
    }
    return "控制台";
}

int MainWindow::visibleLines() const
{
    int v = rText().h() / UI_LINE_H;
    return v > 0 ? v : 1;
}

// ============================================================================
//  菜单栏
// ============================================================================
void MainWindow::drawMenuBar()
{
    Rect m = rMenu();
    fillRect(m, th->menuBg);
    setlinecolor(th->border);
    line(m.x1, m.y2 - 1, m.x2, m.y2 - 1);

    settextstyle(14, 0, _T("Microsoft YaHei"));
    for (int i = 0; i < (int)m_menus.size(); i++)
    {
        Rect r(0 + i * UI_MENU_TITLE_W, m.y1,
               0 + (i + 1) * UI_MENU_TITLE_W, m.y2);
        bool on = (m_openMenu == i) || (m_openMenu < 0 && m_hoverMenu == i);
        if (on) fillRect(r, th->menuHover);
        settextcolor(on ? th->menuTextHover : th->menuText);
        drawStrCenter(r, m_menus[i].title);
    }
}

void MainWindow::drawDropdown()
{
    if (m_openMenu < 0) return;
    const MenuDef& md = m_menus[m_openMenu];
    Rect d = dropRect(m_openMenu);

    setfillcolor(th->menuBg);
    setlinecolor(th->border);
    solidrectangle(d.x1, d.y1, d.x2, d.y2);
    rectangle(d.x1, d.y1, d.x2, d.y2);

    settextstyle(14, 0, _T("Microsoft YaHei"));
    for (int i = 0; i < (int)md.items.size(); i++)
    {
        Rect r = dropItemRect(m_openMenu, i);
        bool on = (m_hoverItem == i);
        if (on) fillRect(r, th->menuHover);

        settextcolor(on ? th->menuTextHover : th->menuText);
        outtextxy(r.x1 + 8, r.y1 + 5, ts(md.items[i].label).c_str());

        if (!md.items[i].hot.empty())
        {
            settextcolor(th->dim);
            int hw = strWidth(md.items[i].hot);
            outtextxy(r.x2 - hw - 8, r.y1 + 5, ts(md.items[i].hot).c_str());
        }
    }
}

// ============================================================================
//  工具栏（A 的 drawButtons，改名后仍由 drawAll 调用）
// ============================================================================
void MainWindow::drawButtons()
{
    drawToolbar();
}

void MainWindow::drawToolbar()
{
    Rect t = rTool();
    fillRect(t, th->menuBg);
    setlinecolor(th->border);
    line(t.x1, t.y2 - 1, t.x2, t.y2 - 1);

    settextstyle(14, 0, _T("Microsoft YaHei"));
    for (int i = 0; i < (int)m_tools.size(); i++)
    {
        Rect r = toolBtnRect(i);
        if (m_tools[i].cmd == CMD_NONE)
        {
            setlinecolor(th->border);
            line(r.x1, r.y1 + 2, r.x1, r.y2 - 2);
            continue;
        }
        bool on = r.hit(m_mouseX, m_mouseY);
        fillRoundRect(r, 4, on ? th->btnHover : th->btn);
        settextcolor(th->btnText);
        drawStrCenter(r, m_tools[i].label);
    }
}

// ============================================================================
//  编辑区
// ============================================================================
namespace
{
    // 简易裁剪：进入作用域裁剪，离开自动恢复
    struct ClipGuard
    {
        HRGN m_rgn;
        ClipGuard(const Rect& r)
        {
            m_rgn = CreateRectRgn(r.x1, r.y1, r.x2, r.y2);
            setcliprgn(m_rgn);
        }
        ~ClipGuard()
        {
            setcliprgn(NULL);
            DeleteObject(m_rgn);
        }
    };

    const char* KEYWORDS[] = {
        "if","else","for","while","do","switch","case","default","break","continue",
        "return","goto","sizeof","static","const","extern","volatile","register",
        "struct","union","enum","typedef","auto","restrict","inline","_Bool", 0
    };
    const char* TYPES[] = {
        "int","char","float","double","long","short","void","signed","unsigned", 0
    };

    bool inList(const char* list[], const std::string& s)
    {
        for (int i = 0; list[i]; i++) if (s == list[i]) return true;
        return false;
    }
}

void MainWindow::drawEditor()
{
    Rect e = rEdit();
    fillRect(e, th->panel);

    if (!buffer) return;

    int total = buffer->getLineCount();
    int vis   = visibleLines();

    // 滚动位置夹取
    if (m_topLine > total - 1) m_topLine = total - 1;
    if (m_topLine < 0) m_topLine = 0;

    int curRow = buffer->getCursorY();

    // ---- 行号栏 ----
    Rect g = rGutter();
    {
        ClipGuard cg(g);
        fillRect(g, th->gutterBg);
        setlinecolor(th->border);
        line(g.x2 - 1, g.y1, g.x2 - 1, g.y2);

        settextstyle(13, 0, _T("Consolas"));
        for (int i = m_topLine; i < total && i < m_topLine + vis; i++)
        {
            int y  = e.y1 + (i - m_topLine) * UI_LINE_H;
            char buf[16];
            sprintf_s(buf, "%d", i + 1);
            int w = strWidth(buf);
            settextcolor(i == curRow ? th->text : th->gutterText);
            drawStr(g.x2 - 8 - w, y + 2, buf);
        }
    }

    // ---- 正文 ----
    Rect t = rText();
    {
        ClipGuard cg(t);

        settextstyle(UI_FONT_H, 0, _T("Consolas"));
        setbkmode(TRANSPARENT);

        // 块注释状态：只在视图变化时重算一次
        if (m_blockFrameCache != m_tick / 4 || m_blockTopCache != m_topLine)
        {
            bool st = false;
            for (int i = 0; i < m_topLine; i++)
            {
                std::string s = buffer->getLine(i);
                for (size_t k = 0; k + 1 < s.size(); k++)
                {
                    if (!st && s[k] == '/' && s[k + 1] == '*') { st = true; k++; }
                    else if (st && s[k] == '*' && s[k + 1] == '/') { st = false; k++; }
                }
            }
            m_blockValCache  = st;
            m_blockTopCache  = m_topLine;
            m_blockFrameCache = m_tick / 4;
        }
        bool inBlock = m_blockValCache;

        for (int i = m_topLine; i < total && i < m_topLine + vis; i++)
        {
            int y = e.y1 + (i - m_topLine) * UI_LINE_H;

            // 当前行高亮
            if (i == curRow)
            {
                setfillcolor(th->curLine);
                solidrectangle(t.x1, y, t.x2, y + UI_LINE_H);
            }
            drawCodeLine(t.x1 + 6, y + 2, i, t, inBlock);
        }

        // ---- 光标 ----
        if (m_focus == FOCUS_EDITOR && (m_tick % 30) < 18)
        {
            std::string curLineText = buffer->getLine(curRow);
            int dcol = dispColOfIndex(curLineText, buffer->getCursorX()) - m_leftCol;
            int cx = t.x1 + 6 + dcol * m_charW;
            int cy = e.y1 + (curRow - m_topLine) * UI_LINE_H;
            setlinecolor(th->text);
            line(cx, cy + 2, cx, cy + UI_LINE_H - 2);
        }
    }

    // ---- 滚动条 ----
    drawScrollbar(rVScroll(), m_topLine, total, vis, true);

    int maxDisp = 0;
    for (int i = 0; i < total; i++)
        maxDisp = std::max(maxDisp, dispWidth(buffer->getLine(i)));
    drawScrollbar(rHScroll(), m_leftCol, maxDisp + 1, rText().w() / (m_charW > 0 ? m_charW : 8), false);
}

// 画一行代码（含选区高亮与语法着色）
void MainWindow::drawCodeLine(int x, int y, int row, const Rect& clip, bool& inBlock)
{
    std::string s = buffer->getLine(row);

    // ---- 选区高亮 ----
    if (m_selActive)
    {
        int r1 = m_selR1, c1 = m_selC1, r2 = m_selR2, c2 = m_selC2;
        if (row >= r1 && row <= r2)
        {
            int a = (row == r1) ? c1 : 0;
            int b = (row == r2) ? c2 : (int)s.size();
            int da = dispColOfIndex(s, a) - m_leftCol;
            int db = dispColOfIndex(s, b) - m_leftCol;
            if (db > da)
            {
                setfillcolor(th->sel);
                solidrectangle(x + da * m_charW, y,
                               x + db * m_charW, y + UI_LINE_H - 2);
            }
        }
    }

    if (s.empty()) return;

    // ---- 简易分词 + 着色 ----
    size_t i = 0;
    while (i < s.size())
    {
        size_t start = i;
        COLORREF col = th->ident;

        unsigned char c0 = (unsigned char)s[i];

        if (inBlock)
        {
            while (i + 1 < s.size())
            {
                if (s[i] == '*' && s[i + 1] == '/') { i += 2; inBlock = false; break; }
                i++;
            }
            if (i >= s.size() && inBlock) i = s.size();
            col = th->comment;
        }
        else if (c0 == '/' && i + 1 < s.size() && s[i + 1] == '/')
        {
            i = s.size();
            col = th->comment;
        }
        else if (c0 == '/' && i + 1 < s.size() && s[i + 1] == '*')
        {
            inBlock = true;
            i += 2;
            while (i + 1 < s.size())
            {
                if (s[i] == '*' && s[i + 1] == '/') { i += 2; inBlock = false; break; }
                i++;
            }
            if (inBlock) i = s.size();
            col = th->comment;
        }
        else if (c0 == '"' || c0 == '\'')
        {
            char quote = (char)c0;
            i++;
            while (i < s.size())
            {
                if (s[i] == '\\' && i + 1 < s.size()) { i += 2; continue; }
                if (s[i] == quote) { i++; break; }
                i++;
            }
            col = th->str;
        }
        else if (c0 == '#')
        {
            while (i < s.size() && isalpha((unsigned char)s[i])) i++;
            col = th->prep;
        }
        else if (isdigit(c0))
        {
            while (i < s.size() && (isalnum((unsigned char)s[i]) || s[i] == '.' || s[i] == 'x' || s[i] == 'X')) i++;
            col = th->num;
        }
        else if (isalpha(c0) || c0 == '_')
        {
            while (i < s.size() && (isalnum((unsigned char)s[i]) || s[i] == '_')) i++;
            std::string w = s.substr(start, i - start);
            if      (inList(KEYWORDS, w)) col = th->kw;
            else if (inList(TYPES, w))    col = th->kwType;
            else                          col = th->ident;
        }
        else if (c0 >= 0x81 && c0 <= 0xFE)   // 中文：两个字节一起输出
        {
            i += 2;
            col = th->ident;
        }
        else
        {
            i++;
            col = th->punct;
        }

        std::string seg = s.substr(start, i - start);
        if (seg.empty()) continue;

        int dStart = dispColOfIndex(s, (int)start) - m_leftCol;
        int dEnd   = dispColOfIndex(s, (int)i)     - m_leftCol;
        if (dEnd <= 0) continue;                       // 完全在左边界外
        if (dStart * m_charW > clip.w()) break;        // 完全在右边界外

        settextcolor(col);
        drawStr(x + dStart * m_charW, y, seg);
    }
}

void MainWindow::drawScrollbar(const Rect& r, int pos, int total, int page, bool vertical)
{
    if (total <= page || total <= 1)
    {
        fillRect(r, th->panel);
        return;
    }
    fillRect(r, th->gutterBg);

    int track = vertical ? r.h() : r.w();
    int thumb = track * page / total;
    if (thumb < 20) thumb = 20;
    int maxPos = total - page;
    if (maxPos < 1) maxPos = 1;
    int t = (track - thumb) * pos / maxPos;

    Rect tb = vertical ? Rect(r.x1 + 2, r.y1 + t, r.x2 - 2, r.y1 + t + thumb)
                       : Rect(r.x1 + t, r.y1 + 2, r.x1 + t + thumb, r.y2 - 2);
    fillRoundRect(tb, 3, th->btnBorder);
}

// ============================================================================
//  查找 / 替换条
// ============================================================================
void MainWindow::drawFindBar()
{
    if (!m_findVisible) return;
    FindLayout L = findLayout();

    fillRect(L.bar, th->menuBg);
    setlinecolor(th->border);
    line(L.bar.x1, L.bar.y2 - 1, L.bar.x2, L.bar.y2 - 1);

    settextstyle(14, 0, _T("Microsoft YaHei"));

    auto drawInput = [&](const Rect& r, const std::string& text, bool focused)
    {
        fillRectB(r, th->panel, focused ? th->accent : th->border);
        std::string show = text.empty() ? "" : text;
        settextcolor(text.empty() ? th->dim : th->text);
        outtextxy(r.x1 + 5, r.y1 + 4, ts(show).c_str());
        if (focused && (m_tick % 30) < 18)
        {
            int w = strWidth(show);
            setlinecolor(th->text);
            line(r.x1 + 6 + w, r.y1 + 3, r.x1 + 6 + w, r.y2 - 3);
        }
    };

    drawInput(L.fInput, m_findText.empty() ? "查找" : m_findText, m_focus == FOCUS_FIND);
    drawInput(L.rInput, m_replaceText.empty() ? "替换为" : m_replaceText, m_focus == FOCUS_REPLACE);

    auto drawBtn = [&](const Rect& r, const std::string& label)
    {
        bool on = r.hit(m_mouseX, m_mouseY);
        fillRoundRect(r, 3, on ? th->btnHover : th->btn);
        settextcolor(th->btnText);
        drawStrCenter(r, label);
    };

    drawBtn(L.bPrev,   "◀");
    drawBtn(L.bNext,   "▶");
    drawBtn(L.bRep,    "替换");
    drawBtn(L.bRepAll, "全部替换");

    // 大小写复选框
    fillRectB(L.chk, th->panel, th->border);
    Rect box(L.chk.x1 + 4, L.chk.y1 + (L.chk.h() - 12) / 2, L.chk.x1 + 16, L.chk.y1 + (L.chk.h() - 12) / 2 + 12);
    fillRectB(box, m_caseSensitive ? th->accent : th->panel, th->border);
    settextcolor(th->text);
    outtextxy(L.chk.x1 + 20, L.chk.y1 + 4, ts("区分大小写").c_str());

    drawBtn(L.bClose, "✕");

    if (m_hasMatch)
    {
        settextcolor(th->dim);
        char buf[64];
        sprintf_s(buf, "第 %d 行", m_matchR + 1);
        outtextxy(L.bNext.x2 + 10, L.bar.y1 + 9, ts(buf).c_str());
    }
}

// ============================================================================
//  底部面板（诊断 / 控制台）
// ============================================================================
void MainWindow::drawBottom()
{
    Rect b = rBottom();
    fillRect(b, th->panel);
    setlinecolor(th->border);
    line(b.x1, b.y1, b.x2, b.y1);

    Rect tabs = rBottomTabs();
    fillRect(tabs, th->gutterBg);
    settextstyle(13, 0, _T("Microsoft YaHei"));

    int x = tabs.x1 + 8;
    for (int i = 0; i < 2; i++)
    {
        std::string label = tabLabel(i);
        int w = strWidth(label) + 26;
        Rect r(x, tabs.y1 + 2, x + w, tabs.y2 - 1);
        bool on = (m_bottomTab == i);
        fillRect(r, on ? th->panel : th->gutterBg);
        setlinecolor(on ? th->accent : th->border);
        line(r.x1, r.y2 - 1, r.x2, r.y2 - 1);
        settextcolor(on ? th->text : th->dim);
        outtextxy(r.x1 + 13, tabs.y1 + 6, ts(label).c_str());
        x += w;
    }

    if (m_bottomTab == 0) drawDiagnostics();
    else                  drawConsole();
}

void MainWindow::drawDiagnostics()
{
    Rect body = rBottomBody();
    fillRect(body, th->panel);

    int lh = 20;
    int page = body.h() / lh;
    int total = (int)m_diagnostics.size();
    if (m_diagTop > total - page) m_diagTop = total - page;
    if (m_diagTop < 0) m_diagTop = 0;

    {
        ClipGuard cg(body);
        settextstyle(14, 0, _T("Consolas"));
        for (int i = m_diagTop; i < total && i < m_diagTop + page; i++)
        {
            const Diagnostic& d = m_diagnostics[i];
            int y = body.y1 + (i - m_diagTop) * lh;

            bool hover = (m_mouseX > body.x1 && m_mouseX < body.x2 &&
                          m_mouseY >= y && m_mouseY < y + lh);
            if (hover)
            {
                setfillcolor(th->curLine);
                solidrectangle(body.x1, y, body.x2, y + lh);
            }

            COLORREF c = (d.level == DIAG_ERROR)   ? th->err
                       : (d.level == DIAG_WARNING) ? th->warn : th->dim;
            settextcolor(c);

            char buf[32];
            sprintf_s(buf, "第 %d 行", d.line + 1);
            drawStr(body.x1 + 8, y + 3, buf);
            drawStr(body.x1 + 78, y + 3, d.message);
        }
        if (total == 0)
        {
            settextcolor(th->dim);
            drawStr(body.x1 + 8, body.y1 + 6, "（暂无诊断信息，按 F9 编译后会在这里显示）");
        }
    }

    drawScrollbar(rBottomScroll(), m_diagTop, total, page, true);
}

void MainWindow::drawConsole()
{
    Rect body = rBottomBody();
    fillRect(body, th->outBg);

    int lh = 18;
    int page = rConsoleBody().h() / lh;
    int total = (int)m_console.size();
    if (m_consoleTop > total - page) m_consoleTop = total - page;
    if (m_consoleTop < 0) m_consoleTop = 0;

    {
        ClipGuard cg(rConsoleBody());
        settextstyle(13, 0, _T("Consolas"));
        settextcolor(th->outText);
        for (int i = m_consoleTop; i < total && i < m_consoleTop + page; i++)
        {
            int y = rConsoleBody().y1 + (i - m_consoleTop) * lh;
            drawStr(body.x1 + 8, y + 2, m_console[i]);
        }
    }

    // 输入行
    Rect in = rConsoleInput();
    fillRect(in, th->outBg);
    setlinecolor(th->border);
    line(in.x1, in.y1, in.x2, in.y1);
    settextcolor(th->outPrompt);
    drawStr(in.x1 + 8, in.y1 + 4, ">");
    settextcolor(th->outText);
    drawStr(in.x1 + 24, in.y1 + 4, m_inputLine);
    if (m_focus == FOCUS_CONSOLE && (m_tick % 30) < 18)
    {
        int w = strWidth(m_inputLine);
        setlinecolor(th->outText);
        line(in.x1 + 25 + w, in.y1 + 3, in.x1 + 25 + w, in.y2 - 3);
    }

    drawScrollbar(rBottomScroll(), m_consoleTop, total, page, true);
}

// ============================================================================
//  状态栏（A 原有，函数体重写）
// ============================================================================
void MainWindow::drawStatusBar()
{
    Rect s = rStatus();
    fillRect(s, th->statusBg);
    setlinecolor(th->border);
    line(s.x1, s.y1, s.x2, s.y1);

    settextstyle(13, 0, _T("Microsoft YaHei"));
    settextcolor(th->statusText);

    std::string fname = buffer->getFilePath();
    if (fname.empty()) fname = "未命名.c";
    std::string head = fname + (buffer->isDirty() ? " *" : "");
    drawStr(s.x1 + 10, s.y1 + 5, head);

    char buf[64];
    sprintf_s(buf, "行 %d, 列 %d", buffer->getCursorY() + 1, buffer->getCursorX() + 1);
    int x = s.x1 + 10 + strWidth(head) + 24;
    drawStr(x, s.y1 + 5, buf);
    x += strWidth(buf) + 24;

    // 编译状态
    const char* cs = "";
    switch (m_compileState)
    {
    case CS_OK:         cs = "编译成功"; break;
    case CS_WARNING:    cs = "编译成功（有警告）"; break;
    case CS_ERROR:      cs = "编译失败"; break;
    case CS_NOCOMPILER: cs = "未找到编译器"; break;
    case CS_TIMEOUT:    cs = "编译超时"; break;
    case CS_INTERNAL:   cs = "内部错误"; break;
    default:            cs = "尚未编译"; break;
    }
    settextcolor(m_compileState == CS_NONE ? th->statusText : m_statusColor);
    drawStr(x, s.y1 + 5, cs);
    x += strWidth(cs) + 24;

    settextcolor(th->statusText);
    drawStr(x, s.y1 + 5, th->name);

    int rw = strWidth(MINIC_VERSION);
    drawStr(s.x2 - rw - 10, s.y1 + 5, MINIC_VERSION);

    // 临时提示信息（居中显示，约 2.4 秒后自动消失）
    if (!m_statusMsg.empty() && m_tick < m_statusUntil)
    {
        int mw = strWidth(m_statusMsg);
        settextcolor(m_statusColor);
        drawStr((s.x1 + s.x2) / 2 - mw / 2, s.y1 + 5, m_statusMsg);
    }
}

// ============================================================================
//  输出面板（A 原有名字，现在把 A 的 outputPanelText 同步进控制台）
// ============================================================================
void MainWindow::drawOutputPanel()
{
    // A 的 outputPanelText 由 doCompile / doRun 等写入，
    // 这里保证它至少在控制台里能看到一次。
    // 真正的绘制在 drawConsole() 里完成。
}

void MainWindow::appendConsole(const std::string& text)
{
    if (text.empty()) return;
    std::string cur;
    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] == '\n') { m_console.push_back(cur); cur.clear(); }
        else if (text[i] != '\r') cur += text[i];
    }
    if (!cur.empty()) m_console.push_back(cur);
    if ((int)m_console.size() > 2000)
        m_console.erase(m_console.begin(), m_console.begin() + 500);
    m_consoleTop = (int)m_console.size();
    m_bottomTab = 1;
}

void MainWindow::setStatus(const std::string& msg, COLORREF c)
{
    m_statusMsg = msg;
    m_statusColor = c;
    m_statusUntil = m_tick + 150;      // 约 2.4 秒后自动消失
}

// ============================================================================
//  鼠标（A 原有 handleMouseClick，函数体由 C 实现）
// ============================================================================
void MainWindow::handleMouseClick(int x, int y)
{
    m_mouseX = x; m_mouseY = y;

    // ---- 1. 展开中的下拉菜单 ----
    if (m_openMenu >= 0)
    {
        if (dropRect(m_openMenu).hit(x, y))
        {
            for (int i = 0; i < (int)m_menus[m_openMenu].items.size(); i++)
            {
                if (dropItemRect(m_openMenu, i).hit(x, y))
                {
                    int cmd = m_menus[m_openMenu].items[i].cmd;
                    m_openMenu = -1;
                    onCommand(cmd);
                    return;
                }
            }
            return;
        }
        m_openMenu = -1;      // 点到外面，收起菜单
    }

    // ---- 2. 菜单栏标题 ----
    if (rMenu().hit(x, y))
    {
        for (int i = 0; i < (int)m_menus.size(); i++)
        {
            if (x >= i * UI_MENU_TITLE_W && x < (i + 1) * UI_MENU_TITLE_W)
            {
                m_openMenu = (m_openMenu == i) ? -1 : i;
                return;
            }
        }
        return;
    }

    // ---- 3. 工具栏 ----
    if (rTool().hit(x, y))
    {
        for (int i = 0; i < (int)m_tools.size(); i++)
        {
            if (m_tools[i].cmd != CMD_NONE && toolBtnRect(i).hit(x, y))
            {
                onCommand(m_tools[i].cmd);
                return;
            }
        }
        return;
    }

    // ---- 4. 查找条 ----
    if (m_findVisible && rFindBar().hit(x, y))
    {
        FindLayout L = findLayout();
        if (L.bClose.hit(x, y)) { m_findVisible = false; m_focus = FOCUS_EDITOR; return; }
        if (L.fInput.hit(x, y)) { m_focus = FOCUS_FIND;    return; }
        if (L.rInput.hit(x, y)) { m_focus = FOCUS_REPLACE; return; }
        if (L.chk.hit(x, y))    { m_caseSensitive = !m_caseSensitive; return; }
        if (L.bPrev.hit(x, y))  { onCommand(CMD_FIND_PREV); return; }
        if (L.bNext.hit(x, y))  { onCommand(CMD_FIND_NEXT); return; }
        if (L.bRep.hit(x, y))   { onCommand(CMD_REPLACE_ONE); return; }
        if (L.bRepAll.hit(x, y)){ onCommand(CMD_REPLACE_ALL); return; }
        return;
    }

    // ---- 5. 底部面板 ----
    if (rBottom().hit(x, y))
    {
        if (rBottomScroll().hit(x, y))
        {
            m_dragV = true; m_dragInBottom = true; m_dragStart = y;
            return;
        }
        if (rBottomTabs().hit(x, y))
        {
            int tx = rBottomTabs().x1 + 8;
            for (int i = 0; i < 2; i++)
            {
                int w = strWidth(tabLabel(i)) + 26;
                if (x >= tx && x < tx + w) { m_bottomTab = i; return; }
                tx += w;
            }
            return;
        }
        // 诊断行 → 跳到源码对应行
        if (m_bottomTab == 0 && rBottomBody().hit(x, y))
        {
            int idx = m_diagTop + (y - rBottomBody().y1) / 20;
            if (idx >= 0 && idx < (int)m_diagnostics.size())
            {
                int line = m_diagnostics[idx].line;
                bufSetCursor(buffer, line, 0);
                m_topLine = line - visibleLines() / 2;
                if (m_topLine < 0) m_topLine = 0;
                clearSel();
                m_focus = FOCUS_EDITOR;
            }
            return;
        }
        // 控制台输入行
        if (m_bottomTab == 1 && rConsoleInput().hit(x, y))
        {
            m_focus = FOCUS_CONSOLE;
            return;
        }
        m_focus = (m_bottomTab == 1) ? FOCUS_CONSOLE : FOCUS_EDITOR;
        return;
    }

    // ---- 6. 编辑区滚动条 ----
    if (rVScroll().hit(x, y))
    {
        m_dragV = true; m_dragInBottom = false; m_dragStart = y;
        return;
    }
    if (rHScroll().hit(x, y))
    {
        m_dragH = true; m_dragStart = x;
        return;
    }

    // ---- 7. 编辑区：定位光标 / 开始拖拽选区 ----
    if (rEdit().hit(x, y))
    {
        m_focus = FOCUS_EDITOR;
        int row, col;
        screenToDoc(x, y, row, col);
        bufSetCursor(buffer, row, col);
        m_anchorR = row; m_anchorC = col;
        m_selActive = false;
        m_dragging = true;
        m_wantCol = -1;
        return;
    }
}

void MainWindow::onMouseUp(int, int)
{
    m_dragging = false;
    m_dragV = false;
    m_dragH = false;
}

void MainWindow::onMouseMove(int x, int y)
{
    m_mouseX = x; m_mouseY = y;

    // 菜单项悬停
    m_hoverMenu = -1; m_hoverItem = -1;
    if (m_openMenu >= 0)
    {
        for (int i = 0; i < (int)m_menus[m_openMenu].items.size(); i++)
            if (dropItemRect(m_openMenu, i).hit(x, y)) { m_hoverItem = i; break; }
    }
    else if (rMenu().hit(x, y))
    {
        m_hoverMenu = x / UI_MENU_TITLE_W;
        if (m_hoverMenu >= (int)m_menus.size()) m_hoverMenu = -1;
    }

    // 拖动垂直滚动条（编辑区 / 底部面板各有一个，用 m_dragInBottom 区分）
    if (m_dragV)
    {
        if (m_dragInBottom)
        {
            int track = rBottomScroll().h();
            int delta = y - m_dragStart;
            int& top = (m_bottomTab == 0) ? m_diagTop : m_consoleTop;
            int total = (m_bottomTab == 0) ? (int)m_diagnostics.size() : (int)m_console.size();
            int page  = rBottomBody().h() / ((m_bottomTab == 0) ? 20 : 18);
            int maxPos = total - page;
            if (maxPos > 0 && track > 0) top += delta * maxPos / track;
            if (top < 0) top = 0;
            if (top > maxPos) top = maxPos;
            m_dragStart = y;
        }
        else
        {
            int total = buffer->getLineCount();
            int page  = visibleLines();
            if (total > page)
            {
                int track = rVScroll().h();
                int delta = y - m_dragStart;
                int maxPos = total - page;
                m_topLine += delta * maxPos / (track > 0 ? track : 1);
                if (m_topLine < 0) m_topLine = 0;
                if (m_topLine > maxPos) m_topLine = maxPos;
                m_dragStart = y;
            }
        }
        return;
    }

    // 拖动水平滚动条
    if (m_dragH)
    {
        int delta = (x - m_dragStart) / (m_charW > 0 ? m_charW : 8);
        m_leftCol -= delta;
        if (m_leftCol < 0) m_leftCol = 0;
        m_dragStart = x;
        return;
    }

    // 在编辑区里拖拽 → 扩展选区
    if (m_dragging && rEdit().hit(x, y))
    {
        int row, col;
        screenToDoc(x, y, row, col);
        m_selR1 = m_anchorR; m_selC1 = m_anchorC;
        m_selR2 = row;       m_selC2 = col;
        normalizeSel();
        m_selActive = (m_selR1 != m_selR2 || m_selC1 != m_selC2);
        bufSetCursor(buffer, row, col);
        syncSelToBuffer();
    }
}

void MainWindow::onWheel(int delta)
{
    if (rEdit().hit(m_mouseX, m_mouseY) || m_focus == FOCUS_EDITOR)
    {
        int step = (delta > 0) ? -3 : 3;
        m_topLine += step;
        int maxTop = buffer->getLineCount() - 1;
        if (m_topLine < 0) m_topLine = 0;
        if (m_topLine > maxTop) m_topLine = maxTop;
    }
    else if (rBottom().hit(m_mouseX, m_mouseY))
    {
        int step = (delta > 0) ? -3 : 3;
        if (m_bottomTab == 0) m_diagTop += step;
        else                  m_consoleTop += step;
        if (m_diagTop < 0) m_diagTop = 0;
        if (m_consoleTop < 0) m_consoleTop = 0;
    }
}

// ============================================================================
//  键盘（A 原有 handleKeyPress，函数体由 C 实现）
//  A 原来的 13 行逻辑（可打印字符 / 退格 / 删除 / 回车 / 四个方向键）
//  已完整保留在 onKeyDown + onCharInput 中，并补齐了
//  Home/End、翻页、Tab、Ctrl 组合、查找替换等。
// ============================================================================
void MainWindow::handleKeyPress(int key)
{
    bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
    onKeyDown(key, ctrl, shift);
}

void MainWindow::onKeyDown(int vk, bool ctrl, bool shift)
{
    // ---------- Ctrl 组合 ----------
    if (ctrl)
    {
        switch (vk)
        {
        case 'N': onCommand(CMD_FILE_NEW);   return;
        case 'O': onCommand(CMD_FILE_OPEN);  return;
        case 'S': onCommand(shift ? CMD_FILE_SAVEAS : CMD_FILE_SAVE); return;
        case 'A': onCommand(CMD_EDIT_SELALL);return;
        case 'Z': onCommand(CMD_EDIT_UNDO);  return;
        case 'Y': onCommand(CMD_EDIT_REDO);  return;
        case 'X': onCommand(CMD_EDIT_CUT);   return;
        case 'C': onCommand(CMD_EDIT_COPY);  return;
        case 'V': onCommand(CMD_EDIT_PASTE); return;
        case 'F': onCommand(CMD_FIND);       return;
        case 'H': onCommand(shift ? CMD_REPLACE_ALL : CMD_REPLACE); return;
        case 'W': doExit();                  return;
        default: break;
        }
        if (vk == VK_F5) { onCommand(CMD_COMPILE_RUN); return; }
        return;
    }

    // ---------- 单键 ----------
    switch (vk)
    {
    case VK_LEFT:
        if (m_selActive && !shift) { clearSel(); break; }
        bufMoveLeft(buffer);
        m_wantCol = -1;
        break;
    case VK_RIGHT:
        if (m_selActive && !shift) { clearSel(); break; }
        bufMoveRight(buffer);
        m_wantCol = -1;
        break;
    case VK_UP:
        m_wantCol = bufMoveUp(buffer, m_wantCol);
        break;
    case VK_DOWN:
        m_wantCol = bufMoveDown(buffer, m_wantCol);
        break;
    case VK_HOME: bufHome(buffer); m_wantCol = -1; break;
    case VK_END:  bufEnd(buffer);  m_wantCol = -1; break;
    case VK_PRIOR:   // PageUp
    {
        int page = visibleLines();
        int r = buffer->getCursorY() - page;
        if (r < 0) r = 0;
        bufSetCursor(buffer, r, buffer->getCursorX());
        m_topLine -= page;
        if (m_topLine < 0) m_topLine = 0;
        break;
    }
    case VK_NEXT:    // PageDown
    {
        int page = visibleLines();
        int r = buffer->getCursorY() + page;
        if (r >= buffer->getLineCount()) r = buffer->getLineCount() - 1;
        bufSetCursor(buffer, r, buffer->getCursorX());
        m_topLine += page;
        break;
    }
    case VK_DELETE:
        if (m_selActive) deleteSel();
        else             buffer->deleteForward();
        break;
    case VK_BACK:                       // A 原有：退格
        if (m_selActive) deleteSel();
        else             buffer->deleteChar();
        break;
    case VK_TAB:
        deleteSel();
        bufInsertString(buffer, "    ");
        break;
    case VK_ESCAPE:
        if (m_findVisible) { m_findVisible = false; m_focus = FOCUS_EDITOR; }
        else if (m_selActive) clearSel();
        break;
    case VK_F1: onCommand(CMD_ABOUT);   break;
    case VK_F3: onCommand(shift ? CMD_FIND_PREV : CMD_FIND_NEXT); break;
    case VK_F5: onCommand(CMD_RUN);     break;
    case VK_F9: onCommand(CMD_COMPILE); break;
    default: break;
    }

    scrollToCursor();
}

// 可打印字符（含中文）入口；由 mainLoop 的 WM_CHAR 调用
void MainWindow::onCharInput(unsigned int code)
{
    if (code == 0) return;

    // 查找条处于焦点时，字符进输入框
    if (m_focus == FOCUS_FIND && code >= 32)
    {
        m_findText += (char)code;
        m_hasMatch = false;
        return;
    }
    if (m_focus == FOCUS_REPLACE && code >= 32)
    {
        m_replaceText += (char)code;
        return;
    }
    if (m_focus == FOCUS_CONSOLE && code >= 32)
    {
        m_inputLine += (char)code;
        return;
    }

    // 回车：Enter（在 WM_CHAR 里统一处理，避免和 VK_RETURN 重复）
    if (code == '\r' || code == '\n')
    {
        if (m_focus == FOCUS_FIND)    { onCommand(CMD_FIND_NEXT); return; }
        if (m_focus == FOCUS_REPLACE) { onCommand(CMD_REPLACE_ONE); return; }
        if (m_focus == FOCUS_CONSOLE)
        {
            appendConsole("> " + m_inputLine);
            if (m_progRunning) CoreApi::inst().runSendInput(m_inputLine);
            m_inputLine.clear();
            return;
        }
        deleteSel();
        buffer->enter();
        scrollToCursor();
        return;
    }

    if (code < 32) return;      // 其余控制字符交给 onKeyDown

    // 中文（GBK 双字节）：先收下首字节，等第二个字节到齐再一起插入
    unsigned char c = (unsigned char)code;
    if (!m_dbcsPending.empty())
    {
        std::string two = m_dbcsPending + (char)c;
        m_dbcsPending.clear();
        deleteSel();
        bufInsertString(buffer, two);
        scrollToCursor();
        return;
    }
    if (c >= 0x81 && c <= 0xFE)
    {
        m_dbcsPending = std::string(1, (char)c);
        return;
    }

    // 普通 ASCII 字符（A 原有：buffer->insertChar）
    deleteSel();
    bufInsertString(buffer, std::string(1, (char)c));
    scrollToCursor();
}

// ============================================================================
//  命令分发（菜单 / 工具栏 / 快捷键 共用同一套）
// ============================================================================
void MainWindow::onCommand(int cmd)
{
    switch (cmd)
    {
    // ---- 文件 ----
    case CMD_FILE_NEW:
        if (!confirmDiscard()) return;
        buffer->loadFromString("");
        buffer->setFilePath("");
        buffer->setDirty(false);
        m_topLine = 0; m_leftCol = 0;
        clearSel();
        setStatus("已新建空白文件", th->ok);
        break;
    case CMD_FILE_OPEN:
        doOpen();
        break;
    case CMD_FILE_SAVE:
        doSave();
        break;
    case CMD_FILE_SAVEAS:
        doSaveAs();
        break;
    case CMD_EXIT:
        doExit();
        break;

    // ---- 编辑 ----
    case CMD_EDIT_UNDO:
        if (buffer->canUndo()) { clearSel(); buffer->undo(); scrollToCursor(); }
        break;
    case CMD_EDIT_REDO:
        if (buffer->canRedo()) { clearSel(); buffer->redo(); scrollToCursor(); }
        break;
    case CMD_EDIT_CUT:
        if (hasSel()) { setClipboardText(bufSelectedText(buffer)); deleteSel(); }
        break;
    case CMD_EDIT_COPY:
        if (hasSel()) setClipboardText(bufSelectedText(buffer));
        break;
    case CMD_EDIT_PASTE:
    {
        std::string t = getClipboardText();
        if (!t.empty()) { insertAtCursor(t); scrollToCursor(); }
        break;
    }
    case CMD_EDIT_SELALL:
    {
        int last = buffer->getLineCount() - 1;
        m_selR1 = 0; m_selC1 = 0;
        m_selR2 = last; m_selC2 = buffer->getLineLength(last);
        m_selActive = true;
        syncSelToBuffer();
        break;
    }

    // ---- 查找替换 ----
    case CMD_FIND:
        m_findVisible = true;
        m_focus = FOCUS_FIND;
        if (hasSel()) m_findText = bufSelectedText(buffer);
        break;
    case CMD_REPLACE:
        m_findVisible = true;
        m_focus = FOCUS_REPLACE;
        break;
    case CMD_FIND_NEXT:
        searchNext(true);
        break;
    case CMD_FIND_PREV:
        searchNext(false);
        break;
    case CMD_REPLACE_ONE:
        replaceOne();
        break;
    case CMD_REPLACE_ALL:
        replaceAll();
        break;

    // ---- 编译运行 ----
    case CMD_COMPILE:
        doCompile();
        break;
    case CMD_RUN:
        doRun();
        break;
    case CMD_COMPILE_RUN:
        doCompile();
        if (m_compileState == CS_OK || m_compileState == CS_WARNING) doRun();
        break;
    case CMD_STOP:
        doStop();
        break;

    // ---- 视图 / 帮助 ----
    case CMD_THEME:
        m_dark = !m_dark;
        th = m_dark ? &themeDark() : &themeLight();
        setStatus("已切换到" + std::string(th->name) + "主题", th->accent);
        break;
    case CMD_ABOUT:
        uiAlert(windowWidth, windowHeight, *th, "关于 Mini-C Studio",
                std::string(MINIC_TITLE) + "  " + MINIC_VERSION + "\n"
                + MINIC_ORG + "\n\n"
                + "本模块：图形交互与状态反馈（角色 C）\n"
                + "文本编辑：B　　文件管理：E\n"
                + "编译与运行：D　　主控整合：A\n");
        break;
    default:
        break;
    }
}

// ============================================================================
//  文件操作（真正写盘的地方走 CoreApi → E 同学）
// ============================================================================
bool MainWindow::confirmDiscard()
{
    if (!buffer->isDirty()) return true;
    DlgRet r = uiMessageBox(windowWidth, windowHeight, *th,
                            "Mini-C Studio",
                            "当前文件有未保存的修改，要保存吗？",
                            DLG_YESNOCANCEL);
    if (r == RET_CANCEL || r == RET_NONE) return false;
    if (r == RET_NO) return true;
    return doSave();
}

bool MainWindow::doSaveTo(const std::string& path)
{
    if (path.empty()) return false;
    std::string content = buffer->saveToString();
    // B 的 saveToString() 输出纯 "\n"（见《B同学答复清单》Q3）。
    // 按 B 同学的分工说明，写盘方负责把 "\n" 转成 Windows 的 "\r\n"，
    // 这样生成的 .c 文件在记事本里也能正确换行；读回时 loadFromString 会剥掉 "\r"，内部仍用 "\n"。
    std::string crlf;
    crlf.reserve(content.size() + content.size() / 4);
    for (size_t i = 0; i < content.size(); ++i)
    {
        if (content[i] == '\n') crlf += "\r\n";
        else crlf += content[i];
    }
    if (!CoreApi::inst().fileWrite(path, crlf))
    {
        uiAlert(windowWidth, windowHeight, *th, "保存失败",
                "无法写入文件：\n" + path + "\n\n请检查路径是否合法、是否有写入权限。");
        setStatus("保存失败", th->err);
        return false;
    }
    buffer->setFilePath(path);
    buffer->setDirty(false);
    appendConsole("[已保存] " + path);
    setStatus("保存成功", th->ok);
    outputPanelText = "保存成功: " + path;
    return true;
}

bool MainWindow::doSave()
{
    std::string path = buffer->getFilePath();
    if (path.empty()) return doSaveAs();
    return doSaveTo(path);
}

bool MainWindow::doSaveAs()
{
    std::string path = buffer->getFilePath();
    if (path.empty()) path = "untitled.c";
    if (!uiPickPath("另存为", path, true)) return false;
    return doSaveTo(path);
}

void MainWindow::doOpen()
{
    if (!confirmDiscard()) return;

    std::string path = buffer->getFilePath();
    if (!uiPickPath("打开 C 源文件", path, false)) return;

    std::string text;
    if (!CoreApi::inst().fileRead(path, text))
    {
        uiAlert(windowWidth, windowHeight, *th, "打开失败",
                "无法读取文件：\n" + path + "\n\n请检查文件是否存在、是否有读取权限。");
        setStatus("打开失败", th->err);
        return;
    }
    buffer->loadFromString(text);
    buffer->setFilePath(path);
    buffer->setDirty(false);
    m_topLine = 0; m_leftCol = 0;
    clearSel();
    m_diagnostics.clear();
    m_compileState = CS_NONE;
    appendConsole("[已打开] " + path);
    setStatus("打开成功", th->ok);
    outputPanelText = "已打开: " + path;
}

void MainWindow::doExit()
{
    if (!confirmDiscard()) return;
    closegraph();
    exit(0);
}

// ============================================================================
//  编译 / 运行（走 CoreApi → D 同学）
// ============================================================================
void MainWindow::doCompile()
{
    if (m_progRunning) { setStatus("程序正在运行，请先停止", th->warn); return; }

    // 源码为空 → PPT 要求的异常提示
    if (buffer->saveToString().find_first_not_of(" \t\r\n") == std::string::npos)
    {
        uiAlert(windowWidth, windowHeight, *th, "无法编译",
                "当前源码是空的，请先写点代码再编译。");
        m_compileState = CS_INTERNAL;
        return;
    }

    // 先自动保存（编译的是磁盘上的文件）
    if (!doSave()) return;

    isCompiling = true;
    setStatus("正在编译...", th->accent);
    outputPanelText = "编译中...";

    CompileResult r = CoreApi::inst().compile(buffer->getFilePath());

    m_compileState = compileStateOf(r);
    m_diagnostics  = r.diags;
    m_diagTop      = 0;
    m_lastExe      = CoreApi::inst().exePathOf(buffer->getFilePath());
    isCompiling    = false;

    appendConsole(r.rawOutput);
    m_bottomTab = (m_compileState == CS_OK) ? 1 : 0;

    switch (m_compileState)
    {
    case CS_OK:
        setStatus("编译成功", th->ok);
        outputPanelText = "编译成功";
        break;
    case CS_WARNING:
        setStatus("编译成功，有警告", th->warn);
        outputPanelText = "编译成功（有警告）";
        break;
    case CS_ERROR:
        setStatus("编译失败", th->err);
        outputPanelText = "编译失败";
        break;
    case CS_NOCOMPILER:
        setStatus("未找到编译器", th->err);
        uiAlert(windowWidth, windowHeight, *th, "未找到编译器",
                "没有检测到 gcc。\n请安装 MinGW-w64 / TDM-GCC，\n"
                "并把 gcc.exe 所在目录加入系统 PATH 后重启本程序。");
        break;
    default:
        setStatus("编译异常", th->err);
        break;
    }
}

void MainWindow::doRun()
{
    if (m_progRunning) { setStatus("程序正在运行", th->warn); return; }

    std::string exe = m_lastExe;
    if (exe.empty()) exe = CoreApi::inst().exePathOf(buffer->getFilePath());

    if (!CoreApi::inst().fileExists(exe))
    {
        uiAlert(windowWidth, windowHeight, *th, "无法运行",
                "找不到可执行文件：\n" + exe + "\n\n请先按 F9 编译。");
        setStatus("请先编译", th->warn);
        return;
    }

    m_bottomTab = 1;
    appendConsole("----- 程序开始运行 -----");
    if (CoreApi::inst().runStart(exe))
    {
        m_progRunning = true;
        setStatus("运行中...", th->accent);
        outputPanelText = "运行中...";
    }
    else
    {
        setStatus("启动失败", th->err);
    }
}

void MainWindow::doStop()
{
    if (!m_progRunning) return;
    CoreApi::inst().runStop();
    m_progRunning = false;
    appendConsole("[已终止]");
    setStatus("已停止", th->warn);
}

// ============================================================================
//  A 原有的 5 个按钮回调：保持 A 的语义，内部调用 C 的实现
// ============================================================================
void MainWindow::onNewFile()  { onCommand(CMD_FILE_NEW); }
void MainWindow::onOpenFile() { onCommand(CMD_FILE_OPEN); }
void MainWindow::onSaveFile() { onCommand(CMD_FILE_SAVE); }
void MainWindow::onCompile()  { onCommand(CMD_COMPILE); }
void MainWindow::onRun()      { onCommand(CMD_RUN); }

// ============================================================================
//  编辑辅助
// ============================================================================
void MainWindow::clearSel()
{
    m_selActive = false;
    if (buffer) buffer->clearSelection();
}

bool MainWindow::hasSel() const
{
    return m_selActive;
}

void MainWindow::normalizeSel()
{
    if (cmpPos(m_selR1, m_selC1, m_selR2, m_selC2) > 0)
    {
        int tr = m_selR1, tc = m_selC1;
        m_selR1 = m_selR2; m_selC1 = m_selC2;
        m_selR2 = tr;      m_selC2 = tc;
    }
}

void MainWindow::syncSelToBuffer()
{
    bufSyncSelection(buffer, m_selActive, m_selR1, m_selC1, m_selR2, m_selC2);
}

void MainWindow::deleteSel()
{
    if (!m_selActive) return;
    syncSelToBuffer();
    buffer->deleteSelection();
    m_selActive = false;
}

void MainWindow::insertAtCursor(const std::string& s)
{
    if (s.empty()) return;
    if (m_selActive) deleteSel();
    bufInsertString(buffer, s);
}

void MainWindow::screenToDoc(int x, int y, int& row, int& col)
{
    Rect t = rText();
    int line = m_topLine + (y - t.y1) / UI_LINE_H;
    int total = buffer->getLineCount();
    if (line < 0) line = 0;
    if (line >= total) line = total - 1;

    std::string s = buffer->getLine(line);
    int disp = (x - t.x1 - 6) / (m_charW > 0 ? m_charW : 8) + m_leftCol;
    if (disp < 0) disp = 0;

    row = line;
    col = indexOfDispCol(s, disp);
}

void MainWindow::scrollToCursor()
{
    int row = buffer->getCursorY();
    int page = visibleLines();

    if (row < m_topLine) m_topLine = row;
    if (row >= m_topLine + page) m_topLine = row - page + 1;

    std::string s = buffer->getLine(row);
    int dcol = dispColOfIndex(s, buffer->getCursorX());
    int cols = rText().w() / (m_charW > 0 ? m_charW : 8);
    if (dcol < m_leftCol) m_leftCol = dcol;
    if (dcol >= m_leftCol + cols - 2) m_leftCol = dcol - cols + 4;
    if (m_leftCol < 0) m_leftCol = 0;
}

// ============================================================================
//  查找 / 替换
// ============================================================================
bool MainWindow::searchNext(bool forward)
{
    if (m_findText.empty()) { setStatus("请先输入查找内容", th->warn); return false; }

    int r, c, len;

    // 反向查找：从头扫一遍，记下光标之前的最后一个匹配
    if (!forward)
    {
        int curR = buffer->getCursorY(), curC = buffer->getCursorX();
        int bestR = -1, bestC = -1, bestLen = 0;
        int fromR = 0, fromC = 0;
        for (int guard = 0; guard < 20000; guard++)
        {
            int rr, cc, ll;
            if (!bufFindNext(buffer, m_findText, m_caseSensitive,
                             fromR, fromC, rr, cc, ll, false)) break;
            if (cmpPos(rr, cc, curR, curC) >= 0) break;   // 已经到光标之后了
            bestR = rr; bestC = cc; bestLen = ll;
            fromR = rr; fromC = cc + 1;
        }
        if (bestR < 0)
        {
            m_hasMatch = false;
            clearSel();
            setStatus("找不到 \"" + m_findText + "\"", th->warn);
            return false;
        }
        r = bestR; c = bestC; len = bestLen;
    }
    else
    {
        int fromR = buffer->getCursorY();
        int fromC = buffer->getCursorX() + 1;
        if (!bufFindNext(buffer, m_findText, m_caseSensitive,
                         fromR, fromC, r, c, len, true))
        {
            m_hasMatch = false;
            clearSel();
            setStatus("找不到 \"" + m_findText + "\"", th->warn);
            return false;
        }
    }

    m_hasMatch = true; m_matchR = r; m_matchC = c; m_matchLen = len;
    bufSetCursor(buffer, r, c);
    m_selR1 = r; m_selC1 = c;
    m_selR2 = r; m_selC2 = c + len;
    m_selActive = true;
    syncSelToBuffer();
    scrollToCursor();
    setStatus("找到匹配（第 " + itos(r + 1) + " 行）", th->ok);
    return true;
}

void MainWindow::replaceOne()
{
    if (m_findText.empty()) return;
    if (!m_hasMatch) { if (!searchNext(true)) return; }

    syncSelToBuffer();
    buffer->deleteSelection();
    bufInsertString(buffer, m_replaceText);
    m_selActive = false;
    m_hasMatch = false;
    setStatus("已替换 1 处", th->ok);
    searchNext(true);
}

void MainWindow::replaceAll()
{
    if (m_findText.empty()) { setStatus("请先输入查找内容", th->warn); return; }

    int n = bufReplaceAll(buffer, m_findText, m_replaceText, m_caseSensitive);
    clearSel();
    scrollToCursor();
    if (n > 0) setStatus("共替换 " + itos(n) + " 处", th->ok);
    else       setStatus("没有找到可替换的内容", th->warn);
}

// ============================================================================
//  剪贴板（Win32，UTF-8/GBK 都用 CF_TEXT，够用）
// ============================================================================
void MainWindow::setClipboardText(const std::string& s)
{
    if (s.empty()) return;
    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s.size() + 1);
    if (h)
    {
        char* p = (char*)GlobalLock(h);
        memcpy(p, s.c_str(), s.size() + 1);
        GlobalUnlock(h);
        SetClipboardData(CF_TEXT, h);
    }
    CloseClipboard();
}

std::string MainWindow::getClipboardText()
{
    std::string out;
    if (!OpenClipboard(NULL)) return out;
    HANDLE h = GetClipboardData(CF_TEXT);
    if (h)
    {
        const char* p = (const char*)GlobalLock(h);
        if (p) out = p;
        GlobalUnlock(h);
    }
    CloseClipboard();
    return out;
}
