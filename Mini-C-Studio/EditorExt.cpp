// ============================================================================
// 文件名：EditorExt.cpp
// 负责人：C（GUI 界面 + 交互控制）
// 说明：查找 / 替换 / 按单词移动 的实现。
//      这里【不重新实现 B 的缓冲区】，只用 B 的公开只读接口 getLine
//      和已有原语（pasteText / deleteSelection / setSelection）组合。
//      B 交付 findNext / replaceAll 后，把本文件里对应函数体替换为
//      直接调用 B 的函数即可，MainWindow.cpp 一行不改。
// ============================================================================

#include "EditorExt.h"
#include <cctype>

// ---------------------------------------------------------------------------
//  字符分类：英文算单词字符，中文（GBK 双字节）每个字节都算单词字符，
//  这样中文连在一起会被当成一个"词"。
// ---------------------------------------------------------------------------
static bool isWordByte(unsigned char c)
{
    if (c >= 0x81 && c <= 0xFE) return true;      // GBK 首/次字节
    return isalnum(c) || c == '_';
}

void bufMoveWordLeft(EditorBuffer* b)
{
    if (!b) return;
    int r = b->getCursorY(), c = b->getCursorX();

    // 先跳过紧邻的空白
    while (r >= 0)
    {
        std::string s = b->getLine(r);
        while (c > 0 && !isWordByte((unsigned char)s[c - 1])) c--;
        if (c > 0) break;
        if (r == 0) { bufSetCursor(b, 0, 0); return; }
        r--; c = b->getLineLength(r);
    }
    // 再跳过单词
    std::string s = b->getLine(r);
    while (c > 0 && isWordByte((unsigned char)s[c - 1])) c--;
    bufSetCursor(b, r, c);
}

void bufMoveWordRight(EditorBuffer* b)
{
    if (!b) return;
    int r = b->getCursorY(), c = b->getCursorX();
    int total = b->getLineCount();

    while (r < total)
    {
        std::string s = b->getLine(r);
        int len = (int)s.size();
        while (c < len && !isWordByte((unsigned char)s[c])) c++;
        if (c < len) break;
        if (r + 1 >= total) { bufSetCursor(b, r, len); return; }
        r++; c = 0;
    }
    std::string s = b->getLine(r);
    int len = (int)s.size();
    while (c < len && isWordByte((unsigned char)s[c])) c++;
    bufSetCursor(b, r, c);
}

// ---------------------------------------------------------------------------
//  取任意区间文本
// ---------------------------------------------------------------------------
std::string bufRangeText(EditorBuffer* b, int r1, int c1, int r2, int c2)
{
    if (!b) return "";
    // 归一化
    if (r1 > r2 || (r1 == r2 && c1 > c2))
    {
        int t;
        t = r1; r1 = r2; r2 = t;
        t = c1; c1 = c2; c2 = t;
    }
    std::string out;
    for (int r = r1; r <= r2; r++)
    {
        std::string s = b->getLine(r);
        int a = (r == r1) ? c1 : 0;
        int z = (r == r2) ? c2 : (int)s.size();
        if (a < 0) a = 0;
        if (z > (int)s.size()) z = (int)s.size();
        if (z > a) out += s.substr(a, z - a);
        if (r < r2) out += "\r\n";
    }
    return out;
}

// ---------------------------------------------------------------------------
//  大小写不敏感比较（只处理 ASCII 字母，中文不受影响）
// ---------------------------------------------------------------------------
static bool eqCharCI(char a, char b, bool matchCase)
{
    if (matchCase) return a == b;
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    return a == b;
}

bool bufFindNext(EditorBuffer* b,
                 const std::string& pattern,
                 bool matchCase,
                 int fromR, int fromC,
                 int& outR, int& outC, int& outLen,
                 bool wrap)
{
    outR = outC = 0; outLen = 0;
    if (!b || pattern.empty()) return false;

    int total = b->getLineCount();
    if (fromR < 0) fromR = 0;
    if (fromR >= total) fromR = total - 1;

    int laps = wrap ? 2 : 1;
    for (int lap = 0; lap < laps; lap++)
    {
        for (int r = (lap == 0 ? fromR : 0); r < total; r++)
        {
            std::string s = b->getLine(r);
            int start = (lap == 0 && r == fromR) ? fromC : 0;
            if (start < 0) start = 0;
            int limit = (int)s.size() - (int)pattern.size();
            for (int c = start; c <= limit; c++)
            {
                bool ok = true;
                for (size_t k = 0; k < pattern.size(); k++)
                {
                    if (!eqCharCI(s[c + k], pattern[k], matchCase)) { ok = false; break; }
                }
                if (ok)
                {
                    outR = r; outC = c; outLen = (int)pattern.size();
                    return true;
                }
            }
        }
    }
    return false;
}

int bufReplaceAll(EditorBuffer* b,
                  const std::string& pattern,
                  const std::string& replacement,
                  bool matchCase)
{
    if (!b || pattern.empty()) return 0;

    // 用 B 的只读接口把全文拿出来，替换完再整体写回。
    // 这样完全不需要访问 B 的内部结构。
    std::string text = b->saveToString();
    std::string out;
    int count = 0;
    size_t n = pattern.size();

    for (size_t i = 0; i < text.size(); )
    {
        if (i + n <= text.size())
        {
            bool ok = true;
            for (size_t k = 0; k < n; k++)
            {
                if (!eqCharCI(text[i + k], pattern[k], matchCase)) { ok = false; break; }
            }
            if (ok)
            {
                out += replacement;
                i += n;
                count++;
                continue;
            }
        }
        out += text[i];
        i++;
    }

    if (count > 0)
    {
        int r = b->getCursorY(), c = b->getCursorX();
        b->clearSelection();
        b->loadFromString(out);
        b->setDirty(true);
        bufSetCursor(b, r, c);   // 经宏映射，保证 (列,行) 顺序正确（B 同学答复 Q1）
    }
    return count;
}
