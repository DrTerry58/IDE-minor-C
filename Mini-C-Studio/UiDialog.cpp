// ============================================================================
// 文件名：UiDialog.cpp
// 职责：自绘弹窗实现
// 负责人：C（GUI 界面 + 交互控制）
// ============================================================================

#include "UiDialog.h"
#include <commdlg.h>
#include <vector>
#include <string>

namespace
{
    struct BtnDef { const char* text; DlgRet ret; };

    std::vector<std::string> splitLines(const std::string& s)
    {
        std::vector<std::string> v;
        std::string cur;
        for (size_t i = 0; i < s.size(); i++)
        {
            if (s[i] == '\n') { v.push_back(cur); cur.clear(); }
            else if (s[i] != '\r') cur += s[i];
        }
        v.push_back(cur);
        return v;
    }
}

// ---------------------------------------------------------------------------
// 自绘模态消息框
// ---------------------------------------------------------------------------
DlgRet uiMessageBox(int winW, int winH, const Theme& th,
                    const std::string& title,
                    const std::string& text,
                    DlgBtn buttons)
{
    std::vector<BtnDef> btns;
    switch (buttons)
    {
    case DLG_OK:          btns.push_back(BtnDef{ "确定", RET_OK }); break;
    case DLG_OKCANCEL:    btns.push_back(BtnDef{ "确定", RET_OK }); btns.push_back(BtnDef{ "取消", RET_CANCEL }); break;
    case DLG_YESNO:       btns.push_back(BtnDef{ "是", RET_YES });  btns.push_back(BtnDef{ "否", RET_NO }); break;
    case DLG_YESNOCANCEL: btns.push_back(BtnDef{ "是", RET_YES });  btns.push_back(BtnDef{ "否", RET_NO });
                          btns.push_back(BtnDef{ "取消", RET_CANCEL }); break;
    }

    std::vector<std::string> lines = splitLines(text);

    // ---- 计算对话框尺寸 ----
    settextstyle(15, 0, _T("Microsoft YaHei"));
    int maxTw = strWidth(title);
    for (size_t i = 0; i < lines.size(); i++)
        if (strWidth(lines[i]) > maxTw) maxTw = strWidth(lines[i]);

    int bw = 92, bh = 30, gap = 12;
    int btnAreaW = (int)btns.size() * bw + ((int)btns.size() - 1) * gap;
    int dlgW = maxTw + 64;
    if (dlgW < btnAreaW + 48) dlgW = btnAreaW + 48;
    if (dlgW > winW - 80) dlgW = winW - 80;
    int dlgH = 40 + 26 + (int)lines.size() * 22 + 22 + bh + 26;
    int dx = (winW - dlgW) / 2;
    int dy = (winH - dlgH) / 2 - 20;
    Rect dlg(dx, dy, dx + dlgW, dy + dlgH);

    DlgRet result = RET_NONE;
    int    mx = -1, my = -1;
    ExMessage msg;

    while (result == RET_NONE)
    {
        // ---- 消息 ----
        while (peekmessage(&msg, EX_MOUSE | EX_KEY | EX_CHAR, true))
        {
            if (msg.message == WM_MOUSEMOVE) { mx = msg.x; my = msg.y; }
            else if (msg.message == WM_LBUTTONDOWN)
            {
                for (int i = 0; i < (int)btns.size(); i++)
                {
                    Rect b(dlg.x2 - 24 - ((int)btns.size() - i) * (bw + gap),
                           dlg.y2 - 24 - bh,
                           dlg.x2 - 24 - ((int)btns.size() - i) * (bw + gap) + bw,
                           dlg.y2 - 24);
                    if (b.hit(msg.x, msg.y)) { result = btns[i].ret; break; }
                }
            }
            else if (msg.message == WM_KEYDOWN)
            {
                if (msg.vkcode == VK_ESCAPE)
                    result = (buttons == DLG_OK) ? RET_OK : RET_CANCEL;
                else if (msg.vkcode == VK_RETURN)
                    result = btns[0].ret;
                else if ((msg.vkcode == 'Y' || msg.vkcode == 'y') && buttons != DLG_OK && buttons != DLG_OKCANCEL)
                    result = RET_YES;
                else if ((msg.vkcode == 'N' || msg.vkcode == 'n') && buttons != DLG_OK && buttons != DLG_OKCANCEL)
                    result = RET_NO;
            }
        }
        if (result != RET_NONE) break;

        // ---- 绘制 ----
        setbkmode(TRANSPARENT);

        // 半透明遮罩：用网点模拟
        setlinecolor(mixColor(th.bg, RGB(0, 0, 0), 0.45));
        for (int y = 0; y < winH; y += 3) line(0, y, winW, y);

        // 阴影 + 面板
        fillRoundRect(Rect(dlg.x1 + 4, dlg.y1 + 5, dlg.x2 + 4, dlg.y2 + 5), 12, RGB(0, 0, 0));
        fillRoundRect(dlg, 12, th.panel);
        setlinecolor(th.border);
        // 顶部点缀色条
        setfillcolor(th.accent);
        solidrectangle(dlg.x1 + 12, dlg.y1, dlg.x2 - 12, dlg.y1 + 4);

        // 标题
        settextcolor(th.text);
        settextstyle(16, 0, _T("Microsoft YaHei"));
        drawStr(dlg.x1 + 24, dlg.y1 + 22, title);

        // 正文
        settextcolor(th.dim);
        settextstyle(15, 0, _T("Microsoft YaHei"));
        for (size_t i = 0; i < lines.size(); i++)
            drawStr(dlg.x1 + 24, dlg.y1 + 56 + (int)i * 22, lines[i]);

        // 按钮
        for (int i = 0; i < (int)btns.size(); i++)
        {
            Rect b(dlg.x2 - 24 - ((int)btns.size() - i) * (bw + gap),
                   dlg.y2 - 24 - bh,
                   dlg.x2 - 24 - ((int)btns.size() - i) * (bw + gap) + bw,
                   dlg.y2 - 24);
            bool hv = b.hit(mx, my);
            bool primary = (i == (int)btns.size() - 1);
            fillRoundRect(b, 6, primary ? th.accent : (hv ? th.btnHover : th.btn));
            settextcolor(primary ? RGB(255, 255, 255) : th.btnText);
            settextstyle(15, 0, _T("Microsoft YaHei"));
            drawStrCenter(b, btns[i].text);
        }

        FlushBatchDraw();
        Sleep(16);
    }

    return result;
}

// ---------------------------------------------------------------------------
// 输入框（复用 EasyX InputBox，天然支持中文输入法）
// ---------------------------------------------------------------------------
bool uiInputBox(const std::string& title, const std::string& prompt,
                std::string& out, const std::string& defValue)
{
    TCHAR buf[1024] = { 0 };
    if (!defValue.empty())
    {
        std::basic_string<TCHAR> d = ts(defValue);
        if (d.size() < 1023) _tcsncpy(buf, d.c_str(), 1023);
    }
    BOOL ok = InputBox(buf, 1024,
                       ts(prompt).c_str(), ts(title).c_str(),
                       NULL, 520, 150, false);
    if (!ok) return false;
#ifdef UNICODE
    out = w2s(buf);
#else
    out = buf;
#endif
    return true;
}

// ---------------------------------------------------------------------------
// 选择路径
//   当前实现：InputBox 手输路径（最简单、最不容易出环境兼容问题）
//   想换成系统文件对话框，把下面的 MINIC_USE_SYSFILEDLG 改成 1 即可
// ---------------------------------------------------------------------------
#define MINIC_USE_SYSFILEDLG  0

bool uiPickPath(const std::string& title, std::string& path, bool forSave)
{
#if MINIC_USE_SYSFILEDLG
    TCHAR buf[MAX_PATH] = { 0 };
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = GetHWnd();
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrFilter = _T("C 源文件 (*.c)\0*.c\0所有文件 (*.*)\0*.*\0");
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | (forSave ? OFN_OVERWRITEPROMPT : 0);
    BOOL ok = forSave ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn);
    if (!ok) return false;
#ifdef UNICODE
    path = w2s(buf);
#else
    path = buf;
#endif
    return true;
#else
    std::string tip = forSave
        ? "请输入保存路径（例如 D:\\test\\hello.c）："
        : "请输入要打开的 .c 文件路径：";
    std::string p;
    if (!uiInputBox(title, tip, p, path)) return false;
    if (p.empty()) return false;
    path = p;
    return true;
#endif
}
