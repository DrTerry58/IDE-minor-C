// ============================================================================
// 文件名：MiniCConfig.h
// 职责：全局配置 / 主题配色 / 通用绘制与字符串工具
// 负责人：C（GUI 界面 + 交互控制）
// 环境：Visual Studio + EasyX，C++11
// 说明：本文件被所有 GUI 层文件包含，是 C 同学的"设计系统"入口。
// ============================================================================

#pragma once
#ifndef MINIC_CONFIG_H
#define MINIC_CONFIG_H

#include <graphics.h>   // EasyX
#include <windows.h>
#include <string>
#include <vector>

// ---------------- 应用信息 ----------------
#define MINIC_TITLE      "Mini-C Studio"
#define MINIC_VERSION    "v0.1.0"
#define MINIC_ORG        "北京理工大学 · ECE 计算机编程实习"
#define MINIC_SPLASH_MS  2000        // 欢迎界面显示时长（毫秒）

// ---------------- 默认窗口大小（main.cpp 会按屏幕尺寸自适应缩放） ----------------
#define MINIC_DEF_W     1200
#define MINIC_DEF_H     760

// ---------------- 布局常量（像素） ----------------
#define UI_MENU_H           28       // 菜单栏高度
#define UI_TOOL_H           36       // 工具栏高度
#define UI_STATUS_H         24       // 状态栏高度
#define UI_BOTTOM_H         200      // 底部面板（诊断/控制台）高度
#define UI_GUTTER_W         54       // 行号栏宽度
#define UI_SCROLL_W         14       // 滚动条厚度
#define UI_LINE_H           20       // 编辑器行高
#define UI_FONT_H           16       // 编辑器字号
#define UI_TAB_H            26       // 底部面板标签页高度
#define UI_FINDBAR_H        36       // 查找/替换条高度
#define UI_CONSOLE_INPUT_H  24       // 控制台输入行高度
#define UI_MENU_TITLE_W     64       // 每个菜单标题宽度
#define UI_DROP_ITEM_H      26       // 菜单项高度
#define UI_DROP_W           210      // 菜单下拉宽度

// ============================================================================
//  主题配色（70-20-10 原则：主色大面积 / 次色面板 / 点缀色按钮与高亮）
// ============================================================================
struct Theme
{
    const char* name;

    COLORREF bg;            // 窗口底色
    COLORREF panel;         // 面板底色（编辑区等）
    COLORREF border;        // 分割线 / 边框
    COLORREF text;          // 主文本
    COLORREF dim;           // 次要文本

    COLORREF menuBg;        // 菜单栏底色
    COLORREF menuHover;     // 菜单高亮
    COLORREF menuText;      // 菜单文字
    COLORREF menuTextHover;

    COLORREF btn;           // 按钮底色
    COLORREF btnHover;      // 按钮悬停
    COLORREF btnBorder;     // 按钮边框
    COLORREF btnText;       // 按钮文字

    COLORREF gutterBg;      // 行号栏底色
    COLORREF gutterText;    // 行号文字
    COLORREF curLine;       // 当前行底色

    COLORREF sel;           // 选区底色
    COLORREF selText;       // 选区文字

    COLORREF kw;            // C 关键字
    COLORREF kwType;        // 类型关键字
    COLORREF str;           // 字符串 / 字符常量
    COLORREF comment;       // 注释
    COLORREF num;           // 数字
    COLORREF prep;          // 预处理指令
    COLORREF ident;         // 普通标识符
    COLORREF punct;         // 运算符 / 标点

    COLORREF statusBg;      // 状态栏底色
    COLORREF statusText;    // 状态栏文字

    COLORREF outBg;         // 控制台底色
    COLORREF outText;       // 控制台文字
    COLORREF outPrompt;     // 控制台提示符

    COLORREF err;           // 错误
    COLORREF warn;          // 警告
    COLORREF ok;            // 成功
    COLORREF accent;        // 点缀色
};

inline COLORREF mkRGB(int r, int g, int b) { return RGB(r, g, b); }

inline COLORREF mixColor(COLORREF a, COLORREF b, double t)
{
    if (t < 0) t = 0; if (t > 1) t = 1;
    int r = (int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t);
    int g = (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t);
    int bl = (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t);
    return RGB(r, g, bl);
}

// 亮色主题（默认）
inline const Theme& themeLight()
{
    static Theme t = {
        "亮色",
        mkRGB(243,245,249), mkRGB(255,255,255), mkRGB(206,212,222), mkRGB(32,35,42), mkRGB(122,129,142),
        mkRGB(250,251,253), mkRGB(58,120,255),  mkRGB(45,50,60),   mkRGB(255,255,255),
        mkRGB(233,236,242), mkRGB(214,221,233), mkRGB(186,193,206),mkRGB(45,50,60),
        mkRGB(238,240,245), mkRGB(150,157,170), mkRGB(228,238,255),
        mkRGB(120,170,255), mkRGB(255,255,255),
        mkRGB(0,72,204),    mkRGB(16,124,140),  mkRGB(168,32,32),  mkRGB(0,128,48),
        mkRGB(176,96,0),    mkRGB(128,0,128),   mkRGB(32,35,42),   mkRGB(96,104,120),
        mkRGB(232,235,241), mkRGB(70,77,90),
        mkRGB(26,28,34),    mkRGB(220,224,232), mkRGB(126,208,126),
        mkRGB(198,40,40),   mkRGB(196,132,0),   mkRGB(30,146,80),  mkRGB(58,120,255)
    };
    return t;
}

// 暗色主题
inline const Theme& themeDark()
{
    static Theme t = {
        "暗色",
        mkRGB(30,32,38),  mkRGB(37,40,47),  mkRGB(60,65,76),  mkRGB(220,224,232), mkRGB(140,147,160),
        mkRGB(37,40,47),  mkRGB(70,130,255),mkRGB(215,220,230),mkRGB(255,255,255),
        mkRGB(52,56,66),  mkRGB(68,74,88),  mkRGB(74,80,94),  mkRGB(220,224,232),
        mkRGB(33,36,43),  mkRGB(112,119,132),mkRGB(45,49,59),
        mkRGB(58,100,190),mkRGB(255,255,255),
        mkRGB(126,176,255),mkRGB(88,208,198),mkRGB(226,156,116),mkRGB(118,138,120),
        mkRGB(220,180,120),mkRGB(198,150,255),mkRGB(220,224,232),mkRGB(160,168,182),
        mkRGB(45,48,56),  mkRGB(200,206,216),
        mkRGB(20,22,27),  mkRGB(212,217,226),mkRGB(126,208,126),
        mkRGB(238,110,110),mkRGB(228,180,80),mkRGB(110,208,140),mkRGB(92,142,255)
    };
    return t;
}

// ============================================================================
//  基础几何
// ============================================================================
struct Rect
{
    int x1, y1, x2, y2;
    Rect() : x1(0), y1(0), x2(0), y2(0) {}
    Rect(int a, int b, int c, int d) : x1(a), y1(b), x2(c), y2(d) {}
    int  w() const { return x2 - x1; }
    int  h() const { return y2 - y1; }
    bool hit(int x, int y) const { return x >= x1 && x <= x2 && y >= y1 && y <= y2; }
};

// ============================================================================
//  字符串工具（EasyX 在 Unicode 工程下需要 wchar_t，这里统一封装）
// ============================================================================
inline std::wstring s2w(const std::string& s)
{
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

inline std::string w2s(const std::wstring& w)
{
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}

// std::string -> TCHAR 串
inline std::basic_string<TCHAR> ts(const std::string& s)
{
#ifdef UNICODE
    return s2w(s);
#else
    return s;
#endif
}

inline std::string fms(const std::basic_string<TCHAR>& t)
{
#ifdef UNICODE
    return w2s(t);
#else
    return t;
#endif
}

// 判断 GBK 首字节（用于中文全角字符处理）
inline bool isDbcsLead(unsigned char c) { return c >= 0x81 && c <= 0xFE; }
inline int  charBytes(unsigned char c)  { return isDbcsLead(c) ? 2 : 1; }

// 某个字节下标（std::string 下标）对应的"显示列"（全角算 2 列）
inline int dispColOfIndex(const std::string& s, int idx)
{
    int c = 0;
    int n = (int)s.size();
    for (int i = 0; i < idx && i < n; )
    {
        int nb = charBytes((unsigned char)s[i]);
        c += (nb == 2 ? 2 : 1);
        i += nb;
    }
    return c;
}

// "显示列" -> 最近的字节下标（点击定位光标用）
inline int indexOfDispCol(const std::string& s, int target)
{
    int c = 0, i = 0, n = (int)s.size();
    if (target <= 0) return 0;
    while (i < n)
    {
        int nb = charBytes((unsigned char)s[i]);
        int wd = (nb == 2 ? 2 : 1);
        if (target < c + wd / 2) return i;
        if (target < c + wd)     return i + nb;
        c += wd; i += nb;
    }
    return n;
}

inline int dispWidth(const std::string& s) { return dispColOfIndex(s, (int)s.size()); }

// ============================================================================
//  绘图工具
// ============================================================================
inline void fillRect(const Rect& r, COLORREF c)
{
    if (r.w() <= 0 || r.h() <= 0) return;
    setfillcolor(c);
    solidrectangle(r.x1, r.y1, r.x2, r.y2);
}

inline void frameRect(const Rect& r, COLORREF c)
{
    setlinecolor(c);
    rectangle(r.x1, r.y1, r.x2, r.y2);
}

inline void fillRectB(const Rect& r, COLORREF fill, COLORREF border)
{
    if (r.w() <= 0 || r.h() <= 0) return;
    setfillcolor(fill);
    setlinecolor(border);
    fillrectangle(r.x1, r.y1, r.x2, r.y2);
}

// 圆角矩形填充（只用 solidrectangle / solidcircle 实现，不依赖 roundrect 系列）
inline void fillRoundRect(const Rect& r, int rad, COLORREF c)
{
    if (r.w() <= 0 || r.h() <= 0) return;
    if (rad <= 0) { fillRect(r, c); return; }
    if (rad > r.w() / 2) rad = r.w() / 2;
    if (rad > r.h() / 2) rad = r.h() / 2;
    setfillcolor(c);
    solidrectangle(r.x1 + rad, r.y1, r.x2 - rad, r.y2);
    solidrectangle(r.x1, r.y1 + rad, r.x2, r.y2 - rad);
    solidcircle(r.x1 + rad, r.y1 + rad, rad);
    solidcircle(r.x2 - rad, r.y1 + rad, rad);
    solidcircle(r.x1 + rad, r.y2 - rad, rad);
    solidcircle(r.x2 - rad, r.y2 - rad, rad);
}

inline void drawStr(int x, int y, const std::string& s) { outtextxy(x, y, ts(s).c_str()); }
inline void drawStrT(int x, int y, const TCHAR* s)      { outtextxy(x, y, s); }

inline int  strWidth(const std::string& s) { return textwidth(ts(s).c_str()); }
inline int  strHeight(const std::string& s){ return textheight(ts(s).c_str()); }

// 在矩形内居中绘制文字
inline void drawStrCenter(const Rect& r, const std::string& s)
{
    int w = strWidth(s);
    int h = UI_FONT_H;
    int x = r.x1 + (r.w() - w) / 2;
    int y = r.y1 + (r.h() - h) / 2;
    if (x < r.x1 + 2) x = r.x1 + 2;
    outtextxy(x, y, ts(s).c_str());
}

#endif // MINIC_CONFIG_H
