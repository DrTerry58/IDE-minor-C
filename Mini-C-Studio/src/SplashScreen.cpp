// ============================================================================
// 文件名：SplashScreen.cpp
// 职责：启动欢迎界面实现
// 负责人：C（GUI 界面 + 交互控制）
//
// LOGO 说明（后期换图很方便）：
//   现在是"纯代码绘制"的占位 LOGO —— 圆角渐变方块 + 白色 C + 下划线光标。
//   想换成真正的图片：把 MINIC_SPLASH_USE_IMAGE 改成 1，
//   并把图片放到工程运行目录下的 res\logo.png 即可（会自动缩放绘制）。
// ============================================================================

#include "SplashScreen.h"
#include <cmath>
#include <cstdio>

// 想用图片 LOGO 就改成 1（图片路径：res\logo.png）
#define MINIC_SPLASH_USE_IMAGE   0

#define SPLASH_W    560
#define SPLASH_H    340

#define LOGO_SIZE   118
#define LOGO_TOP    RGB(64,124,255)     // LOGO 渐变起始色
#define LOGO_BOTTOM RGB(126,72,232)     // LOGO 渐变结束色
#define SPLASH_BG1  RGB(20,26,46)       // 背景渐变起始色
#define SPLASH_BG2  RGB(38,54,104)      // 背景渐变结束色

// ---------------------------------------------------------------------------
// 画一个"圆角的渐变矩形"（逐行扫描填充，避免依赖 roundrect 系列函数）
// ---------------------------------------------------------------------------
static void gradientRoundRect(const Rect& r, int rad, COLORREF cTop, COLORREF cBottom)
{
    int h = r.h();
    if (h <= 0 || r.w() <= 0) return;
    for (int y = 0; y < h; y++)
    {
        int inset = 0;
        int d = 0;
        if (y < rad)            d = rad - y;
        else if (y >= h - rad)  d = y - (h - rad);
        if (d > 0)
            inset = (int)(rad - sqrt((double)rad * rad - (double)d * d));

        double t = (h == 1) ? 0.0 : (double)y / (h - 1);
        setlinecolor(mixColor(cTop, cBottom, t));
        line(r.x1 + inset, r.y1 + y, r.x2 - inset, r.y1 + y);
    }
}

// ---------------------------------------------------------------------------
// LOGO 主体
// ---------------------------------------------------------------------------
void drawMiniCLogo(int cx, int cy, int size, double progress)
{
    Rect logo(cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2);
    int  rad = size / 5;

#if MINIC_SPLASH_USE_IMAGE
    // ---- 图片版 LOGO ----
    if (GetFileAttributes(_T("res\\logo.png")) != INVALID_FILE_ATTRIBUTES)
    {
        static IMAGE img;
        static bool  loaded = false;
        if (!loaded) { loadimage(&img, _T("res\\logo.png"), size, size, true); loaded = true; }
        putimage(logo.x1, logo.y1, &img);
        return;
    }
#endif

    // ---- 代码绘制版 LOGO ----
    // 1) 外圈柔和光晕
    setfillcolor(mixColor(SPLASH_BG2, LOGO_TOP, 0.28));
    solidcircle(cx, cy, size / 2 + 14);

    // 2) 渐变圆角方块
    gradientRoundRect(logo, rad, LOGO_TOP, LOGO_BOTTOM);

    // 3) 白色字母 C（画两遍做"伪粗体"）
    setbkmode(TRANSPARENT);
    settextcolor(WHITE);
    int fs = (int)(size * 0.56);
    settextstyle(uiFont(fs), 0, _T("Consolas"));
    int tw = textwidth(_T("C"));
    int th = textheight(_T("C"));
    int tx = cx - tw / 2;
    int ty = cy - th / 2 - (int)(size * 0.04);
    outtextxy(tx,     ty,     _T("C"));
    outtextxy(tx + 1, ty,     _T("C"));

    // 4) LOGO 内的"命令行下划线光标"
    int uw = (int)(size * 0.26);
    int uy = ty + th + (int)(size * 0.03);
    setfillcolor(WHITE);
    solidrectangle(cx - uw / 2, uy, cx - uw / 2 + (int)(uw * (progress < 0.2 ? 0.2 : 1.0)), uy + (int)(size * 0.045));
}

// ---------------------------------------------------------------------------
// 启动欢迎界面主流程
// ---------------------------------------------------------------------------
void showSplashScreen(int totalMs)
{
    // 1) 创建独立的欢迎窗口
    initgraph(SPLASH_W, SPLASH_H, EW_SHOWCONSOLE);
    SetWindowText(GetHWnd(), _T("Mini-C Studio - 启动中"));
    BeginBatchDraw();
    setbkmode(TRANSPARENT);

    DWORD t0 = GetTickCount();
    bool  skip = false;
    ExMessage msg;

    while (true)
    {
        DWORD el = GetTickCount() - t0;

        // 点击 / 按键 可跳过
        while (peekmessage(&msg, EX_MOUSE | EX_KEY | EX_CHAR, true))
        {
            if (msg.message == WM_LBUTTONDOWN || msg.message == WM_KEYDOWN ||
                msg.message == WM_RBUTTONDOWN || msg.message == WM_CHAR)
                skip = true;
        }
        if (skip || el >= (DWORD)totalMs) break;

        double t = (double)el / (double)totalMs;   // 0 ~ 1
        if (t > 1) t = 1;

        // ---------- 背景渐变 ----------
        for (int y = 0; y < SPLASH_H; y++)
        {
            setlinecolor(mixColor(SPLASH_BG1, SPLASH_BG2, (double)y / SPLASH_H));
            line(0, y, SPLASH_W, y);
        }

        // ---------- 装饰光点 ----------
        setfillcolor(mixColor(SPLASH_BG2, WHITE, 0.10));
        solidcircle(70,  70, 46);
        solidcircle(500, 290, 62);

        // ---------- LOGO（前 25% 时间做"弹出"缩放动画） ----------
        double pop = (t < 0.25) ? (t / 0.25) : 1.0;
        double e   = 1.0 - pow(1.0 - pop, 3.0);          // ease-out
        int    sz  = (int)(LOGO_SIZE * (0.72 + 0.28 * e));
        drawMiniCLogo(SPLASH_W / 2, 108, sz, t);

        // ---------- 主标题 ----------
        settextcolor(WHITE);
        settextstyle(uiFont(30), 0, _T("Microsoft YaHei"));
        int tw = textwidth(_T("Mini-C Studio"));
        outtextxy((SPLASH_W - tw) / 2, 190, _T("Mini-C Studio"));

        // ---------- 副标题 ----------
        settextcolor(RGB(176, 192, 226));
        settextstyle(uiFont(15), 0, _T("Microsoft YaHei"));
        std::string sub = std::string("轻量级 C 语言集成开发环境   ") + MINIC_VERSION;
        int sw = strWidth(sub);
        drawStr((SPLASH_W - sw) / 2, 232, sub);

        // ---------- 进度条 ----------
        int pbw = 260, pbh = 5;
        int pbx = (SPLASH_W - pbw) / 2, pby = 272;
        setfillcolor(RGB(58, 74, 116));
        solidrectangle(pbx, pby, pbx + pbw, pby + pbh);
        setfillcolor(mixColor(LOGO_TOP, WHITE, 0.15));
        solidrectangle(pbx, pby, pbx + (int)(pbw * t), pby + pbh);

        // ---------- 底部信息 ----------
        settextcolor(RGB(140, 158, 194));
        settextstyle(uiFont(13), 0, _T("Microsoft YaHei"));
        std::string org = MINIC_ORG;
        int ow = strWidth(org);
        drawStr((SPLASH_W - ow) / 2, 292, org);
        std::string tip = "点击鼠标或按任意键跳过";
        int tpw = strWidth(tip);
        drawStr((SPLASH_W - tpw) / 2, 314, tip);

        FlushBatchDraw();
        Sleep(16);
    }

    // 收尾：保证进度条走满再退出（避免"卡在 90%"的观感）
    EndBatchDraw();
    closegraph();          // 关掉欢迎窗口，主窗口随后 initgraph
}
