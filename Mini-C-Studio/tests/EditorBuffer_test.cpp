// ============================================================================
//  EditorBuffer_test.cpp
//  EditorBuffer 模块的单元测试（无 GUI 依赖，可独立编译运行）
//
//  编译：
//    g++ -std=c++11 -Wall -Wextra EditorBuffer.cpp EditorBuffer_test.cpp -o eb_test
//  运行：
//    ./eb_test
//  全部通过输出：=== ALL TESTS PASSED ===
// ============================================================================

#include "EditorBuffer.h"

#include <iostream>
#include <string>

static int g_failed = 0;
static int g_total = 0;

static void check(bool cond, const std::string& name)
{
    ++g_total;
    if (!cond)
    {
        ++g_failed;
        std::cout << "FAIL: " << name << "\n";
    }
}

// 依次调用 insertChar 把一个字符串塞进缓冲区
static void type(EditorBuffer& b, const std::string& s)
{
    for (size_t i = 0; i < s.size(); ++i)
    {
        b.insertChar(s[i]);
    }
}

int main()
{
    // ---- 构造 ----
    {
        EditorBuffer b;
        check(b.getLineCount() == 1 && b.getCursorX() == 0 && b.getCursorY() == 0,
              "ctor: 一行空行 + 光标(0,0)");
        check(!b.isDirty() && b.getFilePath().empty(), "ctor: 未修改 + 无路径");
    }

    // ---- 越界安全 ----
    {
        EditorBuffer b;
        check(b.getLineLength(-1) == -1 && b.getLineLength(5) == -1, "oob: getLineLength");
        check(b.getLine(99).empty() && b.getLine(-3).empty(), "oob: getLine");
        b.setCursor(1000, 1000);
        check(b.getCursorX() == 0 && b.getCursorY() == 0, "setCursor: 大坐标被夹回(0,0)");
        b.moveCursor(-50, -50);
        check(b.getCursorX() == 0 && b.getCursorY() == 0, "moveCursor: 负数被夹回(0,0)");
    }

    // ---- 插入 + 换行 ----
    {
        EditorBuffer b;
        type(b, "ab\ncd");
        check(b.getLineCount() == 2 && b.getLine(0) == "ab" && b.getLine(1) == "cd",
              "insert: 换行拆两行");
        check(b.getCursorY() == 1 && b.getCursorX() == 2, "insert: 光标停在(cd)行首后");
        check(b.isDirty(), "insert: 置脏");
        check(b.saveToString() == "ab\ncd", "saveToString: 往返一致");
    }

    // ---- 行首退格合并 / 文首无操作 ----
    {
        EditorBuffer b;
        b.loadFromString("ab\ncd");
        b.setCursor(0, 1);
        b.deleteChar();
        check(b.getLineCount() == 1 && b.getLine(0) == "abcd" && b.getCursorX() == 2,
              "backspace: 行首合并上一行");
        b.setCursor(0, 0);
        b.setDirty(false);
        b.deleteChar();
        check(b.getLine(0) == "abcd" && !b.isDirty(), "backspace: 文首无操作且不置脏");
    }

    // ---- 行尾 Delete 合并 / 文末无操作 ----
    {
        EditorBuffer b;
        b.loadFromString("ab\ncd");
        b.setCursor(2, 0);
        b.deleteForward();
        check(b.getLineCount() == 1 && b.getLine(0) == "abcd", "del: 行尾合并下一行");
        b.setCursor(4, 0);
        b.setDirty(false);
        b.deleteForward();
        check(b.getLine(0) == "abcd" && !b.isDirty(), "del: 文末无操作且不置脏");
    }

    // ---- 空文件 / 换行结尾 / CRLF ----
    {
        EditorBuffer b;
        b.loadFromString("");
        check(b.getLineCount() == 1 && b.getLine(0) == "" && !b.isDirty(), "load: 空内容留一行");
        b.loadFromString("a\n");
        check(b.getLineCount() == 2 && b.getLine(1) == "" && b.saveToString() == "a\n",
              "load: 末尾换行保留空行");
        b.loadFromString("x\r\ny\r\n");
        check(b.getLine(0) == "x" && b.getLine(1) == "y" && b.getLineCount() == 3,
              "load: CRLF 兼容");
    }

    // ---- 选中：规范化 / 跨行取文本 / 删除 ----
    {
        EditorBuffer b;
        b.loadFromString("hello\nworld\nfoo");
        b.setSelection(2, 2, 3, 0);   // 反向传入
        check(b.hasSelection(), "sel: 有选区");
        check(b.getSelectedText() == "lo\nworld\nfo", "sel: 跨行文本");
        int sx, sy, ex, ey;
        check(b.getSelectionRange(sx, sy, ex, ey)
              && sx == 3 && sy == 0 && ex == 2 && ey == 2,
              "sel: getSelectionRange 规范化输出");
        b.deleteSelection();
        check(b.getLineCount() == 1 && b.getLine(0) == "helo" && !b.hasSelection(),
              "sel: 跨行删除");
        check(b.getCursorX() == 3 && b.getCursorY() == 0, "sel: 删除后光标落起点");
        b.setSelection(1, 0, 1, 0);
        check(!b.hasSelection(), "sel: 空选区视为无");
        int a, bb, c, d;
        check(!b.getSelectionRange(a, bb, c, d), "sel: 空选区 query 返回 false");
        b.setSelection(0, 0, 999, 999);
        check(b.getSelectedText() == "helo", "sel: 越界被裁剪");
        check(b.getSelectionRange(a, bb, c, d)
              && a == 0 && bb == 0 && c == 4 && d == 0,
              "sel: 非空选区 query 返回规范化坐标");
    }

    // ---- 粘贴单行 / 多行 ----
    {
        EditorBuffer b;
        b.loadFromString("abcd");
        b.setCursor(2, 0);
        b.pasteText("XY");
        check(b.getLine(0) == "abXYcd" && b.getCursorX() == 4, "paste: 单行插入");
        b.loadFromString("abcd");
        b.setCursor(2, 0);
        b.pasteText("1\n2\n3");
        check(b.getLineCount() == 3
              && b.getLine(0) == "ab1" && b.getLine(1) == "2" && b.getLine(2) == "3cd",
              "paste: 多行插入");
        check(b.getCursorY() == 2 && b.getCursorX() == 1, "paste: 光标落末尾");
    }

    // ---- Tab：插入 4 个空格 ----
    {
        EditorBuffer b;
        b.loadFromString("a");
        b.setCursor(1, 0);
        b.insertChar('\t');
        check(b.getLine(0) == "a    " && b.getCursorX() == 5, "tab: 展开成4空格");
    }

    // ---- 撤销合并：连续字符一次撤销 / 回车切断 ----
    {
        EditorBuffer b;
        type(b, "hello");
        check(b.canUndo(), "undo: 有可撤销");
        b.undo();
        check(b.getLine(0).empty() && b.getCursorX() == 0, "undo: 一次撤销整串字符");
        check(b.canRedo(), "redo: 有可重做");
        b.redo();
        check(b.getLine(0) == "hello", "redo: 恢复整串字符");

        // 回车后输入属于另一个撤销单元
        b.loadFromString("");
        type(b, "ab");
        b.enter();
        type(b, "cd");
        b.undo();  // 撤掉 "cd"
        check(b.getLine(0) == "ab" && b.getLine(1) == "" && b.getCursorY() == 1,
              "undo: 回车后 cd 单独撤销");
        b.undo();  // 撤掉 "回车"
        check(b.getLineCount() == 1 && b.getLine(0) == "ab", "undo: 回车也单独一步");
        b.undo();  // 撤掉 "ab"
        check(b.getLine(0) == "" && b.getLineCount() == 1, "undo: 回车前的 ab 继续可撤销");
    }

    // ---- 多行粘贴只记一步撤销 ----
    {
        EditorBuffer b;
        b.loadFromString("abcd");
        b.setCursor(2, 0);
        b.pasteText("1\n2\n3");
        b.undo();
        check(b.getLineCount() == 1 && b.getLine(0) == "abcd", "undo: 一次粘贴一步撤销");
    }

    // ---- 括号配对 ----
    {
        EditorBuffer b;
        b.loadFromString("int f(int a){\n  if(a){\n  }\n}");
        int x = 0, y = 0;
        check(b.findMatchingBracket(12, 0, x, y) && x == 0 && y == 3, "bracket: { 跨行配对");
        check(b.findMatchingBracket(0, 3, x, y) && x == 12 && y == 0, "bracket: } 反向配对");
        check(b.findMatchingBracket(4, 1, x, y) && x == 6 && y == 1, "bracket: 同行 ( 配对");
        check(!b.findMatchingBracket(1, 0, x, y), "bracket: 非括号返回 false");
        check(!b.findMatchingBracket(0, 99, x, y), "bracket: 越界行返回 false");
    }

    // ---- 可折叠区域（跳过注释/字符串里的花括号）----
    {
        EditorBuffer b;
        b.loadFromString("void f(){\n  // }\n  char* s=\"{\";\n  while(1){\n  }\n}\nint g(){}");
        std::vector<std::pair<int, int> > r = b.getFoldableRegions();
        check(r.size() == 2, "fold: 数量");
        if (r.size() == 2)
        {
            check(r[0] == std::make_pair(0, 5) && r[1] == std::make_pair(3, 4), "fold: 区间");
        }
    }

    // ---- 自动缩进：'{' 后回车加深一级 ----
    {
        EditorBuffer b;
        b.loadFromString("if(1){");
        b.setCursor(6, 0);
        b.enter();
        check(b.getLine(0) == "if(1){" && b.getLine(1) == "    " && b.getCursorX() == 4,
              "indent: { 后回车缩进一级");
        b.insertChar('}');
        // 类型 '}' 应以 '}' 回退一级（光标前的空白 >= 4）
        check(b.getLine(1) == "}", "indent: 输入 } 自动回退一级");
    }

    // ---- 自动缩进：非 '{' 行回车只继承原缩进 ----
    {
        EditorBuffer b;
        b.loadFromString("abc");
        b.setCursor(3, 0);
        b.enter();
        check(b.getLine(1) == "" && b.getCursorX() == 0, "indent: 无缩进则不加深");
    }

    // ---- getLineRef：内容一致且为引用 ----
    {
        EditorBuffer b;
        b.loadFromString("hello\nworld");
        check(b.getLineRef(0) == "hello", "getLineRef: 内容正确");
        check(b.getLineRef(99).empty(), "getLineRef: 越界返回空串");
        check(&b.getLineRef(0) == &b.getLineRef(0), "getLineRef: 返回引用");
    }

    // ---- 大批量输入性能（2000 行不多于可感知时间）----
    {
        EditorBuffer b;
        b.loadFromString("");
        std::string bigLine;
        for (int i = 0; i < 100; ++i)
        {
            bigLine += "abcdefghij";
        }
        for (int i = 0; i < 2000; ++i)
        {
            b.insertChar('\n');
            b.insertChar(static_cast<char>('A' + (i % 26)));  // 每个新行先放一个字符
        }
        check(b.getLineCount() == 2001, "perf: 2000 行建行");
    }

    if (g_failed == 0)
    {
        std::cout << "=== ALL TESTS PASSED ===" << std::endl;
    }
    else
    {
        std::cout << "=== " << g_failed << "/" << g_total << " TESTS FAILED ===" << std::endl;
    }
    return g_failed == 0 ? 0 : 1;
}
