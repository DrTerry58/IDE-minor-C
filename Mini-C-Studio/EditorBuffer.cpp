// ============================================================================
//  EditorBuffer.cpp
//  文本缓冲区模块实现（C++11，不含任何 GUI 依赖）
// ============================================================================

#include "EditorBuffer.h"

#include <algorithm>

namespace
{
    // 撤销栈的最大深度，超出后丢弃最早的快照，避免长时间编辑占用过多内存
    const size_t kMaxUndoDepth = 200;

    // 自动缩进的单位：每次缩进的空格数
    const size_t kIndentSize = 4;

    // 判断字符是否是左括号
    bool isOpenBracket(char c)
    {
        return c == '(' || c == '[' || c == '{';
    }

    // 判断字符是否是右括号
    bool isCloseBracket(char c)
    {
        return c == ')' || c == ']' || c == '}';
    }

    // 取括号的配对字符；不是括号返回 '\0'
    char matchOf(char c)
    {
        switch (c)
        {
        case '(': return ')';
        case ')': return '(';
        case '[': return ']';
        case ']': return '[';
        case '{': return '}';
        case '}': return '{';
        default:  return '\0';
        }
    }

    // 返回字符串开头的空白（空格 / 制表符）长度；非空字符或全空则返回 0
    size_t leadingWhitespace(const std::string& s)
    {
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        {
            ++i;
        }
        return i;
    }
}

// ----------------------------------------------------------------------------
// 构造：初始化为 1 行空行，光标 (0,0)，脏标记 false，无选中
// ----------------------------------------------------------------------------
EditorBuffer::EditorBuffer()
    : lines(),
      cursorX(0),
      cursorY(0),
      filePath(),
      dirty(false),
      selectionActive(false),
      selStartX(0), selStartY(0),
      selEndX(0), selEndY(0),
      undoStack(),
      redoStack(),
      suppressUndo(false),
      mergeRow(-1)
{
    lines.push_back(std::string());
}

// ----------------------------------------------------------------------------
// 析构：只持有标准库容器，自动释放，无需手工处理
// ----------------------------------------------------------------------------
EditorBuffer::~EditorBuffer()
{
}

// ============================================================================
//  私有辅助函数
// ============================================================================

// ----------------------------------------------------------------------------
// 保证缓冲区至少有 1 行，并把光标修正到合法范围内。
// 这是所有“光标不越界”要求的统一保障点，每个用到光标的操作都先调用它
// ----------------------------------------------------------------------------
void EditorBuffer::clampCursor()
{
    if (lines.empty())
    {
        lines.push_back(std::string());
    }

    if (cursorY < 0)
    {
        cursorY = 0;
    }
    if (cursorY >= static_cast<int>(lines.size()))
    {
        cursorY = static_cast<int>(lines.size()) - 1;
    }

    if (cursorX < 0)
    {
        cursorX = 0;
    }
    const int lineLen = static_cast<int>(lines[cursorY].size());
    if (cursorX > lineLen)
    {
        cursorX = lineLen;  // 允许等于行长，表示光标停在行尾
    }
}

// ----------------------------------------------------------------------------
// 判断行号是否合法
// ----------------------------------------------------------------------------
bool EditorBuffer::isValidRow(int row) const
{
    return row >= 0 && row < static_cast<int>(lines.size());
}

// ----------------------------------------------------------------------------
// 把选区坐标修正到合法范围并重新规范化。
// 内容被编辑后旧选区可能指向已不存在的位置，用到选区前先调用它
// ----------------------------------------------------------------------------
void EditorBuffer::clampSelection()
{
    if (!selectionActive)
    {
        return;
    }

    if (lines.empty())
    {
        lines.push_back(std::string());
    }

    const int lastRow = static_cast<int>(lines.size()) - 1;

    selStartY = std::max(0, std::min(selStartY, lastRow));
    selEndY   = std::max(0, std::min(selEndY,   lastRow));

    selStartX = std::max(0, std::min(selStartX, static_cast<int>(lines[selStartY].size())));
    selEndX   = std::max(0, std::min(selEndX,   static_cast<int>(lines[selEndY].size())));

    // 规范化：保证 (selStartY, selStartX) <= (selEndY, selEndX)
    if (selStartY > selEndY || (selStartY == selEndY && selStartX > selEndX))
    {
        std::swap(selStartX, selEndX);
        std::swap(selStartY, selEndY);
    }

    // 修正后可能退化为空选区
    if (selStartY == selEndY && selStartX == selEndX)
    {
        selectionActive = false;
    }
}

// ----------------------------------------------------------------------------
// 取当前状态的快照（内容 + 光标位置）
// ----------------------------------------------------------------------------
EditorBuffer::Snapshot EditorBuffer::makeSnapshot() const
{
    Snapshot snap;
    snap.lines = lines;
    snap.cursorX = cursorX;
    snap.cursorY = cursorY;
    return snap;
}

// ----------------------------------------------------------------------------
// 用快照覆盖当前状态
// ----------------------------------------------------------------------------
void EditorBuffer::applySnapshot(const Snapshot& snap)
{
    lines = snap.lines;
    cursorX = snap.cursorX;
    cursorY = snap.cursorY;
    clampCursor();
}

// ----------------------------------------------------------------------------
// 记录当前状态到撤销栈，并清空重做栈。
// suppressUndo 为 true 时不压栈，用于 pasteText 这类内部调用多步操作的场景，
// 保证一次用户操作只对应一个快照
// ----------------------------------------------------------------------------
void EditorBuffer::pushUndoSnapshot()
{
    if (suppressUndo)
    {
        return;
    }

    undoStack.push_back(makeSnapshot());
    if (undoStack.size() > kMaxUndoDepth)
    {
        undoStack.erase(undoStack.begin());
    }
    redoStack.clear();
}

// ----------------------------------------------------------------------------
// 按 '\n' 拆分文本，并去掉行尾多余的 '\r'（兼容 Windows 换行）。
// 例："a\nb" -> {"a","b"}；"a\n" -> {"a",""}；"" -> {""}
// ----------------------------------------------------------------------------
std::vector<std::string> EditorBuffer::splitLines(const std::string& text)
{
    std::vector<std::string> result;
    std::string cur;

    for (size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (c == '\n')
        {
            if (!cur.empty() && cur[cur.size() - 1] == '\r')
            {
                cur.erase(cur.size() - 1);
            }
            result.push_back(cur);
            cur.clear();
        }
        else
        {
            cur.push_back(c);
        }
    }

    if (!cur.empty() && cur[cur.size() - 1] == '\r')
    {
        cur.erase(cur.size() - 1);
    }
    result.push_back(cur);  // 最后一段（可能为空）也要作为一行

    return result;
}

// ============================================================================
//  编辑操作
// ============================================================================

// ----------------------------------------------------------------------------
// 在光标位置插入一个字符；ch 为 '\n' 时等价于换行
// ----------------------------------------------------------------------------
void EditorBuffer::insertChar(char ch)
{
    if (ch == '\n')
    {
        enter();
        return;
    }

    clampCursor();

    // 同一行上连续插入字符时合并成一个撤销单元，只压一次栈
    if (mergeRow != cursorY)
    {
        pushUndoSnapshot();
        mergeRow = cursorY;
    }

    if (ch == '\t')
    {
        const std::string spaces(4, ' ');
        lines[cursorY].insert(static_cast<size_t>(cursorX), spaces);
        cursorX += 4;
    }
    else if (ch == '}')
    {
        // 光标在一行行首空白处（尚未输入任何代码）且空白 >= 一级缩进时，输入 '}' 回退一级：
        // 先把行首那一级缩进的 4 个空格删掉，再插入 '}'
        if (cursorX >= static_cast<int>(kIndentSize)
            && lines[cursorY].find_first_not_of(" \t") >= static_cast<size_t>(cursorX))
        {
            cursorX -= static_cast<int>(kIndentSize);
            lines[cursorY].erase(static_cast<size_t>(cursorX), kIndentSize);
        }
        lines[cursorY].insert(static_cast<size_t>(cursorX), 1, ch);
        ++cursorX;
    }
    else
    {
        lines[cursorY].insert(static_cast<size_t>(cursorX), 1, ch);
        ++cursorX;
    }
    dirty = true;
}

// ----------------------------------------------------------------------------
// 退格键：删除光标前一个字符；行首则把本行合并到上一行；
// 第一行行首时不做任何事（不修改内容、不置脏标记）
// ----------------------------------------------------------------------------
void EditorBuffer::deleteChar()
{
    clampCursor();

    if (cursorX > 0)
    {
        pushUndoSnapshot();
        mergeRow = -1;
        lines[cursorY].erase(static_cast<size_t>(cursorX - 1), 1);
        --cursorX;
        dirty = true;
    }
    else if (cursorY > 0)
    {
        pushUndoSnapshot();
        mergeRow = -1;
        const int prevLen = static_cast<int>(lines[cursorY - 1].size());
        lines[cursorY - 1] += lines[cursorY];
        lines.erase(lines.begin() + cursorY);
        --cursorY;
        cursorX = prevLen;  // 光标落在合并处
        dirty = true;
    }
    // else: 已在文首，无操作
}

// ----------------------------------------------------------------------------
// Delete 键：删除光标后一个字符；行尾则把下一行合并上来；
// 最后一行行尾时不做任何事
// ----------------------------------------------------------------------------
void EditorBuffer::deleteForward()
{
    clampCursor();

    const int lineLen = static_cast<int>(lines[cursorY].size());

    if (cursorX < lineLen)
    {
        pushUndoSnapshot();
        mergeRow = -1;
        lines[cursorY].erase(static_cast<size_t>(cursorX), 1);
        dirty = true;
    }
    else if (cursorY + 1 < static_cast<int>(lines.size()))
    {
        pushUndoSnapshot();
        mergeRow = -1;
        lines[cursorY] += lines[cursorY + 1];
        lines.erase(lines.begin() + (cursorY + 1));
        dirty = true;
    }
    // else: 已在文末，无操作
}

// ----------------------------------------------------------------------------
// 回车换行：把光标所在行按光标列拆成两行，光标移到新行行首
// ----------------------------------------------------------------------------
void EditorBuffer::enter()
{
    clampCursor();
    pushUndoSnapshot();
    mergeRow = -1;

    // 自动缩进：新行继承当前行行首的空白；若光标前紧挨着 '{' 则再加深一级
    const std::string prefix = lines[cursorY].substr(0, static_cast<size_t>(cursorX));
    std::string indent(lines[cursorY].begin(),
                       lines[cursorY].begin() +
                       static_cast<std::ptrdiff_t>(leadingWhitespace(lines[cursorY])));
    if (!prefix.empty() && prefix[prefix.size() - 1] == '{')
    {
        indent.append(kIndentSize, ' ');
    }

    const std::string tail = lines[cursorY].substr(static_cast<size_t>(cursorX));
    lines[cursorY].erase(static_cast<size_t>(cursorX));
    lines.insert(lines.begin() + (cursorY + 1), indent + tail);

    ++cursorY;
    cursorX = static_cast<int>(indent.size());
    dirty = true;
}

// ============================================================================
//  光标操作
// ============================================================================

// ----------------------------------------------------------------------------
// 相对移动光标；结果自动修正到合法范围（上下移动后若新行更短，列号被拉回行尾）
// ----------------------------------------------------------------------------
void EditorBuffer::moveCursor(int dx, int dy)
{
    clampCursor();
    mergeRow = -1;
    cursorX += dx;
    cursorY += dy;
    clampCursor();
}

// ----------------------------------------------------------------------------
// 绝对定位光标；越界时修正到最近的合法位置
// ----------------------------------------------------------------------------
void EditorBuffer::setCursor(int x, int y)
{
    mergeRow = -1;
    cursorX = x;
    cursorY = y;
    clampCursor();
}

// ----------------------------------------------------------------------------
// 取光标列号
// ----------------------------------------------------------------------------
int EditorBuffer::getCursorX() const
{
    return cursorX;
}

// ----------------------------------------------------------------------------
// 取光标行号
// ----------------------------------------------------------------------------
int EditorBuffer::getCursorY() const
{
    return cursorY;
}

// ----------------------------------------------------------------------------
// 取总行数（至少为 1）
// ----------------------------------------------------------------------------
int EditorBuffer::getLineCount() const
{
    return static_cast<int>(lines.size());
}

// ----------------------------------------------------------------------------
// 取第 row 行的字符数；row 越界返回 -1
// ----------------------------------------------------------------------------
int EditorBuffer::getLineLength(int row) const
{
    if (!isValidRow(row))
    {
        return -1;
    }
    return static_cast<int>(lines[row].size());
}

// ----------------------------------------------------------------------------
// 取第 row 行的内容；row 越界返回空串
// ----------------------------------------------------------------------------
std::string EditorBuffer::getLine(int row) const
{
    if (!isValidRow(row))
    {
        return std::string();
    }
    return lines[row];
}

// ----------------------------------------------------------------------------
// 取第 row 行内容的引用；row 越界返回静态空串引用（不拷贝）
// ----------------------------------------------------------------------------
const std::string& EditorBuffer::getLineRef(int row) const
{
    static const std::string empty;
    if (!isValidRow(row))
    {
        return empty;
    }
    return lines[row];
}

// ============================================================================
//  文件 IO 辅助
// ============================================================================

// ----------------------------------------------------------------------------
// 把整篇文本拆分成行装入缓冲区。
// 空内容时保留 1 行空行；重置光标、选中和撤销栈；脏标记置 false
// ----------------------------------------------------------------------------
void EditorBuffer::loadFromString(const std::string& content)
{
    lines = splitLines(content);
    if (lines.empty())
    {
        lines.push_back(std::string());  // 保证至少 1 行空行
    }

    cursorX = 0;
    cursorY = 0;
    clearSelection();
    undoStack.clear();
    redoStack.clear();
    suppressUndo = false;
    mergeRow = -1;
    dirty = false;
    clampCursor();
}

// ----------------------------------------------------------------------------
// 把缓冲区拼成整篇文本，行间用 '\n' 连接，最后一行末尾不加换行符
// ----------------------------------------------------------------------------
std::string EditorBuffer::saveToString() const
{
    std::string result;

    // 预估容量，减少多次扩容
    size_t total = 0;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        total += lines[i].size() + 1;
    }
    result.reserve(total);

    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (i > 0)
        {
            result.push_back('\n');
        }
        result += lines[i];
    }

    return result;
}

// ----------------------------------------------------------------------------
// 设置脏标记
// ----------------------------------------------------------------------------
void EditorBuffer::setDirty(bool dirtyFlag)
{
    dirty = dirtyFlag;
}

// ----------------------------------------------------------------------------
// 查询是否有未保存的修改
// ----------------------------------------------------------------------------
bool EditorBuffer::isDirty() const
{
    return dirty;
}

// ----------------------------------------------------------------------------
// 设置当前文件路径
// ----------------------------------------------------------------------------
void EditorBuffer::setFilePath(const std::string& path)
{
    filePath = path;
}

// ----------------------------------------------------------------------------
// 取当前文件路径
// ----------------------------------------------------------------------------
std::string EditorBuffer::getFilePath() const
{
    return filePath;
}

// ============================================================================
//  选中与剪贴板
// ============================================================================

// ----------------------------------------------------------------------------
// 设置选中区域；内部规范化为“起点 <= 终点”并修正越界坐标；
// 空选区（起点与终点重合）视为没有选中
// ----------------------------------------------------------------------------
void EditorBuffer::setSelection(int startX, int startY, int endX, int endY)
{
    selStartX = startX;
    selStartY = startY;
    selEndX = endX;
    selEndY = endY;
    selectionActive = true;

    clampSelection();  // 在这里完成越界修正、排序和空选区判定
}

// ----------------------------------------------------------------------------
// 取消选中
// ----------------------------------------------------------------------------
void EditorBuffer::clearSelection()
{
    selectionActive = false;
    selStartX = 0;
    selStartY = 0;
    selEndX = 0;
    selEndY = 0;
}

// ----------------------------------------------------------------------------
// 是否存在非空选中区域
// ----------------------------------------------------------------------------
bool EditorBuffer::hasSelection() const
{
    return selectionActive;
}

// ----------------------------------------------------------------------------
// 取规范化后的选区起点/终点坐标；无选中返回 false 且不改参数
// ----------------------------------------------------------------------------
bool EditorBuffer::getSelectionRange(int& outStartX, int& outStartY,
                                     int& outEndX, int& outEndY) const
{
    if (!selectionActive)
    {
        return false;
    }
    outStartX = selStartX;
    outStartY = selStartY;
    outEndX = selEndX;
    outEndY = selEndY;
    return true;
}

// ----------------------------------------------------------------------------
// 取选中的文本；跨行时行间用 '\n' 连接；无选中返回空串
// ----------------------------------------------------------------------------
std::string EditorBuffer::getSelectedText() const
{
    if (!selectionActive)
    {
        return std::string();
    }

    // 本函数是 const，不能改成员，故在局部变量里做一次范围修正
    const int lastRow = static_cast<int>(lines.size()) - 1;
    int sy = std::max(0, std::min(selStartY, lastRow));
    int ey = std::max(0, std::min(selEndY, lastRow));
    int sx = std::max(0, std::min(selStartX, static_cast<int>(lines[sy].size())));
    int ex = std::max(0, std::min(selEndX, static_cast<int>(lines[ey].size())));

    if (sy > ey || (sy == ey && sx > ex))
    {
        std::swap(sx, ex);
        std::swap(sy, ey);
    }

    if (sy == ey)
    {
        return lines[sy].substr(static_cast<size_t>(sx), static_cast<size_t>(ex - sx));
    }

    std::string result = lines[sy].substr(static_cast<size_t>(sx));  // 首行：从 sx 到行尾
    for (int row = sy + 1; row < ey; ++row)
    {
        result.push_back('\n');
        result += lines[row];                                        // 中间行：整行
    }
    result.push_back('\n');
    result += lines[ey].substr(0, static_cast<size_t>(ex));          // 末行：行首到 ex

    return result;
}

// ----------------------------------------------------------------------------
// 删除选中的文本；删除后光标落在原选区起点并取消选中；无选中则不做任何事
// ----------------------------------------------------------------------------
void EditorBuffer::deleteSelection()
{
    clampSelection();
    if (!selectionActive)
    {
        return;
    }

    pushUndoSnapshot();

    if (selStartY == selEndY)
    {
        lines[selStartY].erase(static_cast<size_t>(selStartX),
                               static_cast<size_t>(selEndX - selStartX));
    }
    else
    {
        // 首行保留 [0, selStartX)，接上末行的 [selEndX, 末尾)，中间各行整体删除
        lines[selStartY] = lines[selStartY].substr(0, static_cast<size_t>(selStartX))
                         + lines[selEndY].substr(static_cast<size_t>(selEndX));
        lines.erase(lines.begin() + (selStartY + 1),
                    lines.begin() + (selEndY + 1));
    }

    cursorX = selStartX;
    cursorY = selStartY;
    clearSelection();
    mergeRow = -1;
    clampCursor();
    dirty = true;
}

// ----------------------------------------------------------------------------
// 在光标处粘贴文本，支持多行；粘贴后光标停在插入内容的末尾。
// 不会自动删除已有选中内容（需要“替换选中”请先调用 deleteSelection）
// ----------------------------------------------------------------------------
void EditorBuffer::pasteText(const std::string& text)
{
    if (text.empty())
    {
        return;
    }

    clampCursor();
    pushUndoSnapshot();
    mergeRow = -1;

    // 整个粘贴过程只记一个快照，撤销时一次性回退
    const bool oldSuppress = suppressUndo;
    suppressUndo = true;

    const std::vector<std::string> parts = splitLines(text);

    if (parts.size() == 1)
    {
        lines[cursorY].insert(static_cast<size_t>(cursorX), parts[0]);
        cursorX += static_cast<int>(parts[0].size());
    }
    else
    {
        // 先把光标后的内容切下来，最后接到粘贴内容的末尾
        const std::string tail = lines[cursorY].substr(static_cast<size_t>(cursorX));
        lines[cursorY].erase(static_cast<size_t>(cursorX));
        lines[cursorY] += parts[0];

        for (size_t i = 1; i < parts.size(); ++i)
        {
            lines.insert(lines.begin() + (cursorY + static_cast<int>(i)), parts[i]);
        }

        const int lastIndex = cursorY + static_cast<int>(parts.size()) - 1;
        cursorY = lastIndex;
        cursorX = static_cast<int>(lines[lastIndex].size());
        lines[lastIndex] += tail;
    }

    suppressUndo = oldSuppress;
    clampCursor();
    dirty = true;
}

// ============================================================================
//  加分项：撤销 / 重做
// ============================================================================

// ----------------------------------------------------------------------------
// 撤销上一步编辑操作
// ----------------------------------------------------------------------------
void EditorBuffer::undo()
{
    if (undoStack.empty())
    {
        return;
    }

    redoStack.push_back(makeSnapshot());       // 当前状态存入重做栈
    applySnapshot(undoStack.back());           // 回退到上一个快照
    undoStack.pop_back();

    clearSelection();
    mergeRow = -1;
    dirty = true;  // 撤销后内容与磁盘上的文件仍可能不一致，保守地标记为脏
}

// ----------------------------------------------------------------------------
// 重做被撤销的操作
// ----------------------------------------------------------------------------
void EditorBuffer::redo()
{
    if (redoStack.empty())
    {
        return;
    }

    undoStack.push_back(makeSnapshot());
    applySnapshot(redoStack.back());
    redoStack.pop_back();

    clearSelection();
    mergeRow = -1;
    dirty = true;
}

// ----------------------------------------------------------------------------
// 是否还有可撤销的操作
// ----------------------------------------------------------------------------
bool EditorBuffer::canUndo() const
{
    return !undoStack.empty();
}

// ----------------------------------------------------------------------------
// 是否还有可重做的操作
// ----------------------------------------------------------------------------
bool EditorBuffer::canRedo() const
{
    return !redoStack.empty();
}

// ============================================================================
//  加分项：括号配对与代码折叠
// ============================================================================

// ----------------------------------------------------------------------------
// 查找 (posX, posY) 处括号的配对括号。
// 左括号向后扫描、右括号向前扫描，用深度计数找到同层的配对括号。
// 只统计同类型括号的深度，不识别字符串和注释中的括号
// ----------------------------------------------------------------------------
bool EditorBuffer::findMatchingBracket(int posX, int posY, int& outX, int& outY) const
{
    if (!isValidRow(posY))
    {
        return false;
    }
    if (posX < 0 || posX >= static_cast<int>(lines[posY].size()))
    {
        return false;
    }

    const char here = lines[posY][posX];
    const char want = matchOf(here);
    if (want == '\0')
    {
        return false;  // 该位置不是括号
    }

    int depth = 0;

    if (isOpenBracket(here))
    {
        // 向后（下方 / 右方）扫描
        for (int row = posY; row < static_cast<int>(lines.size()); ++row)
        {
            const std::string& line = lines[row];
            const int startCol = (row == posY) ? posX : 0;
            for (int col = startCol; col < static_cast<int>(line.size()); ++col)
            {
                const char c = line[col];
                if (c == here)
                {
                    ++depth;
                }
                else if (c == want)
                {
                    --depth;
                    if (depth == 0)
                    {
                        outX = col;
                        outY = row;
                        return true;
                    }
                }
            }
        }
    }
    else if (isCloseBracket(here))
    {
        // 向前（上方 / 左方）扫描
        for (int row = posY; row >= 0; --row)
        {
            const std::string& line = lines[row];
            const int startCol = (row == posY) ? posX : static_cast<int>(line.size()) - 1;
            for (int col = startCol; col >= 0; --col)
            {
                const char c = line[col];
                if (c == here)
                {
                    ++depth;
                }
                else if (c == want)
                {
                    --depth;
                    if (depth == 0)
                    {
                        outX = col;
                        outY = row;
                        return true;
                    }
                }
            }
        }
    }

    return false;  // 括号不匹配
}

// ----------------------------------------------------------------------------
// 返回所有可折叠区域，每项为 (起始行号, 结束行号)，以 '{' 与 '}' 配对为依据，
// 只返回跨越多行的区域。扫描时跳过 // 注释、/* */ 注释、字符串和字符常量，
// 避免把注释或字符串里的花括号当成代码块
// ----------------------------------------------------------------------------
std::vector<std::pair<int, int> > EditorBuffer::getFoldableRegions() const
{
    std::vector<std::pair<int, int> > regions;
    std::vector<int> stack;       // 存放尚未闭合的 '{' 所在行号

    bool inBlockComment = false;  // 是否处于 /* */ 之中（可跨行）

    for (int row = 0; row < static_cast<int>(lines.size()); ++row)
    {
        const std::string& line = lines[row];
        bool inString = false;    // 是否处于 "..." 之中
        bool inChar = false;      // 是否处于 '...' 之中

        for (size_t col = 0; col < line.size(); ++col)
        {
            const char c = line[col];

            if (inBlockComment)
            {
                if (c == '*' && col + 1 < line.size() && line[col + 1] == '/')
                {
                    inBlockComment = false;
                    ++col;
                }
                continue;
            }

            if (inString)
            {
                if (c == '\\')
                {
                    ++col;             // 跳过转义字符
                }
                else if (c == '"')
                {
                    inString = false;
                }
                continue;
            }

            if (inChar)
            {
                if (c == '\\')
                {
                    ++col;
                }
                else if (c == '\'')
                {
                    inChar = false;
                }
                continue;
            }

            // 以下为普通代码状态
            if (c == '/' && col + 1 < line.size() && line[col + 1] == '/')
            {
                break;                 // 行注释，本行剩余部分忽略
            }
            if (c == '/' && col + 1 < line.size() && line[col + 1] == '*')
            {
                inBlockComment = true;
                ++col;
                continue;
            }
            if (c == '"')
            {
                inString = true;
                continue;
            }
            if (c == '\'')
            {
                inChar = true;
                continue;
            }

            if (c == '{')
            {
                stack.push_back(row);
            }
            else if (c == '}')
            {
                if (!stack.empty())
                {
                    const int startRow = stack.back();
                    stack.pop_back();
                    if (row > startRow)    // 只收跨行的区域，单行 {} 不折叠
                    {
                        regions.push_back(std::make_pair(startRow, row));
                    }
                }
                // 多余的 '}' 直接忽略，保证不崩溃
            }
        }
    }

    // 按起始行号升序排列，方便界面同学顺序绘制折叠标记
    std::sort(regions.begin(), regions.end());
    return regions;
}
