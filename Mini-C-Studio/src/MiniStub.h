// ============================================================================
// 文件名：MiniStub.h
// 职责：B / E / D 三位同学模块的【占位实现】
//
// ！！！重要说明（给全组同学看）！！！
// 这三个类的成员函数签名 = C 同学（GUI）对你模块的【全部调用需求】。
// 你只需要：
//   1) 自己新建同名的类（EditorBuffer / FileManager / Compiler / Runtime），
//      把这些函数都实现一遍（内部换成你自研的数据结构，比如行链表、管道 IO）；
//   2) 打开 CoreApi.cpp，把最上面对应的 USE_X_REAL_XXX 宏从 0 改成 1，
//      并把你的头文件 include 进去。
// 除此之外，GUI 层一行代码都不用改。
//
// 目前的占位实现只是为了"C 同学能独立跑起来调界面"，
// 性能与功能都很粗糙（例如 vector<string> 缓冲、system() 调 gcc），
// 后期必须被真实实现替换。
// ============================================================================

#pragma once
#ifndef MINIC_STUB_H
#define MINIC_STUB_H

#include "MiniCConfig.h"
#include "CoreTypes.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <vector>
#include <string>
#include <cstdlib>

// ============================================================================
//  B 同学：自研文本缓冲区
// ----------------------------------------------------------------------------
//  约定（GUI 与 B 必须遵守，否则会错位）：
//   · 行号 row 从 0 开始，状态栏显示时 +1；
//   · 列号 col 从 0 开始，含义是"该行 std::string 的下标"；
//   · 文件末尾保证至少存在 1 行（空文件 = 1 行空串）；
//   · undo/redo 由 B 内部维护，GUI 只调用 undo()/redo()/canUndo()/canRedo()。
// ============================================================================
class MiniBuffer
{
public:
    MiniBuffer() : m_row(0), m_col(0), m_dirty(false) { m_lines.push_back(""); }

    // ---- 读取 ----
    int         getLineCount() const { return (int)m_lines.size(); }
    std::string getLine(int row) const
    {
        if (row < 0 || row >= (int)m_lines.size()) return "";
        return m_lines[row];
    }
    int         getCursorY() const { return m_row; }   // 行，0-based
    int         getCursorX() const { return m_col; }   // 列，0-based
    bool        isDirty()     const { return m_dirty; }
    std::string getFilePath() const { return m_path; }

    // ---- 光标 ----
    void setCursor(int row, int col)
    {
        clampRow(row);
        m_row = row;
        m_col = col;
        clampCol();
    }
    void gotoLine(int row) { setCursor(row, 0); }
    void moveCursor(int dx, int dy)
    {
        // 先左右（可跨行），再上下
        // 注意：左右必须按【字符】移动，不能按字节。
        // 否则光标会停在汉字的两个字节中间，显示列不变 -> 看起来"按一下不动"。
        if (dx < 0)
        {
            for (int i = 0; i < -dx; i++)
            {
                if (m_col > 0) m_col = charStartBefore(m_lines[m_row], m_col);
                else if (m_row > 0) { m_row--; m_col = (int)m_lines[m_row].size(); }
            }
        }
        else if (dx > 0)
        {
            for (int i = 0; i < dx; i++)
            {
                int len = (int)m_lines[m_row].size();
                if (m_col < len) m_col += charLenAt(m_lines[m_row], m_col);
                else if (m_row + 1 < (int)m_lines.size()) { m_row++; m_col = 0; }
            }
        }
        if (dy != 0)
        {
            m_row += dy;
            clampRow(m_row);
            clampCol();
        }
    }

    // ---- 编辑 ----
    void insertChar(char c) { insertString(std::string(1, c)); }

    void insertString(const std::string& s) { insertAt(m_row, m_col, s); }

    void insertAt(int row, int col, const std::string& s)
    {
        if (row < 0 || row >= (int)m_lines.size()) return;
        pushUndo();
        std::vector<std::string> parts = splitLines(s);
        if (parts.empty()) return;
        const std::string cur  = m_lines[row];
        if (col > (int)cur.size()) col = (int)cur.size();
        const std::string head = cur.substr(0, col);
        const std::string tail = cur.substr(col);

        std::vector<std::string> rebuilt;
        for (int i = 0; i < row; i++) rebuilt.push_back(m_lines[i]);
        if (parts.size() == 1)
        {
            rebuilt.push_back(head + parts[0] + tail);
        }
        else
        {
            rebuilt.push_back(head + parts[0]);
            for (size_t i = 1; i + 1 < parts.size(); i++) rebuilt.push_back(parts[i]);
            rebuilt.push_back(parts[parts.size() - 1] + tail);
        }
        for (size_t i = row + 1; i < m_lines.size(); i++) rebuilt.push_back(m_lines[i]);
        m_lines = rebuilt;

        // 光标落到插入内容末尾
        int r = row + (int)parts.size() - 1;
        int c;
        if (parts.size() == 1) c = col + (int)parts[0].size();
        else                   c = (int)parts[parts.size() - 1].size();
        m_row = r; m_col = c;
        clampRow(m_row); clampCol();
        m_dirty = true;
    }

    void deleteChar()   // Backspace
    {
        pushUndo();
        if (m_col > 0)
        {
            std::string& cur = m_lines[m_row];
            // 全角字符整块删除：必须从行首扫描确定前一个字符的起点。
            // 不能用 isDbcsLead(cur[m_col-2]) 判断 —— GBK 尾字节也可能 >=0x81，
            // 会把 ASCII / 空格连同前一个汉字的尾字节一起删掉，导致整行错位变乱码。
            int st = charStartBefore(cur, m_col);
            cur.erase(st, m_col - st);
            m_col = st;
        }
        else if (m_row > 0)
        {
            int newCol = (int)m_lines[m_row - 1].size();
            m_lines[m_row - 1] += m_lines[m_row];
            m_lines.erase(m_lines.begin() + m_row);
            m_row--; m_col = newCol;
        }
        m_dirty = true;
    }

    void deleteForward()  // Delete
    {
        pushUndo();
        std::string& cur = m_lines[m_row];
        if (m_col < (int)cur.size())
        {
            int nb = charLenAt(cur, m_col);
            cur.erase(m_col, nb);
        }
        else if (m_row + 1 < (int)m_lines.size())
        {
            m_lines[m_row] += m_lines[m_row + 1];
            m_lines.erase(m_lines.begin() + m_row + 1);
        }
        m_dirty = true;
    }

    void deleteRange(int r1, int c1, int r2, int c2)
    {
        if (r1 > r2 || (r1 == r2 && c1 > c2)) { int t = r1; r1 = r2; r2 = t; t = c1; c1 = c2; c2 = t; }
        if (r1 < 0) r1 = 0;
        if (r2 >= (int)m_lines.size()) r2 = (int)m_lines.size() - 1;
        if (r1 > r2) return;
        pushUndo();
        if (r1 == r2)
        {
            m_lines[r1].erase(c1, c2 - c1);
        }
        else
        {
            std::string head = m_lines[r1].substr(0, c1);
            std::string tail = (c2 < (int)m_lines[r2].size()) ? m_lines[r2].substr(c2) : "";
            m_lines[r1] = head + tail;
            m_lines.erase(m_lines.begin() + r1 + 1, m_lines.begin() + r2 + 1);
        }
        m_row = r1; m_col = c1;
        clampRow(m_row); clampCol();
        m_dirty = true;
    }

    std::string getRangeText(int r1, int c1, int r2, int c2) const
    {
        if (r1 > r2 || (r1 == r2 && c1 > c2)) { int t = r1; r1 = r2; r2 = t; t = c1; c1 = c2; c2 = t; }
        if (r1 < 0) r1 = 0;
        if (r2 >= (int)m_lines.size()) r2 = (int)m_lines.size() - 1;
        if (r1 > r2) return "";
        std::string out;
        for (int r = r1; r <= r2; r++)
        {
            const std::string& L = m_lines[r];
            int a = (r == r1) ? c1 : 0;
            int b = (r == r2) ? c2 : (int)L.size();
            if (a > (int)L.size()) a = (int)L.size();
            if (b > (int)L.size()) b = (int)L.size();
            if (r > r1) out += "\n";
            out += L.substr(a, b - a);
        }
        return out;
    }

    void enter()
    {
        pushUndo();
        std::string tail = m_lines[m_row].substr(m_col);
        m_lines[m_row] = m_lines[m_row].substr(0, m_col);
        m_lines.insert(m_lines.begin() + m_row + 1, tail);
        m_row++; m_col = 0;
        m_dirty = true;
    }

    // ---- 整体读写 ----
    void loadFromString(const std::string& text)
    {
        m_lines = splitLines(text);
        if (m_lines.empty()) m_lines.push_back("");
        m_row = 0; m_col = 0; m_dirty = false;
        m_undo.clear(); m_redo.clear();
    }
    std::string saveToString() const
    {
        std::string out;
        for (size_t i = 0; i < m_lines.size(); i++)
        {
            if (i) out += "\n";
            out += m_lines[i];
        }
        return out;
    }
    void clear() { m_lines.clear(); m_lines.push_back(""); m_row = m_col = 0; m_dirty = false; m_undo.clear(); m_redo.clear(); }

    void setDirty(bool d)      { m_dirty = d; }
    void setFilePath(const std::string& p) { m_path = p; }

    // ---- 撤销 / 重做（占位：整体快照，B 同学可改成操作栈） ----
    void undo()
    {
        if (m_undo.empty()) return;
        Snap s = m_undo.back(); m_undo.pop_back();
        Snap cur; cur.text = saveToString(); cur.row = m_row; cur.col = m_col;
        m_redo.push_back(cur);
        m_lines = splitLines(s.text);
        if (m_lines.empty()) m_lines.push_back("");
        m_row = s.row; m_col = s.col; clampRow(m_row); clampCol();
        m_dirty = true;
    }
    void redo()
    {
        if (m_redo.empty()) return;
        Snap s = m_redo.back(); m_redo.pop_back();
        Snap cur; cur.text = saveToString(); cur.row = m_row; cur.col = m_col;
        m_undo.push_back(cur);
        m_lines = splitLines(s.text);
        if (m_lines.empty()) m_lines.push_back("");
        m_row = s.row; m_col = s.col; clampRow(m_row); clampCol();
        m_dirty = true;
    }
    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }

private:
    struct Snap { std::string text; int row, col; };
    std::vector<std::string> m_lines;
    std::vector<Snap>        m_undo, m_redo;
    int  m_row, m_col;
    bool m_dirty;
    std::string m_path;

    void clampRow(int& r)
    {
        if (r < 0) r = 0;
        if (r >= (int)m_lines.size()) r = (int)m_lines.size() - 1;
    }
    void clampCol()
    {
        int len = (int)m_lines[m_row].size();
        if (m_col < 0) m_col = 0;
        if (m_col > len) m_col = len;
        // 保险：万一光标落在汉字两字节中间（外部 setCursor 传了非法值），对齐到字符起点
        if (m_col > 0 && m_col < len) m_col = charStartAt(m_lines[m_row], m_col);
    }
    void pushUndo()
    {
        Snap s; s.text = saveToString(); s.row = m_row; s.col = m_col;
        m_undo.push_back(s);
        if ((int)m_undo.size() > 200) m_undo.erase(m_undo.begin());
        m_redo.clear();
    }
    static std::vector<std::string> splitLines(const std::string& s)
    {
        std::vector<std::string> v;
        std::string cur;
        for (size_t i = 0; i < s.size(); i++)
        {
            char c = s[i];
            if (c == '\r') continue;
            if (c == '\n') { v.push_back(cur); cur.clear(); }
            else cur += c;
        }
        v.push_back(cur);
        return v;
    }
};

// ============================================================================
//  E 同学：文件生命周期管理
// ============================================================================
// 文件管理模块只用到缓冲区的 loadFromString / saveToString / setFilePath / setDirty，
// 以及“清空”（用 loadFromString("") 实现，MiniBuffer 与 EditorBuffer 都有该方法）。
// 因此这里把参数做成模板：既兼容占位实现 MiniBuffer，也兼容 B 同学真实的 EditorBuffer。
template <typename Buf>
class MiniFileManager
{
public:
    bool newFile(Buf& b)
    {
        b.loadFromString("");   // 等价于 clear()：重置为 1 行空行、光标归零、清空撤销栈、脏标记置 false
        b.setFilePath("");
        b.setDirty(false);
        return true;
    }

    bool openFile(const std::string& path, Buf& b)
    {
        std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
        if (!in.is_open()) return false;
        std::ostringstream ss;
        ss << in.rdbuf();
        b.loadFromString(ss.str());
        b.setFilePath(path);
        b.setDirty(false);
        return true;
    }

    bool saveFile(const std::string& path, Buf& b)
    {
        std::ofstream out(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        std::string content = b.saveToString();
        out.write(content.c_str(), content.size());
        out.close();
        b.setFilePath(path);
        b.setDirty(false);
        return true;
    }
};

// ============================================================================
//  D 同学：编译调度（占位：system + 重定向；真实实现请用 CreateProcess + 管道）
// ============================================================================
class MiniCompiler
{
public:
    bool available()
    {
        // 调试用：设置环境变量 MINIC_FORCE_NO_COMPILER=1 可强制走"未找到编译器"分支（T8.7 测试）
        if (std::getenv("MINIC_FORCE_NO_COMPILER")) return false;
        int code = system("gcc --version > nul 2>&1");
        return code == 0;
    }

    CompileResult compile(const std::string& srcPath)
    {
        CompileResult r;
        r.state = CS_NONE;
        r.exePath = exePathOf(srcPath);

        if (srcPath.empty()) { r.state = CS_INTERNAL; r.raw = "[错误] 尚未指定源文件路径。"; return r; }
        if (!available())
        {
            r.state = CS_NOCOMPILER;
            r.raw = "[错误] 未检测到 gcc 编译器。\n"
                    "请安装 MinGW-w64 / TDM-GCC，并把 gcc.exe 所在目录加入 PATH 后重启本程序。";
            return r;
        }

        std::string errFile = "minic_compile.log";
        std::string cmd = "gcc \"" + srcPath + "\" -o \"" + r.exePath + "\" -Wall -std=c11 2>\"" + errFile + "\"";
        int code = system(cmd.c_str());
        r.raw = readAll(errFile);
        ::DeleteFileA("minic_compile.log");

        if (code == 0)
        {
            r.items = parseDiagnostics(r.raw);
            if (r.items.empty()) { r.state = CS_OK; r.raw = "编译成功：0 个错误，0 个警告。\n可执行文件：" + r.exePath; }
            else                 { r.state = CS_WARNING; }
        }
        else
        {
            r.state = CS_ERROR;
            r.items = parseDiagnostics(r.raw);
            if (r.items.empty() && r.raw.empty())
                r.raw = "[错误] 编译器返回码 " + toStr(code) + "，但未捕获到输出。";
        }
        return r;
    }

    std::string exePathOf(const std::string& srcPath)
    {
        std::string base = srcPath;
        size_t p = base.find_last_of("\\/");
        std::string dir, name = base;
        if (p != std::string::npos) { dir = base.substr(0, p + 1); name = base.substr(p + 1); }
        size_t d = name.find_last_of('.');
        if (d != std::string::npos) name = name.substr(0, d);
        return dir + name + ".exe";
    }

private:
    static std::string toStr(int v)
    {
        char buf[32]; sprintf_s(buf, "%d", v); return buf;
    }
    static std::string readAll(const std::string& f)
    {
        std::ifstream in(f.c_str(), std::ios::in | std::ios::binary);
        if (!in.is_open()) return "";
        std::ostringstream ss; ss << in.rdbuf(); return ss.str();
    }
    static bool allDigits(const std::string& s)
    {
        if (s.empty()) return false;
        for (size_t i = 0; i < s.size(); i++) if (s[i] < '0' || s[i] > '9') return false;
        return true;
    }
    // 解析 gcc 输出： main.c:12:5: error: xxx
    static std::vector<Diagnostic> parseDiagnostics(const std::string& raw)
    {
        std::vector<Diagnostic> out;
        std::istringstream is(raw);
        std::string line;
        while (std::getline(is, line))
        {
            if (line.size() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
            DiagLevel lv; std::string tag;
            size_t pos = std::string::npos;
            size_t pe = line.find(": error:");
            size_t pw = line.find(": warning:");
            size_t pn = line.find(": note:");
            if (pe != std::string::npos)   { pos = pe;   lv = DIAG_ERROR;   tag = "error"; }
            else if (pw != std::string::npos) { pos = pw; lv = DIAG_WARNING; tag = "warning"; }
            else if (pn != std::string::npos) { pos = pn; lv = DIAG_INFO;    tag = "note"; }
            else continue;

            std::string head = line.substr(0, pos);
            std::string msg  = line.substr(pos + 2);          // "error: xxx"
            size_t colon = msg.find(':');
            if (colon != std::string::npos) msg = msg.substr(colon + 1);
            while (!msg.empty() && msg[0] == ' ') msg.erase(0, 1);

            int ln = -1, col = -1;
            size_t p1 = head.rfind(':');
            if (p1 != std::string::npos && p1 > 0)
            {
                size_t p2 = head.rfind(':', p1 - 1);
                if (p2 != std::string::npos)
                {
                    std::string a = head.substr(p2 + 1, p1 - p2 - 1);
                    std::string b = head.substr(p1 + 1);
                    if (allDigits(a) && allDigits(b)) { ln = atoi(a.c_str()); col = atoi(b.c_str()); }
                }
            }
            Diagnostic d;
            d.line   = (ln > 0) ? ln - 1 : 0;      // 转成 0-based
            d.column = (col > 0) ? col - 1 : 0;
            d.level  = lv;
            d.message = msg;
            d.tag     = tag;
            out.push_back(d);
        }
        return out;
    }
};

// ============================================================================
//  D 同学：程序运行时托管（占位：同步 system；真实实现请用管道 + 非阻塞读取）
// ============================================================================
class MiniRuntime
{
public:
    MiniRuntime() : m_input(), m_out(), m_code(0), m_running(false), m_done(false) {}

    bool start(const std::string& exePath)
    {
        m_out.clear(); m_code = 0; m_done = false; m_running = true;
        std::string inFile = "minic_stdin.txt", outFile = "minic_stdout.txt";
        {
            std::ofstream f(inFile.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
            for (size_t i = 0; i < m_input.size(); i++) f << m_input[i] << "\n";
        }
        // 修复 T9.1：cmd /c 在命令以引号开头且引号数>2时会剥离首尾引号，导致路径解析失败。
        // 用 cmd /S /c 让外层 cmd 不剥引号；< 和 > 留在内层引号外由外层 cmd 解析为重定向，
        // 内层 cmd 继承 stdin/stdout 重定向后运行真正的 exe。
        std::string cmd = "cmd /S /c \"" + exePath + "\" < \"" + inFile + "\" > \"" + outFile + "\" 2>&1";
        int code = system(cmd.c_str());
        m_code = code;
        m_out = readAll(outFile);
        m_done = true;
        m_running = false;
        m_input.clear();
        return true;
    }

    // 非阻塞轮询：返回 true 表示本次取到了新数据 / 状态变化
    bool poll(std::string& out, int& exitCode, bool& finished)
    {
        if (!m_done) return false;
        out = m_out; exitCode = m_code; finished = true;
        m_out.clear(); m_done = false;
        return true;
    }

    void sendInput(const std::string& line) { m_input.push_back(line); }
    void stop()    { m_running = false; m_done = true; m_out += "\n[已被用户终止]"; }
    bool isRunning() const { return m_running; }

private:
    std::vector<std::string> m_input;
    std::string m_out;
    int  m_code;
    bool m_running, m_done;

    static std::string readAll(const std::string& f)
    {
        std::ifstream in(f.c_str(), std::ios::in | std::ios::binary);
        if (!in.is_open()) return "";
        std::ostringstream ss; ss << in.rdbuf(); return ss.str();
    }
};

#endif // MINIC_STUB_H
