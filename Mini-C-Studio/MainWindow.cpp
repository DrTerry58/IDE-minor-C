// ============================================================================
// 文件名：MainWindow.cpp
// 职责：主窗口全部实现（布局 / 绘制 / 事件 / 命令）
// 负责人：C（GUI 界面 + 交互控制）
// ============================================================================

#include "MainWindow.h"
#include "UiDialog.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <algorithm>

// ============================================================================
//  输入法（IME）支持
//  为什么需要这一段？
//    EasyX 的消息队列只会把 WM_CHAR 交给 peekmessage，而中文是输入法产生的，
//    走的是 WM_IME_COMPOSITION / WM_IME_CHAR，EasyX 不会转发 —— 所以中文"打不进去"。
//  解决办法：
//    给 EasyX 的窗口装一个窗口子类（SetWindowLongPtr），直接接管 IME 结果串，
//    转成 GBK 后交给 imeCommit() 插入到当前焦点（编辑器 / 查找 / 替换 / 控制台）。
// ============================================================================
#ifdef _MSC_VER
#  define MINIC_HAS_IMM 1
#endif

#ifdef MINIC_HAS_IMM
#  include <imm.h>
#  pragma comment(lib, "imm32.lib")
#else
// 非 MSVC 环境（例如用 MinGW 做语法校验）没有 imm.h，这里给出最小声明
#  ifndef _IMM_
#    ifndef GCS_RESULTSTR
#      define GCS_RESULTSTR 0x0800
#    endif
#    ifndef CFS_POINT
#      define CFS_POINT 0x0002
#    endif
DECLARE_HANDLE(HIMC);
typedef struct tagCOMPOSITIONFORM { DWORD dwStyle; POINT ptCurrentPos; RECT rcArea; } COMPOSITIONFORM, *LPCOMPOSITIONFORM;
extern "C" {
HIMC WINAPI ImmGetContext(HWND);
BOOL WINAPI ImmReleaseContext(HWND, HIMC);
LONG WINAPI ImmGetCompositionStringW(HIMC, DWORD, LPVOID, DWORD);
BOOL WINAPI ImmSetCompositionWindow(HIMC, LPCOMPOSITIONFORM);
}
#  endif
#endif

// ============================================================================
//  工具函数
// ============================================================================
namespace
{
    std::string itos(int v) { char b[32]; sprintf_s(b, "%d", v); return b; }

    int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // ---------------- C 语言词法着色 ----------------
    enum TokType { TOK_PLAIN, TOK_KW, TOK_TYPE, TOK_STR, TOK_CHR,
                   TOK_COMMENT, TOK_NUM, TOK_PREP, TOK_PUNCT };
    struct Tok { int b0, b1; TokType t; };

    const char* KW_CTRL[] = { "if","else","for","while","do","switch","case","default",
                              "break","continue","return","goto","sizeof","typedef",
                              "static","extern","const","volatile","register","auto","inline" };
    const char* KW_STRUCT[] = { "struct","union","enum" };
    const char* KW_TYPE[] = { "void","int","char","float","double","long","short",
                              "signed","unsigned","_Bool","FILE","size_t" };

    bool inList(const std::string& s, const char** L, int n)
    {
        for (int i = 0; i < n; i++) if (s == L[i]) return true;
        return false;
    }
    bool isIdStart(char c) { return isalpha((unsigned char)c) || c == '_'; }
    bool isIdChar(char c)  { return isalnum((unsigned char)c) || c == '_'; }
    bool isPunct(char c)   { return strchr("+-*/%=<>!&|^~?:;,.()[]{}", c) != NULL; }

    void pushTok(std::vector<Tok>& v, int a, int b, TokType t)
    {
        if (b <= a) return;
        Tok tk; tk.b0 = a; tk.b1 = b; tk.t = t; v.push_back(tk);
    }

    // 扫描一行，输出 token；inBlock 表示进入本行时是否处于块注释中
    void tokenizeLine(const std::string& s, bool inBlock, std::vector<Tok>& out, bool& outBlock)
    {
        int i = 0, n = (int)s.size();
        outBlock = false;

        if (inBlock)
        {
            size_t e = s.find("*/");
            if (e == std::string::npos) { pushTok(out, 0, n, TOK_COMMENT); outBlock = true; return; }
            pushTok(out, 0, (int)e + 2, TOK_COMMENT);
            i = (int)e + 2;
        }

        while (i < n)
        {
            char c = s[i];
            // 行注释
            if (c == '/' && i + 1 < n && s[i + 1] == '/') { pushTok(out, i, n, TOK_COMMENT); return; }
            // 块注释
            if (c == '/' && i + 1 < n && s[i + 1] == '*')
            {
                size_t e = s.find("*/", i + 2);
                if (e == std::string::npos) { pushTok(out, i, n, TOK_COMMENT); outBlock = true; return; }
                pushTok(out, i, (int)e + 2, TOK_COMMENT);
                i = (int)e + 2; continue;
            }
            // 预处理指令
            if (c == '#') { pushTok(out, i, n, TOK_PREP); return; }
            // 字符串
            if (c == '"')
            {
                int j = i + 1;
                while (j < n && s[j] != '"') { if (s[j] == '\\') j++; j++; }
                if (j < n) j++;
                pushTok(out, i, j, TOK_STR); i = j; continue;
            }
            // 字符常量
            if (c == '\'')
            {
                int j = i + 1;
                while (j < n && s[j] != '\'') { if (s[j] == '\\') j++; j++; }
                if (j < n) j++;
                pushTok(out, i, j, TOK_CHR); i = j; continue;
            }
            // 数字
            if (isdigit((unsigned char)c) || (c == '.' && i + 1 < n && isdigit((unsigned char)s[i + 1])))
            {
                int j = i;
                while (j < n && (isalnum((unsigned char)s[j]) || s[j] == '.' || s[j] == 'x' ||
                                 s[j] == 'X' || s[j] == '+' || s[j] == '-')) j++;
                pushTok(out, i, j, TOK_NUM); i = j; continue;
            }
            // 标识符 / 关键字
            if (isIdStart(c))
            {
                int j = i;
                while (j < n && isIdChar(s[j])) j++;
                std::string w = s.substr(i, j - i);
                TokType t = TOK_PLAIN;
                if (inList(w, KW_CTRL,  sizeof(KW_CTRL)  / sizeof(char*))) t = TOK_KW;
                else if (inList(w, KW_TYPE, sizeof(KW_TYPE) / sizeof(char*))) t = TOK_TYPE;
                else if (inList(w, KW_STRUCT, sizeof(KW_STRUCT) / sizeof(char*))) t = TOK_TYPE;
                pushTok(out, i, j, t); i = j; continue;
            }
            // 运算符
            if (isPunct(c)) { pushTok(out, i, i + 1, TOK_PUNCT); i++; continue; }
            // 其它（空格、中文等）
            {
                int nb = charBytes((unsigned char)c);
                pushTok(out, i, i + nb, TOK_PLAIN);
                i += nb;
            }
        }
    }

    COLORREF tokColor(const Theme& th, TokType t)
    {
        switch (t)
        {
        case TOK_KW:      return th.kw;
        case TOK_TYPE:    return th.kwType;
        case TOK_STR:
        case TOK_CHR:     return th.str;
        case TOK_COMMENT: return th.comment;
        case TOK_NUM:     return th.num;
        case TOK_PREP:    return th.prep;
        case TOK_PUNCT:   return th.punct;
        default:          return th.ident;
        }
    }

    // 大小写不敏感查找
    std::string lower(const std::string& s)
    {
        std::string r = s;
        for (size_t i = 0; i < r.size(); i++)
            if (r[i] >= 'A' && r[i] <= 'Z') r[i] += 32;
        return r;
    }
    int findInLine(const std::string& line, const std::string& pat, int from, bool cs)
    {
        if (pat.empty()) return -1;
        std::string L = cs ? line : lower(line);
        std::string P = cs ? pat : lower(pat);
        if (from < 0) from = 0;
        size_t p = L.find(P, (size_t)from);
        return p == std::string::npos ? -1 : (int)p;
    }
    int rfindInLine(const std::string& line, const std::string& pat, int from, bool cs)
    {
        if (pat.empty()) return -1;
        std::string L = cs ? line : lower(line);
        std::string P = cs ? pat : lower(pat);
        size_t f = (from < 0 || (size_t)from >= L.size()) ? std::string::npos : (size_t)from;
        size_t p = L.rfind(P, f);
        return p == std::string::npos ? -1 : (int)p;
    }

    // 裁剪区域守卫（EasyX setcliprgn 的 RAII 封装）
    struct ClipGuard
    {
        HRGN r;
        ClipGuard(const Rect& rc)
        {
            r = CreateRectRgn(rc.x1, rc.y1, rc.x2, rc.y2);
            setcliprgn(r);
        }
        ~ClipGuard() { setcliprgn(NULL); DeleteObject(r); }
    };
}

// ============================================================================
//  构造函数
// ============================================================================
MainWindow::MainWindow()
    : m_w(0), m_h(0), m_running(true), m_dark(false), th(&themeLight()),
      m_charW(9), m_tick(0), m_mouseX(-1), m_mouseY(-1),
      m_topLine(0), m_leftCol(0),
      m_selActive(false), m_selR1(0), m_selC1(0), m_selR2(0), m_selC2(0),
      m_anchorR(0), m_anchorC(0), m_dragging(false),
      m_dragV(false), m_dragH(false), m_dragStart(0),
      m_imeGuardUntil(0),
      m_focus(FOCUS_EDITOR), m_blockTopCache(-1), m_blockValCache(false), m_blockFrameCache(-999),
      m_openMenu(-1), m_hoverMenu(-1), m_hoverItem(-1), m_hoverTool(-1),
      m_bottomTab(1), m_diagTop(0), m_consoleTop(0), m_progRunning(false),
      m_findVisible(false), m_caseSensitive(false),
      m_hasMatch(false), m_matchR(0), m_matchC(0), m_matchLen(0),
      m_compileState(CS_NONE), m_compiling(false),
      m_statusMsg("就绪"), m_statusColor(themeLight().dim)
{
    buildMenus();
}

MainWindow::~MainWindow() {}

// ============================================================================
//  输入法（IME）支持：让中文能够输入
// ============================================================================
MainWindow* MainWindow::s_self    = 0;
WNDPROC     MainWindow::s_oldProc = 0;

LRESULT CALLBACK MainWindow::imeWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // ---- 输入法结果串（用户选词/敲空格后提交） ----
    if (uMsg == WM_IME_COMPOSITION)
    {
        if (lParam & GCS_RESULTSTR)
        {
            HIMC hImc = ImmGetContext(hWnd);
            if (hImc)
            {
                int len = ImmGetCompositionStringW(hImc, GCS_RESULTSTR, NULL, 0);
                if (len > 0)
                {
                    std::wstring ws((size_t)(len / 2), L'\0');
                    ImmGetCompositionStringW(hImc, GCS_RESULTSTR, &ws[0], (DWORD)len);
                    ImmReleaseContext(hWnd, hImc);
                    if (s_self) s_self->imeCommit(w2s(ws));   // UTF-16 -> GBK
                }
                else
                {
                    ImmReleaseContext(hWnd, hImc);
                }
            }
            return 0;      // 不再交给 DefWindowProc，避免额外生成 WM_CHAR 造成重复输入
        }
        return 0;
    }

    // ---- 兜底：个别输入法只发 WM_IME_CHAR，由上面的 COMPOSITION 分支统一处理 ----
    if (uMsg == WM_IME_CHAR) return 0;

    // ---- 输入法上下文建立时，把候选窗挪到光标处 ----
    if (uMsg == WM_IME_SETCONTEXT && wParam)
    {
        if (s_self) s_self->updateImePosition();
    }

    return CallWindowProc(s_oldProc, hWnd, uMsg, wParam, lParam);
}

void MainWindow::imeCommit(const std::string& gbk)
{
    if (gbk.empty()) return;

    if      (m_focus == FOCUS_FIND)    { m_findText    += gbk; return; }
    else if (m_focus == FOCUS_REPLACE) { m_replaceText += gbk; return; }
    else if (m_focus == FOCUS_CONSOLE) { m_inputLine   += gbk; return; }

    // 编辑器
    if (m_selActive) deleteSelection();
    CoreApi::inst().bufInsertString(gbk);
    scrollToCursor();
    updateImePosition();
    m_imeGuardUntil = GetTickCount() + 120;   // 防止同一批字符被 WM_CHAR 再插一次
}

void MainWindow::updateImePosition()
{
    HIMC hImc = ImmGetContext(GetHWnd());
    if (!hImc) return;

    CoreApi& api = CoreApi::inst();
    int row  = api.bufRow();
    int dcol = dispColOfIndex(api.bufGetLine(row), api.bufCol());
    int x    = rText().x1 + (dcol - m_leftCol) * m_charW;
    int y    = rText().y1 + (row - m_topLine) * UI_LINE_H + UI_LINE_H;

    COMPOSITIONFORM cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.dwStyle        = CFS_POINT;
    cf.ptCurrentPos.x = x;
    cf.ptCurrentPos.y = y;
    ImmSetCompositionWindow(hImc, &cf);
    ImmReleaseContext(GetHWnd(), hImc);
}

void MainWindow::buildMenus()
{
    // ---------------- 菜单栏 ----------------
    MenuDef m;
    m.title = "文件(F)"; m.items.clear();
    m.items.push_back(MenuItem{ "新建",         CMD_FILE_NEW,   "Ctrl+N" });
    m.items.push_back(MenuItem{ "打开...",      CMD_FILE_OPEN,  "Ctrl+O" });
    m.items.push_back(MenuItem{ "保存",         CMD_FILE_SAVE,  "Ctrl+S" });
    m.items.push_back(MenuItem{ "另存为...",    CMD_FILE_SAVEAS,"Ctrl+Shift+S" });
    m.items.push_back(MenuItem{ "关闭文档",     CMD_FILE_CLOSE, "Ctrl+W" });
    m.items.push_back(MenuItem{ "退出",         CMD_EXIT,       "Alt+F4" });
    m_menus.push_back(m);

    m.title = "编辑(E)"; m.items.clear();
    m.items.push_back(MenuItem{ "撤销",         CMD_EDIT_UNDO,   "Ctrl+Z" });
    m.items.push_back(MenuItem{ "重做",         CMD_EDIT_REDO,   "Ctrl+Y" });
    m.items.push_back(MenuItem{ "剪切",         CMD_EDIT_CUT,    "Ctrl+X" });
    m.items.push_back(MenuItem{ "复制",         CMD_EDIT_COPY,   "Ctrl+C" });
    m.items.push_back(MenuItem{ "粘贴",         CMD_EDIT_PASTE,  "Ctrl+V" });
    m.items.push_back(MenuItem{ "全选",         CMD_EDIT_SELALL, "Ctrl+A" });
    m_menus.push_back(m);

    m.title = "查找(S)"; m.items.clear();
    m.items.push_back(MenuItem{ "查找...",      CMD_FIND,        "Ctrl+F" });
    m.items.push_back(MenuItem{ "替换...",      CMD_REPLACE,     "Ctrl+H" });
    m.items.push_back(MenuItem{ "查找下一个",   CMD_FIND_NEXT,   "F3" });
    m.items.push_back(MenuItem{ "查找上一个",   CMD_FIND_PREV,   "Shift+F3" });
    m.items.push_back(MenuItem{ "替换当前",     CMD_REPLACE_ONE, "F4" });
    m.items.push_back(MenuItem{ "全部替换",     CMD_REPLACE_ALL, "Ctrl+Shift+R" });
    m_menus.push_back(m);

    m.title = "编译(C)"; m.items.clear();
    m.items.push_back(MenuItem{ "编译",         CMD_COMPILE,     "F7" });
    m.items.push_back(MenuItem{ "运行",         CMD_RUN,         "F5" });
    m.items.push_back(MenuItem{ "编译并运行",   CMD_COMPILE_RUN, "Ctrl+F5" });
    m.items.push_back(MenuItem{ "停止运行",     CMD_STOP,        "Ctrl+Break" });
    m_menus.push_back(m);

    m.title = "视图(V)"; m.items.clear();
    m.items.push_back(MenuItem{ "切换亮/暗主题", CMD_THEME,      "Ctrl+T" });
    m_menus.push_back(m);

    m.title = "帮助(H)"; m.items.clear();
    m.items.push_back(MenuItem{ "关于 Mini-C Studio", CMD_ABOUT, "F1" });
    m_menus.push_back(m);

    // ---------------- 工具栏 ----------------
    m_tools.push_back(ToolBtn{ "新建",     CMD_FILE_NEW });
    m_tools.push_back(ToolBtn{ "打开",     CMD_FILE_OPEN });
    m_tools.push_back(ToolBtn{ "保存",     CMD_FILE_SAVE });
    m_tools.push_back(ToolBtn{ "另存为",   CMD_FILE_SAVEAS });
    m_tools.push_back(ToolBtn{ "",         CMD_NONE });      // 分隔符
    m_tools.push_back(ToolBtn{ "撤销",     CMD_EDIT_UNDO });
    m_tools.push_back(ToolBtn{ "重做",     CMD_EDIT_REDO });
    m_tools.push_back(ToolBtn{ "剪切",     CMD_EDIT_CUT });
    m_tools.push_back(ToolBtn{ "复制",     CMD_EDIT_COPY });
    m_tools.push_back(ToolBtn{ "粘贴",     CMD_EDIT_PASTE });
    m_tools.push_back(ToolBtn{ "",         CMD_NONE });
    m_tools.push_back(ToolBtn{ "查找",     CMD_FIND });
    m_tools.push_back(ToolBtn{ "替换",     CMD_REPLACE });
    m_tools.push_back(ToolBtn{ "",         CMD_NONE });
    m_tools.push_back(ToolBtn{ "编译",     CMD_COMPILE });
    m_tools.push_back(ToolBtn{ "运行",     CMD_RUN });
    m_tools.push_back(ToolBtn{ "编译运行", CMD_COMPILE_RUN });
    m_tools.push_back(ToolBtn{ "停止",     CMD_STOP });
    m_tools.push_back(ToolBtn{ "",         CMD_NONE });
    m_tools.push_back(ToolBtn{ "主题",     CMD_THEME });
    m_tools.push_back(ToolBtn{ "关于",     CMD_ABOUT });
}

// ============================================================================
//  初始化窗口
// ============================================================================
bool MainWindow::initWindow(int w, int h)
{
    m_w = w; m_h = h;
    initgraph(w, h, EW_SHOWCONSOLE);
    SetWindowText(GetHWnd(), _T("Mini-C Studio - C 语言集成开发环境"));

    // 计算等宽字符宽度
    settextstyle(UI_FONT_H, 0, _T("Consolas"));
    m_charW = textwidth(_T("M"));
    if (m_charW <= 0) m_charW = 9;

    setbkmode(TRANSPARENT);
    setbkcolor(th->bg);
    cleardevice();
    BeginBatchDraw();

    // 安装窗口子类：接管 IME，让中文可以输入（必须在 initgraph 之后）
    s_self    = this;
    s_oldProc = (WNDPROC)(LONG_PTR)SetWindowLongPtr(GetHWnd(), GWLP_WNDPROC,
                                                    (LONG_PTR)imeWndProc);

    loadWelcomeContent();
    return true;
}

void MainWindow::loadWelcomeContent()
{
    std::string demo =
        "#include <stdio.h>\n"
        "\n"
        "int main(void)\n"
        "{\n"
        "    printf(\"Hello, Mini-C Studio!\\n\");\n"
        "    return 0;\n"
        "}\n";
    CoreApi::inst().bufLoad(demo);
    CoreApi::inst().bufSetDirty(false);
    appendConsole("Mini-C Studio " + std::string(MINIC_VERSION) + " 已启动。\n"
                  "提示：F7 编译，F5 运行，Ctrl+F 查找。");
}

// ============================================================================
//  主循环
// ============================================================================
void MainWindow::run()
{
    while (m_running)
    {
        pumpMessages();
        update();
        render();
        Sleep(16);
        m_tick++;
    }
    EndBatchDraw();
}

void MainWindow::update()
{
    CoreApi& api = CoreApi::inst();
    clampScroll();
    if (m_progRunning)
    {
        std::string out; int code = 0; bool fin = false;
        if (api.runPoll(out, code, fin))
        {
            if (!out.empty()) appendConsole(out);
            if (fin)
            {
                m_progRunning = false;
                if (code == 0)
                {
                    appendConsole("\n[程序已结束，退出码 0]");
                    setStatus("程序运行结束（正常）", th->ok);
                }
                else
                {
                    appendConsole("\n[程序异常结束，退出码 " + itos(code) + "]");
                    setStatus("程序异常结束，退出码 " + itos(code), th->err);
                    uiAlert(m_w, m_h, *th, "运行时异常",
                            "程序以非零退出码结束（" + itos(code) + "）。\n"
                            "可能原因：运行时错误、崩溃或访问非法内存。\n"
                            "详细信息见控制台面板。");
                }
            }
        }
    }
}

// ============================================================================
//  布局（绘制与命中测试共用同一套坐标）
// ============================================================================
Rect MainWindow::rMenu()   const { return Rect(0, 0, m_w, UI_MENU_H); }
Rect MainWindow::rTool()   const { return Rect(0, UI_MENU_H, m_w, UI_MENU_H + UI_TOOL_H); }
Rect MainWindow::rEdit()   const { return Rect(0, UI_MENU_H + UI_TOOL_H, m_w, m_h - UI_STATUS_H - UI_BOTTOM_H); }
Rect MainWindow::rBottom() const { return Rect(0, m_h - UI_STATUS_H - UI_BOTTOM_H, m_w, m_h - UI_STATUS_H); }
Rect MainWindow::rStatus() const { return Rect(0, m_h - UI_STATUS_H, m_w, m_h); }
Rect MainWindow::rGutter() const { Rect e = rEdit(); return Rect(e.x1, e.y1, e.x1 + UI_GUTTER_W, e.y2); }
Rect MainWindow::rText()   const
{
    Rect e = rEdit();
    int top = e.y1 + (m_findVisible ? UI_FINDBAR_H : 0);
    return Rect(e.x1 + UI_GUTTER_W, top, e.x2 - UI_SCROLL_W, e.y2 - UI_SCROLL_W);
}
Rect MainWindow::rVScroll() const { Rect e = rEdit(); return Rect(e.x2 - UI_SCROLL_W, e.y1, e.x2, e.y2 - UI_SCROLL_W); }
Rect MainWindow::rHScroll() const { Rect e = rEdit(); return Rect(e.x1, e.y2 - UI_SCROLL_W, e.x2 - UI_SCROLL_W, e.y2); }

// 最长行的显示列数 —— 水平滚动条的 total（全角算 2 列）
int MainWindow::maxLineCols()
{
    CoreApi& api = CoreApi::inst();
    int total = api.bufLineCount();
    int maxw = 0;
    for (int i = 0; i < total; i++)
    {
        int wd = dispWidth(api.bufGetLine(i));
        if (wd > maxw) maxw = wd;
    }
    return maxw;
}

// 编辑区一屏能显示的列数 —— 水平滚动条的 page
int MainWindow::hPageCols()
{
    int page = rText().w() / (m_charW > 0 ? m_charW : 9);
    if (page < 1) page = 1;
    return page;
}

// 把两个滚动偏移钳回合法范围。
// 没有它的话：删掉长行 / 删掉若干行之后，旧的偏移会残留成越界值，
// 表现为"内容能被推到看不见的地方"或"底部露出空白"。
void MainWindow::clampScroll()
{
    CoreApi& api = CoreApi::inst();
    int page = visibleLines();
    int maxTop = api.bufLineCount() - page;
    if (maxTop < 0) maxTop = 0;
    m_topLine = clampi(m_topLine, 0, maxTop);

    int maxLeft = maxLineCols() - hPageCols();
    if (maxLeft < 0) maxLeft = 0;
    m_leftCol = clampi(m_leftCol, 0, maxLeft);
}
Rect MainWindow::rFindBar() const { Rect e = rEdit(); return Rect(e.x1, e.y1, e.x2, e.y1 + UI_FINDBAR_H); }
Rect MainWindow::rBottomTabs()   const { Rect b = rBottom(); return Rect(b.x1, b.y1, b.x2, b.y1 + UI_TAB_H); }
Rect MainWindow::rBottomBody()   const { Rect b = rBottom(); return Rect(b.x1 + 1, b.y1 + UI_TAB_H, b.x2 - UI_SCROLL_W, b.y2 - 1); }
Rect MainWindow::rBottomScroll() const { Rect b = rBottom(); return Rect(b.x2 - UI_SCROLL_W, b.y1 + UI_TAB_H, b.x2, b.y2 - 1); }
Rect MainWindow::rConsoleBody()  const
{
    Rect b = rBottomBody();
    return Rect(b.x1, b.y1, b.x2, (m_bottomTab == 1) ? b.y2 - UI_CONSOLE_INPUT_H : b.y2);
}
Rect MainWindow::rConsoleInput() const { Rect b = rBottom(); return Rect(b.x1 + 1, b.y2 - UI_CONSOLE_INPUT_H, b.x2 - 1, b.y2 - 1); }

Rect MainWindow::dropRect(int mi) const
{
    int x = mi * UI_MENU_TITLE_W;
    int n = (int)m_menus[mi].items.size();
    return Rect(x, UI_MENU_H, x + UI_DROP_W, UI_MENU_H + 6 + n * UI_DROP_ITEM_H + 6);
}
Rect MainWindow::dropItemRect(int mi, int ii) const
{
    Rect d = dropRect(mi);
    int y = d.y1 + 6 + ii * UI_DROP_ITEM_H;
    return Rect(d.x1 + 4, y, d.x2 - 4, y + UI_DROP_ITEM_H);
}
Rect MainWindow::toolBtnRect(int i) const
{
    settextstyle(13, 0, _T("Microsoft YaHei"));
    Rect t = rTool();
    int x = 8;
    for (int k = 0; k <= i && k < (int)m_tools.size(); k++)
    {
        if (m_tools[k].cmd == CMD_NONE) { if (k < i) x += 14; continue; }
        int w = strWidth(m_tools[k].label) + 18;
        if (w < 40) w = 40;
        if (k == i) return Rect(x, t.y1 + 5, x + w, t.y2 - 5);
        x += w + 6;
    }
    return Rect(0, 0, 0, 0);
}

MainWindow::FindLayout MainWindow::findLayout() const
{
    FindLayout L;
    Rect bar = rFindBar();
    L.bar = bar;
    int x = 10, y = bar.y1 + 5, hh = bar.h() - 10;

    settextstyle(14, 0, _T("Microsoft YaHei"));
    drawStr(x, y + 3, "查找:"); x += 46;
    L.fInput = Rect(x, y, x + 170, y + hh); x += 178;
    drawStr(x, y + 3, "替换:"); x += 46;
    L.rInput = Rect(x, y, x + 170, y + hh); x += 178;
    L.chk    = Rect(x, y + 3, x + 16, y + 19); x += 22;
    drawStr(x, y + 3, "区分大小写"); x += 84;

    L.bPrev   = Rect(x, y, x + 34, y + hh);               x += 38;
    L.bNext   = Rect(x, y, x + 34, y + hh);               x += 38;
    L.bRep    = Rect(x, y, x + 46, y + hh);               x += 50;
    L.bRepAll = Rect(x, y, x + 66, y + hh);               x += 70;
    L.bClose  = Rect(bar.x2 - 34, y, bar.x2 - 8, y + hh);
    return L;
}

std::string MainWindow::tabLabel(int i) const
{
    int errN = 0, warnN = 0;
    for (size_t k = 0; k < m_diagnostics.size(); k++)
    {
        if (m_diagnostics[k].level == DIAG_ERROR) errN++;
        else if (m_diagnostics[k].level == DIAG_WARNING) warnN++;
    }
    if (i == 0) return "诊断  " + itos(errN) + " 错误 / " + itos(warnN) + " 警告";
    return "控制台";
}

int MainWindow::visibleLines() const
{
    int v = rText().h() / UI_LINE_H;
    return v > 0 ? v : 1;
}

// ============================================================================
//  消息处理
// ============================================================================
void MainWindow::pumpMessages()
{
    ExMessage msg;
    while (peekmessage(&msg, EX_MOUSE | EX_KEY | EX_CHAR | EX_WINDOW, true))
    {
        switch (msg.message)
        {
        case WM_MOUSEMOVE:
            m_mouseX = msg.x; m_mouseY = msg.y;
            onMouseMove(msg.x, msg.y);
            break;
        case WM_LBUTTONDOWN:
            m_mouseX = msg.x; m_mouseY = msg.y;
            onMouseDown(msg.x, msg.y);
            break;
        case WM_LBUTTONUP:
            onMouseUp(msg.x, msg.y);
            break;
        case WM_MOUSEWHEEL:
            onWheel(msg.wheel);
            break;
        case WM_KEYDOWN:
            onKeyDown(msg.vkcode,
                      (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0,
                      (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0,
                      (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0);
            break;
        case WM_CHAR:
            onCharInput((unsigned int)msg.ch);
            break;
        case WM_CLOSE:
            doExit();
            break;
        default:
            break;
        }
    }
}

void MainWindow::onMouseDown(int x, int y)
{
    CoreApi& api = CoreApi::inst();

    // ---------- 1. 菜单下拉（最高优先级） ----------
    if (m_openMenu >= 0)
    {
        Rect d = dropRect(m_openMenu);
        if (d.hit(x, y))
        {
            for (int i = 0; i < (int)m_menus[m_openMenu].items.size(); i++)
            {
                if (dropItemRect(m_openMenu, i).hit(x, y))
                {
                    int cmd = m_menus[m_openMenu].items[i].cmd;
                    m_openMenu = -1;
                    execCmd(cmd);
                    return;
                }
            }
            return;
        }
        m_openMenu = -1;   // 点击别处则关闭
    }

    // ---------- 2. 菜单栏 ----------
    if (rMenu().hit(x, y))
    {
        int idx = x / UI_MENU_TITLE_W;
        if (idx >= 0 && idx < (int)m_menus.size())
            m_openMenu = (m_openMenu == idx) ? -1 : idx;
        return;
    }

    // ---------- 3. 工具栏 ----------
    if (rTool().hit(x, y))
    {
        for (int i = 0; i < (int)m_tools.size(); i++)
        {
            if (m_tools[i].cmd == CMD_NONE) continue;
            if (toolBtnRect(i).hit(x, y)) { execCmd(m_tools[i].cmd); return; }
        }
        return;
    }

    // ---------- 4. 查找 / 替换条 ----------
    if (m_findVisible && rFindBar().hit(x, y))
    {
        FindLayout L = findLayout();
        if (L.fInput.hit(x, y))  { m_focus = FOCUS_FIND;    return; }
        if (L.rInput.hit(x, y))  { m_focus = FOCUS_REPLACE; return; }
        if (L.chk.hit(x, y))     { m_caseSensitive = !m_caseSensitive; return; }
        if (L.bPrev.hit(x, y))   { searchNext(false); return; }
        if (L.bNext.hit(x, y))   { searchNext(true);  return; }
        if (L.bRep.hit(x, y))    { replaceOne();      return; }
        if (L.bRepAll.hit(x, y)) { replaceAll();      return; }
        if (L.bClose.hit(x, y))  { m_findVisible = false; m_focus = FOCUS_EDITOR; return; }
        return;
    }

    // ---------- 5. 底部面板 ----------
    if (rBottom().hit(x, y))
    {
        Rect tb = rBottomTabs();
        settextstyle(13, 0, _T("Microsoft YaHei"));
        int bx = tb.x1 + 8;
        for (int i = 0; i < 2; i++)
        {
            int w = strWidth(tabLabel(i)) + 26;
            if (x >= bx && x <= bx + w && y >= tb.y1 && y <= tb.y2)
            { m_bottomTab = i; return; }
            bx += w + 4;
        }

        if (rConsoleInput().hit(x, y) && m_bottomTab == 1) { m_focus = FOCUS_CONSOLE; return; }
        if (rBottomBody().hit(x, y))
        {
            if (m_bottomTab == 0)
            {
                // 点击诊断条目 -> 跳转到源码对应行
                Rect body = rBottomBody();
                int idx = m_diagTop + (y - body.y1 - 4) / 22;
                if (idx >= 0 && idx < (int)m_diagnostics.size())
                {
                    const Diagnostic& d = m_diagnostics[idx];
                    api.bufGotoLine(d.line);
                    api.bufSetCursor(d.line, d.column);
                    clearSelection();
                    scrollToCursor();
                    m_focus = FOCUS_EDITOR;
                }
            }
            else m_focus = FOCUS_CONSOLE;
            return;
        }

        // 底部滚动条
        if (rBottomScroll().hit(x, y))
        {
            int total = (m_bottomTab == 0) ? (int)m_diagnostics.size() : (int)m_console.size();
            int page  = (m_bottomTab == 0) ? (rBottomBody().h() / 22)
                                           : (rConsoleBody().h() / 18);
            int& top = (m_bottomTab == 0) ? m_diagTop : m_consoleTop;
            if (total > page)
            {
                int track = rBottomScroll().h();
                int kh = (int)((double)track * page / total);
                if (kh < 24) kh = 24;
                int maxp = track - kh;
                int knobY = rBottomScroll().y1 + (total - page > 0 ? top * maxp / (total - page) : 0);
                if (y < knobY)          top -= page;
                else if (y > knobY + kh) top += page;
                else { m_dragV = true; m_dragStart = y - knobY; }
                top = clampi(top, 0, total - page);
            }
            return;
        }
        return;
    }

    // ---------- 6. 编辑区滚动条 ----------
    if (rVScroll().hit(x, y))
    {
        int total = api.bufLineCount();
        int page  = visibleLines();
        if (total > page)
        {
            int track = rVScroll().h();
            int kh = (int)((double)track * page / total);
            if (kh < 24) kh = 24;
            int maxp = track - kh;
            int knobY = rVScroll().y1 + (total - page > 0 ? m_topLine * maxp / (total - page) : 0);
            if (y < knobY)            m_topLine -= page;
            else if (y > knobY + kh)  m_topLine += page;
            else { m_dragV = true; m_dragStart = y - knobY; }
            m_topLine = clampi(m_topLine, 0, total - page);
        }
        return;
    }
    if (rHScroll().hit(x, y))
    {
        int total = maxLineCols();
        int page  = hPageCols();
        if (total > page)
        {
            int track = rHScroll().w();
            int kh = (int)((double)track * page / total);
            if (kh < 24) kh = 24;
            int maxp = track - kh;
            int knobX = rHScroll().x1 + (total - page > 0 ? m_leftCol * maxp / (total - page) : 0);
            if (x < knobX)            m_leftCol -= page;          // 滑块左侧空白：向左翻一页
            else if (x > knobX + kh)  m_leftCol += page;          // 滑块右侧空白：向右翻一页
            else { m_dragH = true; m_dragStart = x - knobX; }     // 按在滑块上：开始拖动
            int maxLeft = (total - page > 0) ? (total - page) : 0;
            m_leftCol = clampi(m_leftCol, 0, maxLeft);
        }
        return;
    }

    // ---------- 7. 编辑区：定位光标 / 开始选择 ----------
    if (rText().hit(x, y))
    {
        m_focus = FOCUS_EDITOR;
        int row = 0, col = 0;
        screenToDoc(x, y, row, col);
        bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        if (!shift)
        {
            m_anchorR = row; m_anchorC = col;
            clearSelection();
        }
        else if (!m_selActive)
        {
            m_anchorR = api.bufRow(); m_anchorC = api.bufCol();
        }
        api.bufSetCursor(row, col);
        m_dragging = true;
        scrollToCursor();
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
    // 拖拽选区
    if (m_dragging)
    {
        if (!m_selActive) { m_selActive = true; }
        int row = 0, col = 0;
        screenToDoc(x, y, row, col);
        CoreApi::inst().bufSetCursor(row, col);
        m_selR1 = m_anchorR; m_selC1 = m_anchorC;
        m_selR2 = row;       m_selC2 = col;
        normalizeSel();
        scrollToCursor();
        return;
    }

    // 拖拽垂直滚动条
    if (m_dragV)
    {
        CoreApi& api = CoreApi::inst();
        Rect r = rVScroll();
        int total = api.bufLineCount();
        int page  = visibleLines();
        if (total > page)
        {
            int track = r.h();
            int kh = (int)((double)track * page / total);
            if (kh < 24) kh = 24;
            int maxp = track - kh;
            int ny = y - m_dragStart - r.y1;
            m_topLine = (maxp > 0) ? (int)((double)ny * (total - page) / maxp) : 0;
            m_topLine = clampi(m_topLine, 0, total - page);
        }
        return;
    }
    if (m_dragH)
    {
        // 与垂直拖动同一套算法：由滑块当前位置反算滚动偏移，再钳到 [0, total-page]。
        // 旧代码是 m_leftCol -= dx：① 方向反了 ② 只钳下界不钳上界 -> 内容能被无限往右推。
        Rect r = rHScroll();
        int total = maxLineCols();
        int page  = hPageCols();
        if (total > page)
        {
            int track = r.w();
            int kh = (int)((double)track * page / total);
            if (kh < 24) kh = 24;
            int maxp = track - kh;
            int nx = x - m_dragStart - r.x1;      // 滑块左上角相对轨道的偏移
            m_leftCol = (maxp > 0) ? (int)((double)nx * (total - page) / maxp) : 0;
            m_leftCol = clampi(m_leftCol, 0, total - page);
        }
        return;
    }

    // 菜单展开时，鼠标滑过标题自动切换
    if (m_openMenu >= 0 && rMenu().hit(x, y))
    {
        int idx = x / UI_MENU_TITLE_W;
        if (idx >= 0 && idx < (int)m_menus.size() && idx != m_openMenu) m_openMenu = idx;
    }
}

void MainWindow::onWheel(int delta)
{
    int d = delta / 120;
    if (d == 0) d = (delta > 0 ? 1 : -1);

    if (rBottom().hit(m_mouseX, m_mouseY))
    {
        int total = (m_bottomTab == 0) ? (int)m_diagnostics.size() : (int)m_console.size();
        int page  = (m_bottomTab == 0) ? (rBottomBody().h() / 22) : (rConsoleBody().h() / 18);
        int& top  = (m_bottomTab == 0) ? m_diagTop : m_consoleTop;
        top -= d * 2;
        top = clampi(top, 0, (total > page ? total - page : 0));
        return;
    }

    CoreApi& api = CoreApi::inst();
    int total = api.bufLineCount();
    int page  = visibleLines();
    m_topLine -= d * 3;
    m_topLine = clampi(m_topLine, 0, (total > page ? total - page : 0));
}

void MainWindow::onKeyDown(int vk, bool ctrl, bool shift, bool alt)
{
    CoreApi& api = CoreApi::inst();

    // ---------- 菜单打开时的键盘操作 ----------
    if (m_openMenu >= 0)
    {
        if (vk == VK_ESCAPE) m_openMenu = -1;
        else if (vk == VK_LEFT)  { m_openMenu = (m_openMenu - 1 + (int)m_menus.size()) % (int)m_menus.size(); }
        else if (vk == VK_RIGHT) { m_openMenu = (m_openMenu + 1) % (int)m_menus.size(); }
        return;
    }

    // ---------- Alt + F4 ----------
    if (alt && vk == VK_F4) { doExit(); return; }

    // ---------- 全局快捷键 ----------
    if (ctrl)
    {
        // 粘贴按当前焦点分发到不同的输入框
        if (vk == 'V')
        {
            std::string t = getClipboardText();
            if (!t.empty())
            {
                if      (m_focus == FOCUS_FIND)    { m_findText += t;    return; }
                else if (m_focus == FOCUS_REPLACE) { m_replaceText += t; return; }
                else if (m_focus == FOCUS_CONSOLE) { m_inputLine += t;   return; }
            }
        }
        switch (vk)
        {
        case 'N': execCmd(CMD_FILE_NEW);   return;
        case 'O': execCmd(CMD_FILE_OPEN);  return;
        case 'S': execCmd(shift ? CMD_FILE_SAVEAS : CMD_FILE_SAVE); return;
        case 'Z': execCmd(shift ? CMD_EDIT_REDO : CMD_EDIT_UNDO); clearSelection(); scrollToCursor(); return;
        case 'Y': execCmd(CMD_EDIT_REDO);  clearSelection(); scrollToCursor(); return;
        case 'A': execCmd(CMD_EDIT_SELALL);return;
        case 'C': execCmd(CMD_EDIT_COPY);  return;
        case 'X': execCmd(CMD_EDIT_CUT);   scrollToCursor(); return;
        case 'V': execCmd(CMD_EDIT_PASTE); scrollToCursor(); return;
        case 'F': execCmd(CMD_FIND);       return;
        case 'H': execCmd(CMD_REPLACE);    return;
        case 'B': execCmd(CMD_COMPILE);    return;
        case 'R': if (shift && m_findVisible) execCmd(CMD_REPLACE_ALL); else execCmd(CMD_RUN); return;
        case 'T': execCmd(CMD_THEME);      return;
        case 'W': execCmd(CMD_FILE_CLOSE); return;
        default: break;
        }
    }
    if (vk == VK_F1) { execCmd(CMD_ABOUT); return; }
    if (vk == VK_F3) { if (!m_findVisible) { m_findVisible = true; m_focus = FOCUS_FIND; } searchNext(!shift); return; }
    if (vk == VK_F5) { execCmd(ctrl ? CMD_COMPILE_RUN : CMD_RUN); return; }
    if (vk == VK_F7) { execCmd(CMD_COMPILE); return; }
    if (vk == VK_F4) { if (m_findVisible) execCmd(CMD_REPLACE_ONE); return; }
    if (vk == VK_ESCAPE)
    {
        if (m_findVisible) { m_findVisible = false; m_focus = FOCUS_EDITOR; return; }
        if (m_selActive)   { clearSelection(); return; }
        return;
    }

    // ---------- 查找框 / 替换框 ----------
    if (m_focus == FOCUS_FIND || m_focus == FOCUS_REPLACE)
    {
        if (vk == VK_RETURN) { searchNext(!shift); return; }
        if (vk == VK_TAB)    { m_focus = (m_focus == FOCUS_FIND) ? FOCUS_REPLACE : FOCUS_FIND; return; }
        if (vk == VK_BACK)
        {
            std::string& s = (m_focus == FOCUS_FIND) ? m_findText : m_replaceText;
            if (!s.empty())
            {
                // 同 deleteChar：必须扫描定位，不能靠 isDbcsLead 猜
                int st = charStartBefore(s, (int)s.size());
                s.erase(st);
            }
            return;
        }
        return;   // 其余交给 WM_CHAR
    }

    // ---------- 控制台输入 ----------
    if (m_focus == FOCUS_CONSOLE)
    {
        if (vk == VK_RETURN)
        {
            appendConsole("> " + m_inputLine);
            if (m_progRunning)
                CoreApi::inst().runSendInput(m_inputLine);
            else
                appendConsole("[提示] 当前没有正在运行的程序，输入被忽略。请先按 F5 运行。");
            m_inputLine.clear();
            return;
        }
        if (vk == VK_BACK && !m_inputLine.empty())
        {
            int st = charStartBefore(m_inputLine, (int)m_inputLine.size());
            m_inputLine.erase(st);
            return;
        }
        return;
    }

    // ---------- 编辑器 ----------
    switch (vk)
    {
    case VK_LEFT:
        if (shift && !m_selActive) { m_selActive = true; m_anchorR = api.bufRow(); m_anchorC = api.bufCol(); }
        api.bufMoveCursor(-1, 0);
        break;
    case VK_RIGHT:
        if (shift && !m_selActive) { m_selActive = true; m_anchorR = api.bufRow(); m_anchorC = api.bufCol(); }
        api.bufMoveCursor(1, 0);
        break;
    case VK_UP:
        if (shift && !m_selActive) { m_selActive = true; m_anchorR = api.bufRow(); m_anchorC = api.bufCol(); }
        api.bufMoveCursor(0, -1);
        break;
    case VK_DOWN:
        if (shift && !m_selActive) { m_selActive = true; m_anchorR = api.bufRow(); m_anchorC = api.bufCol(); }
        api.bufMoveCursor(0, 1);
        break;
    case VK_HOME:
        if (ctrl) { api.bufSetCursor(0, 0); }                     // Ctrl+Home：跳到文档开头
        else      { api.bufSetCursor(api.bufRow(), 0); }
        break;
    case VK_END:
        if (ctrl)                                                  // Ctrl+End：跳到文档末尾
        {
            int lr = api.bufLineCount() - 1;
            if (lr < 0) lr = 0;
            api.bufSetCursor(lr, (int)api.bufGetLine(lr).size());
        }
        else { api.bufSetCursor(api.bufRow(), (int)api.bufGetLine(api.bufRow()).size()); }
        break;
    case VK_PRIOR:   // PageUp
        api.bufSetCursor(api.bufRow() - visibleLines(), api.bufCol());
        break;
    case VK_NEXT:    // PageDown
        api.bufSetCursor(api.bufRow() + visibleLines(), api.bufCol());
        break;
    case VK_BACK:
        if (m_selActive) { deleteSelection(); }
        else api.bufDeleteBack();
        break;
    case VK_DELETE:
        if (m_selActive) { deleteSelection(); }
        else api.bufDeleteForward();
        break;
    case VK_RETURN:
        if (m_selActive) deleteSelection();
        api.bufEnter();
        break;
    case VK_TAB:
        if (m_selActive) deleteSelection();
        api.bufInsertString("    ");
        break;
    default:
        return;
    }

    // 选区随光标更新
    if (shift && m_selActive)
    {
        m_selR1 = m_anchorR; m_selC1 = m_anchorC;
        m_selR2 = api.bufRow(); m_selC2 = api.bufCol();
        normalizeSel();
    }
    else if (m_selActive && vk != VK_BACK && vk != VK_DELETE)
    {
        clearSelection();
    }
    scrollToCursor();
}

void MainWindow::onCharInput(unsigned int code)
{
    std::string bytes;
#ifdef UNICODE
    if (code < 128) bytes = std::string(1, (char)code);
    else            bytes = w2s(std::wstring(1, (wchar_t)code));
#else
    unsigned char b = (unsigned char)(code & 0xFF);
    if (b < 0x80) bytes = std::string(1, (char)b);
    else if (GetTickCount() < m_imeGuardUntil)
    {
        // 刚刚由 IME 提交过，这里的 WM_CHAR 是重复的一份，丢弃
        m_dbcsPending.clear();
        return;
    }
    else
    {
        m_dbcsPending += (char)b;
        if ((int)m_dbcsPending.size() >= 2) { bytes = m_dbcsPending; m_dbcsPending.clear(); }
        else return;
    }
#endif
    if (bytes.empty()) return;
    if (bytes[0] < 32) return;   // 所有控制字符（含 Tab/回车/退格）都由 WM_KEYDOWN 统一处理

    if (m_focus == FOCUS_FIND)         { m_findText += bytes; return; }
    if (m_focus == FOCUS_REPLACE)      { m_replaceText += bytes; return; }
    if (m_focus == FOCUS_CONSOLE)      { m_inputLine += bytes; return; }

    // 编辑器
    if (m_selActive) deleteSelection();
    CoreApi::inst().bufInsertString(bytes);
    scrollToCursor();
}

// ============================================================================
//  绘制
// ============================================================================
void MainWindow::render()
{
    setbkmode(TRANSPARENT);
    setbkcolor(th->bg);
    cleardevice();

    drawEditor();
    if (m_findVisible) drawFindBar();
    drawBottom();
    drawToolbar();
    drawMenuBar();
    drawStatusBar();
    if (m_openMenu >= 0) drawDropdown();   // 下拉菜单永远在最上层

    FlushBatchDraw();
}

void MainWindow::drawMenuBar()
{
    Rect r = rMenu();
    fillRect(r, th->menuBg);
    setlinecolor(th->border);
    line(0, r.y2, m_w, r.y2);

    settextstyle(14, 0, _T("Microsoft YaHei"));
    for (int i = 0; i < (int)m_menus.size(); i++)
    {
        Rect t(i * UI_MENU_TITLE_W, r.y1, (i + 1) * UI_MENU_TITLE_W, r.y2);
        bool active = (m_openMenu == i) || (m_openMenu < 0 && t.hit(m_mouseX, m_mouseY));
        if (active) fillRect(Rect(t.x1 + 2, t.y1 + 2, t.x2 - 2, t.y2 - 2), th->menuHover);
        settextcolor(active ? th->menuTextHover : th->menuText);
        drawStrCenter(t, m_menus[i].title);
    }
}

void MainWindow::drawDropdown()
{
    if (m_openMenu < 0) return;
    Rect d = dropRect(m_openMenu);

    // 阴影
    fillRoundRect(Rect(d.x1 + 3, d.y1 + 3, d.x2 + 3, d.y2 + 3), 8, RGB(0, 0, 0));
    fillRoundRect(d, 8, th->panel);
    setlinecolor(th->border);
    // 边框用直线近似
    line(d.x1 + 8, d.y1, d.x2 - 8, d.y1);
    line(d.x1 + 8, d.y2, d.x2 - 8, d.y2);

    settextstyle(14, 0, _T("Microsoft YaHei"));
    for (int i = 0; i < (int)m_menus[m_openMenu].items.size(); i++)
    {
        const MenuItem& it = m_menus[m_openMenu].items[i];
        Rect ir = dropItemRect(m_openMenu, i);
        bool hv = ir.hit(m_mouseX, m_mouseY);
        if (hv) fillRoundRect(Rect(ir.x1, ir.y1 + 1, ir.x2, ir.y2 - 1), 4, th->menuHover);
        settextcolor(hv ? th->menuTextHover : th->menuText);
        drawStr(ir.x1 + 12, ir.y1 + 4, it.label);
        if (!it.hot.empty())
        {
            settextcolor(hv ? th->menuTextHover : th->dim);
            int w = strWidth(it.hot);
            drawStr(ir.x2 - 10 - w, ir.y1 + 4, it.hot);
        }
    }
}

void MainWindow::drawToolbar()
{
    Rect t = rTool();
    fillRect(t, th->menuBg);
    setlinecolor(th->border);
    line(0, t.y2, m_w, t.y2);

    settextstyle(13, 0, _T("Microsoft YaHei"));
    int x = 8;
    for (int i = 0; i < (int)m_tools.size(); i++)
    {
        if (m_tools[i].cmd == CMD_NONE)
        {
            setlinecolor(th->border);
            line(x + 6, t.y1 + 8, x + 6, t.y2 - 8);
            x += 14;
            continue;
        }
        Rect b = toolBtnRect(i);
        bool hv = b.hit(m_mouseX, m_mouseY);
        fillRoundRect(b, 5, hv ? th->btnHover : th->btn);
        setlinecolor(th->btnBorder);
        // 用直线近似边框
        line(b.x1 + 5, b.y1, b.x2 - 5, b.y1);
        line(b.x1 + 5, b.y2, b.x2 - 5, b.y2);
        settextcolor(th->btnText);
        drawStrCenter(b, m_tools[i].label);
        x = b.x2 + 6;
    }
}

void MainWindow::drawEditor()
{
    CoreApi& api = CoreApi::inst();
    Rect e = rEdit(), t = rText(), g = rGutter();

    fillRectB(e, th->panel, th->border);
    fillRect(g, th->gutterBg);
    setlinecolor(th->border);
    line(g.x2, e.y1, g.x2, e.y2);

    int total = api.bufLineCount();
    int vis   = visibleLines();
    int crow  = api.bufRow();

    // 当前行底色
    if (crow >= m_topLine && crow < m_topLine + vis)
    {
        int y = t.y1 + (crow - m_topLine) * UI_LINE_H;
        fillRect(Rect(g.x1, y, g.x2, y + UI_LINE_H), mixColor(th->gutterBg, th->accent, 0.10));
        fillRect(Rect(t.x1, y, t.x2, y + UI_LINE_H), th->curLine);
    }

    {
        // 裁剪区域要覆盖"行号栏 + 文本区"，否则行号会被裁掉
        Rect clipAll(e.x1, t.y1, e.x2, t.y2);
        ClipGuard cg(clipAll);
        setbkmode(TRANSPARENT);

        // 计算进入可见区首行时是否处于块注释中（带缓存：顶行变化或每 15 帧重算一次）
        bool inBlock;
        if (m_blockTopCache == m_topLine && (m_tick - m_blockFrameCache) < 15)
        {
            inBlock = m_blockValCache;
        }
        else
        {
            std::vector<Tok> dummy;
            bool tmp = false;
            inBlock = false;
            for (int i = 0; i < m_topLine; i++)
            {
                std::string s = api.bufGetLine(i);
                dummy.clear();
                tokenizeLine(s, inBlock, dummy, tmp);
                inBlock = tmp;
            }
            m_blockTopCache  = m_topLine;
            m_blockValCache  = inBlock;
            m_blockFrameCache = m_tick;
        }

        for (int i = m_topLine; i < total && i < m_topLine + vis; i++)
        {
            int y = t.y1 + (i - m_topLine) * UI_LINE_H;

            // 行号
            settextcolor(i == crow ? th->text : th->gutterText);
            settextstyle(12, 0, _T("Consolas"));
            std::string num = itos(i + 1);
            int nw = strWidth(num);
            drawStr(g.x2 - 10 - nw, y + 4, num);

            // 有诊断的行画一个标记点
            for (size_t k = 0; k < m_diagnostics.size(); k++)
            {
                if (m_diagnostics[k].line == i)
                {
                    setfillcolor(m_diagnostics[k].level == DIAG_ERROR ? th->err : th->warn);
                    solidcircle(g.x1 + 8, y + UI_LINE_H / 2, 3);
                    break;
                }
            }

            // 代码
            settextstyle(UI_FONT_H, 0, _T("Consolas"));
            drawCodeLine(t.x1 + 6, y, i, t, inBlock);
            // 把块注释状态传递到下一行
            {
                std::vector<Tok> d2; bool ob = false;
                tokenizeLine(api.bufGetLine(i), inBlock, d2, ob);
                inBlock = ob;
            }
        }

        // 光标
        if (m_focus == FOCUS_EDITOR && (m_tick % 30) < 18)
        {
            std::string L = api.bufGetLine(crow);
            int dcol = dispColOfIndex(L, api.bufCol());
            int cx = t.x1 + 6 + (dcol - m_leftCol) * m_charW;
            int cy = t.y1 + (crow - m_topLine) * UI_LINE_H + 2;
            setlinecolor(th->text);
            line(cx, cy, cx, cy + UI_LINE_H - 4);
        }
    }

    // 滚动条
    drawScrollbar(rVScroll(), m_topLine, total, vis, true);
    // 水平滚动：以最长行为为准（与命中测试/拖动共用同一套 total/page）
    drawScrollbar(rHScroll(), m_leftCol, maxLineCols(), hPageCols(), false);

    // 编译中 / 运行中的遮罩提示
    if (m_compiling)
    {
        settextcolor(th->accent);
        settextstyle(15, 0, _T("Microsoft YaHei"));
        drawStr(t.x1 + 12, t.y1 + 8, "正在编译，请稍候...");
    }
}

void MainWindow::drawCodeLine(int x, int y, int row, const Rect& clip, bool inBlock)
{
    CoreApi& api = CoreApi::inst();
    std::string s = api.bufGetLine(row);
    int n = (int)s.size();

    // ---- 本行的选区范围 ----
    int sc1 = -1, sc2 = -1;
    if (m_selActive && row >= m_selR1 && row <= m_selR2)
    {
        sc1 = (row == m_selR1) ? m_selC1 : 0;
        sc2 = (row == m_selR2) ? m_selC2 : n;
        if (sc1 > n) sc1 = n;
        if (sc2 > n) sc2 = n;
    }

    // ---- 选区底色 ----
    if (sc1 >= 0 && sc2 >= sc1)
    {
        int a = dispColOfIndex(s, sc1);
        int b = dispColOfIndex(s, sc2);
        int xa = x + (a - m_leftCol) * m_charW;
        int xb = x + (b - m_leftCol) * m_charW;
        if (xb <= xa) xb = xa + 3;
        if (xb > clip.x2) xb = clip.x2;
        if (xa < clip.x1) xa = clip.x1;
        if (xb > xa) fillRect(Rect(xa, y, xb, y + UI_LINE_H), th->sel);
    }

    // ---- 查找命中高亮 ----
    if (m_hasMatch && m_matchR == row && !m_findText.empty())
    {
        int a = dispColOfIndex(s, clampi(m_matchC, 0, n));
        int b = a + (int)m_findText.size();
        int xa = x + (a - m_leftCol) * m_charW;
        int xb = x + (b - m_leftCol) * m_charW;
        if (xa < clip.x1) xa = clip.x1;
        if (xb > clip.x2) xb = clip.x2;
        if (xb > xa)
        {
            setlinecolor(th->warn);
            rectangle(xa, y + 1, xb, y + UI_LINE_H - 1);
        }
    }

    // ---- 词法着色 ----
    std::vector<Tok> toks;
    bool outBlock = false;
    tokenizeLine(s, inBlock, toks, outBlock);

    for (size_t k = 0; k < toks.size(); k++)
    {
        const Tok& tk = toks[k];
        COLORREF base = tokColor(*th, tk.t);

        // 把 token 按选区切成 3 段：选区前 / 选区内 / 选区后
        int segs[4][2] = { { tk.b0, tk.b1 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
        int segCount = 1;
        if (sc1 >= 0 && sc1 < tk.b1 && sc2 > tk.b0)
        {
            int p1 = (sc1 > tk.b0) ? sc1 : tk.b0;
            int p2 = (sc2 < tk.b1) ? sc2 : tk.b1;
            segs[0][0] = tk.b0;  segs[0][1] = p1;
            segs[1][0] = p1;     segs[1][1] = p2;
            segs[2][0] = p2;     segs[2][1] = tk.b1;
            segCount = 3;
        }
        for (int q = 0; q < segCount; q++)
        {
            int a = segs[q][0], b = segs[q][1];
            if (b <= a) continue;
            COLORREF col = (q == 1) ? th->selText : base;

            // 水平裁剪：完全在可视区外则跳过（其余交给 ClipGuard 裁掉）
            int ca = dispColOfIndex(s, a) - m_leftCol;
            int cb = dispColOfIndex(s, b) - m_leftCol;
            if (x + cb * m_charW < clip.x1) continue;
            if (x + ca * m_charW > clip.x2) continue;

            settextcolor(col);
            outtextxy(x + ca * m_charW, y + (UI_LINE_H - UI_FONT_H) / 2,
                      ts(s.substr(a, b - a)).c_str());
        }
    }
}

void MainWindow::drawFindBar()
{
    FindLayout L = findLayout();
    Rect bar = L.bar;
    fillRectB(bar, mixColor(th->panel, th->accent, 0.06), th->border);
    setlinecolor(th->border);
    line(bar.x1, bar.y2, bar.x2, bar.y2);

    settextstyle(14, 0, _T("Microsoft YaHei"));

    // 输入框
    fillRectB(L.fInput, th->panel, (m_focus == FOCUS_FIND) ? th->accent : th->border);
    fillRectB(L.rInput, th->panel, (m_focus == FOCUS_REPLACE) ? th->accent : th->border);
    settextcolor(th->text);
    drawStr(L.fInput.x1 + 6, L.fInput.y1 + 3, m_findText);
    drawStr(L.rInput.x1 + 6, L.rInput.y1 + 3, m_replaceText);
    // 输入光标
    if ((m_tick % 30) < 18)
    {
        if (m_focus == FOCUS_FIND)
        {
            int cx = L.fInput.x1 + 6 + strWidth(m_findText);
            setlinecolor(th->text); line(cx, L.fInput.y1 + 3, cx, L.fInput.y2 - 3);
        }
        else if (m_focus == FOCUS_REPLACE)
        {
            int cx = L.rInput.x1 + 6 + strWidth(m_replaceText);
            setlinecolor(th->text); line(cx, L.rInput.y1 + 3, cx, L.rInput.y2 - 3);
        }
    }

    // 标签
    settextcolor(th->dim);
    drawStr(L.fInput.x1 - 46, L.fInput.y1 + 3, "查找:");
    drawStr(L.rInput.x1 - 46, L.rInput.y1 + 3, "替换:");

    // 复选框
    fillRectB(L.chk, th->panel, th->border);
    if (m_caseSensitive)
    {
        setlinecolor(th->accent);
        line(L.chk.x1 + 3, L.chk.y1 + 8, L.chk.x1 + 7, L.chk.y2 - 5);
        line(L.chk.x1 + 7, L.chk.y2 - 5, L.chk.x2 - 3, L.chk.y1 + 3);
    }
    settextcolor(th->text);
    drawStr(L.chk.x2 + 6, L.chk.y1, "区分大小写");

    // 按钮
    struct B { Rect r; const char* t; };
    B bs[] = { { L.bPrev, "<" }, { L.bNext, ">" }, { L.bRep, "替换" },
               { L.bRepAll, "全部替换" }, { L.bClose, "X" } };
    for (int i = 0; i < 5; i++)
    {
        bool hv = bs[i].r.hit(m_mouseX, m_mouseY);
        fillRoundRect(bs[i].r, 4, hv ? th->btnHover : th->btn);
        settextcolor(th->btnText);
        drawStrCenter(bs[i].r, bs[i].t);
    }
}

void MainWindow::drawBottom()
{
    Rect b = rBottom();
    fillRectB(b, th->panel, th->border);

    // ---- 标签页 ----
    Rect tb = rBottomTabs();
    fillRect(tb, mixColor(th->panel, th->bg, 0.45));
    setlinecolor(th->border);
    line(tb.x1, tb.y2, tb.x2, tb.y2);

    settextstyle(13, 0, _T("Microsoft YaHei"));

    int x = tb.x1 + 8;
    for (int i = 0; i < 2; i++)
    {
        std::string label = tabLabel(i);
        int w = strWidth(label) + 26;
        Rect t(x, tb.y1 + 3, x + w, tb.y2);
        bool act = (m_bottomTab == i);
        if (act) fillRoundRect(Rect(t.x1, t.y1, t.x2, t.y2 + 2), 4, th->panel);
        settextcolor(act ? th->text : th->dim);
        drawStr(t.x1 + 12, t.y1 + 5, label);
        if (act)
        {
            setfillcolor(th->accent);
            solidrectangle(t.x1, tb.y2 - 2, t.x2, tb.y2);
        }
        x += w + 4;
    }

    // ---- 内容 ----
    if (m_bottomTab == 0) drawDiagnostics();
    else                  drawConsole();

    // ---- 滚动条 ----
    int total = (m_bottomTab == 0) ? (int)m_diagnostics.size() : (int)m_console.size();
    int page  = (m_bottomTab == 0) ? (rBottomBody().h() / 22) : (rConsoleBody().h() / 18);
    int top   = (m_bottomTab == 0) ? m_diagTop : m_consoleTop;
    drawScrollbar(rBottomScroll(), top, total, page, true);
}

void MainWindow::drawDiagnostics()
{
    Rect body = rBottomBody();
    setbkmode(TRANSPARENT);
    if (m_diagnostics.empty())
    {
        settextcolor(th->dim);
        settextstyle(14, 0, _T("Microsoft YaHei"));
        drawStr(body.x1 + 14, body.y1 + 12, "暂无诊断信息。按 F7 编译后，错误与警告会显示在这里，点击条目可跳转到源码对应行。");
        return;
    }

    ClipGuard cg(body);
    settextstyle(13, 0, _T("Microsoft YaHei"));
    for (int i = m_diagTop; i < (int)m_diagnostics.size(); i++)
    {
        int y = body.y1 + 4 + (i - m_diagTop) * 22;
        if (y + 22 > body.y2) break;
        const Diagnostic& d = m_diagnostics[i];
        Rect row(body.x1 + 4, y, body.x2 - 8, y + 20);
        if (row.hit(m_mouseX, m_mouseY)) fillRect(row, mixColor(th->panel, th->accent, 0.10));

        COLORREF c = (d.level == DIAG_ERROR) ? th->err : ((d.level == DIAG_WARNING) ? th->warn : th->dim);
        setfillcolor(c);
        solidcircle(body.x1 + 16, y + 10, 4);

        settextcolor(c);
        std::string tag = (d.level == DIAG_ERROR) ? "错误" : ((d.level == DIAG_WARNING) ? "警告" : "提示");
        drawStr(body.x1 + 28, y + 3, tag);

        settextcolor(th->dim);
        drawStr(body.x1 + 72, y + 3, "行 " + itos(d.line + 1) + "  列 " + itos(d.column + 1));

        settextcolor(th->text);
        drawStr(body.x1 + 160, y + 3, d.message);
    }
}

void MainWindow::drawConsole()
{
    Rect body = rConsoleBody();
    fillRect(Rect(body.x1, body.y1, body.x2, body.y2), th->outBg);

    int lh = 18;

    {
        ClipGuard cg(body);
        setbkmode(TRANSPARENT);
        settextstyle(14, 0, _T("Consolas"));
        for (int i = m_consoleTop; i < (int)m_console.size(); i++)
        {
            int y = body.y1 + (i - m_consoleTop) * lh;
            if (y + lh > body.y2) break;
            settextcolor(th->outText);
            drawStr(body.x1 + 8, y + 1, m_console[i]);
        }
    }

    // ---- 输入行 ----
    Rect in = rConsoleInput();
    fillRect(in, mixColor(th->outBg, th->accent, 0.10));
    settextstyle(14, 0, _T("Consolas"));
    settextcolor(th->outPrompt);
    drawStr(in.x1 + 6, in.y1 + 4, ">");
    settextcolor(th->outText);
    std::string shown = m_inputLine;
    drawStr(in.x1 + 20, in.y1 + 4, shown);
    if ((m_tick % 30) < 18)
    {
        int cx = in.x1 + 20 + strWidth(shown);
        setlinecolor(th->outText);
        line(cx, in.y1 + 4, cx, in.y2 - 4);
    }
    if (m_progRunning)
    {
        settextcolor(th->warn);
        settextstyle(13, 0, _T("Microsoft YaHei"));
        std::string s = "  程序运行中... 在此输入后回车可发送给程序";
        int w = strWidth(s);
        drawStr(in.x2 - w - 10, in.y1 + 4, s);
    }
}

void MainWindow::drawStatusBar()
{
    Rect r = rStatus();
    fillRect(r, th->statusBg);
    setlinecolor(th->border);
    line(0, r.y1, m_w, r.y1);

    CoreApi& api = CoreApi::inst();
    settextstyle(13, 0, _T("Microsoft YaHei"));
    setbkmode(TRANSPARENT);

    // 左：文件路径 + 脏标记
    std::string path = api.bufPath();
    if (path.empty()) path = "未命名.c";
    std::string left = path + (api.bufDirty() ? "  *" : "");
    settextcolor(th->statusText);
    drawStr(10, r.y1 + 5, left);

    // 中：行列 / 选区
    int x = 10 + strWidth(left) + 26;
    settextcolor(th->dim);
    std::string pos = "行 " + itos(api.bufRow() + 1) + "，列 " + itos(api.bufCol() + 1);
    drawStr(x, r.y1 + 5, pos);
    x += strWidth(pos) + 26;
    if (m_selActive)
    {
        int cnt = 0;
        for (int i = m_selR1; i <= m_selR2; i++) cnt += (int)api.bufGetLine(i).size() + 1;
        std::string s = "已选 " + itos(cnt) + " 字符";
        drawStr(x, r.y1 + 5, s);
    }

    // 右：状态消息 + 编译状态 + 主题 + 版本
    std::string right = std::string("主题:") + th->name + "   |   " + MINIC_VERSION;
    int rw = strWidth(right);
    settextcolor(th->dim);
    drawStr(m_w - 10 - rw, r.y1 + 5, right);

    std::string cs;
    COLORREF cc = th->dim;
    switch (m_compileState)
    {
    case CS_OK:         cs = "编译成功"; cc = th->ok;   break;
    case CS_WARNING:    cs = "编译成功(有警告)"; cc = th->warn; break;
    case CS_ERROR:      cs = "编译失败"; cc = th->err;  break;
    case CS_NOCOMPILER: cs = "未找到编译器"; cc = th->err; break;
    case CS_TIMEOUT:    cs = "编译超时"; cc = th->err;  break;
    default:            cs = "未编译";  cc = th->dim;  break;
    }
    int cw = strWidth(cs);
    settextcolor(cc);
    drawStr(m_w - 20 - rw - cw, r.y1 + 5, cs);

    int mw = strWidth(m_statusMsg);
    settextcolor(m_statusColor);
    drawStr(m_w - 30 - rw - cw - mw, r.y1 + 5, m_statusMsg);
}

void MainWindow::drawScrollbar(const Rect& r, int pos, int total, int page, bool vertical)
{
    if (total <= page || page <= 0)
    {
        fillRect(r, mixColor(th->panel, th->bg, 0.4));
        return;
    }
    fillRect(r, mixColor(th->panel, th->bg, 0.4));

    int track = vertical ? r.h() : r.w();
    int kh = (int)((double)track * page / total);
    if (kh < 24) kh = 24;
    int maxp = track - kh;
    int p = (total - page > 0) ? (int)((double)pos * maxp / (total - page)) : 0;
    p = clampi(p, 0, maxp);

    Rect knob = vertical ? Rect(r.x1 + 2, r.y1 + p + 1, r.x2 - 2, r.y1 + p + kh - 1)
                         : Rect(r.x1 + p + 1, r.y1 + 2, r.x1 + p + kh - 1, r.y2 - 2);
    bool hv = knob.hit(m_mouseX, m_mouseY) || (m_dragV && vertical) || (m_dragH && !vertical);
    fillRoundRect(knob, 4, hv ? th->dim : mixColor(th->dim, th->panel, 0.45));
}

// ============================================================================
//  命令分发
// ============================================================================
void MainWindow::execCmd(int cmd)
{
    CoreApi& api = CoreApi::inst();
    switch (cmd)
    {
    case CMD_FILE_NEW:
        if (!confirmDiscard()) return;
        api.fileNew();
        m_topLine = 0; m_leftCol = 0;
        clearSelection();
        m_diagnostics.clear();
        m_compileState = CS_NONE;
        setStatus("已新建文档");
        break;
    case CMD_FILE_OPEN:
        if (!confirmDiscard()) return;
        doOpen();
        break;
    case CMD_FILE_SAVE:
        doSave();
        break;
    case CMD_FILE_SAVEAS:
        doSaveAs();
        break;
    case CMD_FILE_CLOSE:
        doCloseDoc();
        break;
    case CMD_EXIT:
        doExit();
        break;

    case CMD_EDIT_UNDO:
        api.bufUndo(); clearSelection(); scrollToCursor();
        setStatus("已撤销");
        break;
    case CMD_EDIT_REDO:
        api.bufRedo(); clearSelection(); scrollToCursor();
        setStatus("已重做");
        break;
    case CMD_EDIT_CUT:
        if (m_selActive) { setClipboardText(selectedText()); deleteSelection(); scrollToCursor(); }
        break;
    case CMD_EDIT_COPY:
        if (m_selActive) { setClipboardText(selectedText()); setStatus("已复制到剪贴板"); }
        break;
    case CMD_EDIT_PASTE:
    {
        std::string txt = getClipboardText();
        if (!txt.empty()) { insertTextAtCursor(txt); scrollToCursor(); setStatus("已粘贴"); }
        break;
    }
    case CMD_EDIT_SELALL:
    {
        int total = api.bufLineCount();
        m_selActive = true;
        m_selR1 = 0; m_selC1 = 0;
        m_selR2 = total - 1; m_selC2 = (int)api.bufGetLine(total - 1).size();
        api.bufSetCursor(m_selR2, m_selC2);
        break;
    }

    case CMD_FIND:
        m_findVisible = true;
        m_focus = FOCUS_FIND;
        // 若已有选区，直接把选区内容带入查找框
        if (m_selActive)
        {
            std::string s = selectedText();
            if (!s.empty() && s.find('\n') == std::string::npos) m_findText = s;
        }
        break;
    case CMD_REPLACE:
        m_findVisible = true;
        m_focus = FOCUS_REPLACE;
        break;
    case CMD_FIND_NEXT:   searchNext(true);  break;
    case CMD_FIND_PREV:   searchNext(false); break;
    case CMD_REPLACE_ONE: replaceOne(); break;
    case CMD_REPLACE_ALL: replaceAll(); break;

    case CMD_COMPILE:      doCompile(); break;
    case CMD_RUN:          doRun();     break;
    case CMD_COMPILE_RUN:  doCompile(); if (m_compileState == CS_OK || m_compileState == CS_WARNING) doRun(); break;
    case CMD_STOP:         doStop();    break;

    case CMD_THEME:
        m_dark = !m_dark;
        th = m_dark ? &themeDark() : &themeLight();
        setbkcolor(th->bg);
        setStatus("已切换到" + std::string(th->name) + "主题");
        break;

    case CMD_ABOUT:
        uiAlert(m_w, m_h, *th, "关于 Mini-C Studio",
                std::string("Mini-C Studio ") + MINIC_VERSION +
                "\n\n轻量级 C 语言集成开发环境\n" + MINIC_ORG + "\n\n"
                "模块分工：\n"
                "  A  组长 / 架构与集成\n"
                "  B  自研文本缓冲区\n"
                "  C  GUI 界面与交互控制\n"
                "  D  编译调度与运行时托管\n"
                "  E  文件生命周期管理\n\n"
                "快捷键：Ctrl+N/O/S  F7 编译  F5 运行  Ctrl+F 查找  Ctrl+T 换主题");
        break;
    default:
        break;
    }
}

// ============================================================================
//  文件操作
// ============================================================================
bool MainWindow::confirmDiscard()
{
    if (!CoreApi::inst().bufDirty()) return true;
    DlgRet r = uiMessageBox(m_w, m_h, *th, "未保存的更改",
                            "当前文档有未保存的修改。\n是否在关闭前保存？", DLG_YESNOCANCEL);
    if (r == RET_CANCEL) return false;
    if (r == RET_YES)    return doSave();
    return true;
}

bool MainWindow::doSaveTo(const std::string& path)
{
    if (path.empty()) return false;
    if (!CoreApi::inst().fileSave(path))
    {
        // 修复 T6.8：保存失败时也立刻把状态栏置红，再弹模态框
        setStatus("保存失败", th->err);
        uiAlert(m_w, m_h, *th, "保存失败",
                "无法写入文件：\n" + path + "\n\n请检查：路径是否存在、是否有写权限、磁盘是否已满。");
        return false;
    }
    setStatus("已保存：" + path, th->ok);
    return true;
}

bool MainWindow::doSave()
{
    std::string path = CoreApi::inst().bufPath();
    if (path.empty()) return doSaveAs();
    return doSaveTo(path);
}

bool MainWindow::doSaveAs()
{
    std::string path = CoreApi::inst().bufPath();
    if (!uiPickPath(m_w, m_h, *th, "另存为", path, true)) return false;
    if (path.empty()) return false;
    // 没写扩展名则补 .c
    if (path.find('.') == std::string::npos) path += ".c";
    return doSaveTo(path);
}

void MainWindow::doOpen()
{
    std::string path = CoreApi::inst().bufPath();
    if (!uiPickPath(m_w, m_h, *th, "打开 C 源文件", path, false)) return;
    if (path.empty()) return;

    if (!CoreApi::inst().fileOpen(path))
    {
        // 修复 T6.8：先把状态栏置红，再弹模态框；弹窗出现的同时状态栏已更新，
        // 不用等用户点确定才看到"打开失败"。
        setStatus("打开失败", th->err);
        uiAlert(m_w, m_h, *th, "打开失败",
                "无法打开文件：\n" + path + "\n\n请检查：文件是否存在、路径是否正确、是否有读权限。");
        return;
    }
    m_topLine = 0; m_leftCol = 0;
    clearSelection();
    m_diagnostics.clear();
    m_compileState = CS_NONE;
    setStatus("已打开：" + path, th->ok);
}

void MainWindow::doCloseDoc()
{
    if (!confirmDiscard()) return;
    CoreApi::inst().fileNew();
    m_topLine = 0; m_leftCol = 0;
    clearSelection();
    m_diagnostics.clear();
    m_compileState = CS_NONE;
    setStatus("文档已关闭");
}

void MainWindow::doExit()
{
    if (!confirmDiscard()) return;
    m_running = false;
}

// ============================================================================
//  编译 / 运行
// ============================================================================
void MainWindow::doCompile()
{
    CoreApi& api = CoreApi::inst();

    // 1) 确保磁盘上是最新代码
    std::string path = api.bufPath();
    if (path.empty())
    {
        if (!doSaveAs()) return;
        path = api.bufPath();
        if (path.empty()) return;
    }
    else if (api.bufDirty())
    {
        if (!doSave()) return;
    }

    // 2) 源码为空检查
    std::string src = api.bufSave();
    if (src.find_first_not_of(" \t\r\n") == std::string::npos)
    {
        uiAlert(m_w, m_h, *th, "无法编译", "源码为空，请先编写 C 代码再编译。");
        m_compileState = CS_INTERNAL;
        setStatus("源码为空", th->err);
        return;
    }

    // 3) 编译器可用性检查
    if (!api.compilerAvailable())
    {
        m_compileState = CS_NOCOMPILER;
        uiAlert(m_w, m_h, *th, "未找到编译器",
                "没有检测到 gcc 编译器。\n\n"
                "请安装 MinGW-w64 或 TDM-GCC，\n"
                "并把 gcc.exe 所在目录加入系统 PATH，然后重启本程序。");
        setStatus("未找到编译器（gcc）", th->err);
        return;
    }

    // 4) 编译（占位实现是同步的，会短暂卡一下；D 同学改成异步后可去掉）
    m_compiling = true;
    setStatus("正在编译...", th->accent);
    render();

    CompileResult res = api.compile(path);
    m_compiling = false;

    m_diagnostics = res.items;
    m_compileState = res.state;
    m_lastExe = res.exePath;
    m_diagTop = 0;
    m_bottomTab = 0;

    appendConsole("> gcc \"" + path + "\" -o \"" + res.exePath + "\" -Wall -std=c11\n");
    appendConsole(res.raw);

    switch (res.state)
    {
    case CS_OK:      setStatus("编译成功", th->ok);   break;
    case CS_WARNING: setStatus("编译成功，有 " + itos((int)res.items.size()) + " 条警告", th->warn); break;
    case CS_ERROR:   setStatus("编译失败，共 " + itos((int)res.items.size()) + " 个错误", th->err); break;
    case CS_NOCOMPILER: setStatus("未找到编译器", th->err); break;
    case CS_TIMEOUT:    setStatus("编译超时（>30s）", th->err);
        uiAlert(m_w, m_h, *th, "编译超时", "编译过程超过 30 秒仍未结束，已终止。请检查代码中是否存在死循环或超大文件。");
        break;
    default:         setStatus("编译未完成", th->err); break;
    }
}

void MainWindow::doRun()
{
    CoreApi& api = CoreApi::inst();

    if (m_compileState != CS_OK && m_compileState != CS_WARNING)
    {
        doCompile();
        if (m_compileState != CS_OK && m_compileState != CS_WARNING) return;
    }
    if (m_lastExe.empty()) m_lastExe = api.exePathOf(api.bufPath());
    if (m_lastExe.empty()) return;

    if (GetFileAttributesA(m_lastExe.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        uiAlert(m_w, m_h, *th, "无法运行",
                "找不到可执行文件：\n" + m_lastExe + "\n请先按 F7 成功编译一次。");
        setStatus("可执行文件不存在", th->err);
        return;
    }

    m_bottomTab = 1;
    m_focus = FOCUS_CONSOLE;
    m_console.clear();
    m_consoleTop = 0;
    m_inputLine.clear();
    appendConsole("> \"" + m_lastExe + "\"");

    m_progRunning = true;
    setStatus("程序正在运行...", th->warn);
    api.runStart(m_lastExe);
}

void MainWindow::doStop()
{
    if (!m_progRunning) { setStatus("当前没有正在运行的程序"); return; }
    CoreApi::inst().runStop();
    m_progRunning = false;
    appendConsole("\n[已被用户终止]");
    setStatus("已终止运行", th->warn);
}

// ============================================================================
//  编辑辅助
// ============================================================================
void MainWindow::clearSelection()
{
    m_selActive = false;
    m_selR1 = m_selC1 = m_selR2 = m_selC2 = 0;
}

void MainWindow::normalizeSel()
{
    if (m_selR1 > m_selR2 || (m_selR1 == m_selR2 && m_selC1 > m_selC2))
    {
        int tr = m_selR1, tc = m_selC1;
        m_selR1 = m_selR2; m_selC1 = m_selC2;
        m_selR2 = tr;      m_selC2 = tc;
    }
    int last = CoreApi::inst().bufLineCount() - 1;
    if (m_selR2 > last) { m_selR2 = last; m_selC2 = (int)CoreApi::inst().bufGetLine(last).size(); }
    if (m_selR1 < 0) { m_selR1 = 0; m_selC1 = 0; }
    if (m_selR1 == m_selR2 && m_selC1 == m_selC2) m_selActive = false;
}

bool MainWindow::hasSelection() const { return m_selActive; }

std::string MainWindow::selectedText() const
{
    if (!m_selActive) return "";
    return CoreApi::inst().bufGetRange(m_selR1, m_selC1, m_selR2, m_selC2);
}

void MainWindow::deleteSelection()
{
    if (!m_selActive) return;
    CoreApi& api = CoreApi::inst();
    api.bufSetCursor(m_selR2, m_selC2);
    api.bufDeleteRange(m_selR1, m_selC1, m_selR2, m_selC2);
    api.bufSetCursor(m_selR1, m_selC1);
    clearSelection();
}

void MainWindow::insertTextAtCursor(const std::string& s)
{
    if (s.empty()) return;
    if (m_selActive) deleteSelection();
    CoreApi::inst().bufInsertString(s);
}

void MainWindow::screenToDoc(int x, int y, int& row, int& col)
{
    Rect t = rText();
    CoreApi& api = CoreApi::inst();
    int total = api.bufLineCount();

    int r = m_topLine + (y - t.y1) / UI_LINE_H;
    r = clampi(r, 0, total - 1);

    std::string L = api.bufGetLine(r);
    int dcol = m_leftCol;
    if (m_charW > 0)
        dcol += (int)floor((double)(x - t.x1 - 6) / m_charW + 0.5);
    if (dcol < 0) dcol = 0;

    col = indexOfDispCol(L, dcol);
    row = r;
}

void MainWindow::scrollToCursor()
{
    CoreApi& api = CoreApi::inst();
    int total = api.bufLineCount();
    int vis = visibleLines();
    int r = api.bufRow();

    if (r < m_topLine)                m_topLine = r;
    else if (r > m_topLine + vis - 1) m_topLine = r - vis + 1;
    m_topLine = clampi(m_topLine, 0, (total > vis ? total - vis : 0));

    std::string L = api.bufGetLine(r);
    int dcol = dispColOfIndex(L, api.bufCol());
    int page = (m_charW > 0) ? (rText().w() / m_charW) : 80;
    if (dcol < m_leftCol)                m_leftCol = dcol;
    else if (dcol > m_leftCol + page - 2) m_leftCol = dcol - page + 2;
    if (m_leftCol < 0) m_leftCol = 0;
}

void MainWindow::setStatus(const std::string& msg, COLORREF c)
{
    m_statusMsg = msg;
    m_statusColor = c;
}

void MainWindow::setStatus(const std::string& msg)
{
    m_statusMsg = msg;
    m_statusColor = th->dim;
}

void MainWindow::appendConsole(const std::string& text)
{
    std::string cur;
    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] == '\n') { m_console.push_back(cur); cur.clear(); }
        else if (text[i] != '\r') cur += text[i];
    }
    if (!cur.empty()) m_console.push_back(cur);

    if ((int)m_console.size() > 3000)
        m_console.erase(m_console.begin(), m_console.begin() + 500);

    // 自动滚到底部
    int page = rConsoleBody().h() / 18;
    m_consoleTop = ((int)m_console.size() > page) ? (int)m_console.size() - page : 0;
    if (m_consoleTop < 0) m_consoleTop = 0;
}

// ============================================================================
//  查找 / 替换
// ============================================================================
bool MainWindow::searchNext(bool forward)
{
    CoreApi& api = CoreApi::inst();
    if (m_findText.empty()) { setStatus("请输入查找内容"); return false; }

    int total = api.bufLineCount();
    if (total <= 0) return false;

    int r = api.bufRow();
    int c = api.bufCol();

    for (int step = 0; step <= total; step++)
    {
        std::string L = api.bufGetLine(r);
        int pos = forward ? findInLine(L, m_findText, forward ? c : c - 1, m_caseSensitive)
                          : rfindInLine(L, m_findText, c - 1, m_caseSensitive);
        if (pos >= 0)
        {
            api.bufSetCursor(r, pos + (int)m_findText.size());
            m_selActive = true;
            m_selR1 = r;  m_selC1 = pos;
            m_selR2 = r;  m_selC2 = pos + (int)m_findText.size();
            m_hasMatch = true;
            m_matchR = r; m_matchC = pos; m_matchLen = (int)m_findText.size();
            scrollToCursor();
            setStatus("已找到匹配：行 " + itos(r + 1) + " 列 " + itos(pos + 1), th->ok);
            return true;
        }
        if (forward)
        {
            r++; if (r >= total) r = 0;
            c = 0;
        }
        else
        {
            r--; if (r < 0) r = total - 1;
            c = (int)api.bufGetLine(r).size();
        }
    }

    m_hasMatch = false;
    clearSelection();
    setStatus("未找到匹配项：" + m_findText, th->warn);
    return false;
}

void MainWindow::replaceOne()
{
    if (m_findText.empty()) { setStatus("请输入查找内容"); return; }
    CoreApi& api = CoreApi::inst();

    // 若当前选区正好是匹配项，直接替换
    if (m_hasMatch && m_selActive && selectedText() == m_findText)
    {
        api.bufSetCursor(m_selR2, m_selC2);
        api.bufDeleteRange(m_selR1, m_selC1, m_selR2, m_selC2);
        api.bufSetCursor(m_selR1, m_selC1);
        api.bufInsertString(m_replaceText);
        clearSelection();
        setStatus("已替换 1 处", th->ok);
    }
    searchNext(true);
}

void MainWindow::replaceAll()
{
    if (m_findText.empty()) { setStatus("请输入查找内容"); return; }
    CoreApi& api = CoreApi::inst();

    int count = 0;
    api.bufSetCursor(0, 0);
    int total = api.bufLineCount();
    for (int r = 0; r < total; r++)
    {
        std::string L = api.bufGetLine(r);
        int from = 0, pos;
        while ((pos = findInLine(L, m_findText, from, m_caseSensitive)) >= 0)
        {
            api.bufDeleteRange(r, pos, r, pos + (int)m_findText.size());
            api.bufSetCursor(r, pos);
            api.bufInsertString(m_replaceText);
            L = api.bufGetLine(r);
            from = pos + (int)m_replaceText.size();
            count++;
            if (from > (int)L.size()) break;
        }
    }
    clearSelection();
    setStatus("共替换 " + itos(count) + " 处", th->ok);
    appendConsole("[替换] 共替换 " + itos(count) + " 处。");
}

// ============================================================================
//  剪贴板
// ============================================================================
void MainWindow::setClipboardText(const std::string& s)
{
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
        char* p = (char*)GlobalLock(h);
        if (p) out = p;
        GlobalUnlock(h);
    }
    CloseClipboard();
    return out;
}
