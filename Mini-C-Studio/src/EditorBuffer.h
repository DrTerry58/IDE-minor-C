// ============================================================================
//  EditorBuffer.h
//  C 语言集成开发环境 —— 文本缓冲区模块（B 同学负责）
//
//  说明：
//    1. 本模块是编辑器的数据核心，不含任何界面代码，可独立编译。
//    2. 不包含任何 GUI 头文件，不依赖其他小组成员的模块。
//    3. C++11 标准。
//
//  与组长 UML 的两点差异（已尽量保持对外接口不变）：
//    - UML 中数据成员名为 isDirty，但方法也叫 isDirty()，C++ 中同名会编译失败。
//      故成员改名为 dirty，对外的 isDirty() / setDirty() 接口不变。
//    - 选中区域、撤销/重做需要额外的私有状态，UML 未列出，见 private 段。
// ============================================================================

#ifndef EDITOR_BUFFER_H
#define EDITOR_BUFFER_H

#include <string>
#include <vector>
#include <utility>

class EditorBuffer
{
public:
    // ---------------- 构造与析构 ----------------

    // 构造：初始化为 1 行空行，光标 (0,0)，脏标记 false，无选中
    EditorBuffer();

    // 析构：本类只持有标准库容器，无需手工释放资源
    ~EditorBuffer();

    // ---------------- 编辑操作 ----------------

    // 在光标位置插入字符；ch 为 '\n' 时等价于换行，ch 为 '\t' 时插入 4 个空格
    void insertChar(char ch);

    // 退格键：删除光标前一个字符；光标在行首时把本行合并到上一行；
    // 若已在第一行行首则不做任何事
    void deleteChar();

    // Delete 键：删除光标后一个字符；光标在行尾时把下一行合并上来；
    // 若已在最后一行行尾则不做任何事
    void deleteForward();

    // 回车换行：把光标所在行按光标列拆成两行，光标移到新行行首
    void enter();

    // ---------------- 光标操作 ----------------

    // 相对移动光标：dx 为列偏移，dy 为行偏移；移动后自动修正到合法范围
    void moveCursor(int dx, int dy);

    // 绝对定位光标；越界时自动修正到最近的合法位置
    void setCursor(int x, int y);

    // 取光标列号（从 0 开始）
    int getCursorX() const;

    // 取光标行号（从 0 开始）
    int getCursorY() const;

    // 取总行数；缓冲区任何时候至少有 1 行，故返回值 >= 1
    int getLineCount() const;

    // 取第 row 行的字符数（不含换行符）；row 越界返回 -1
    int getLineLength(int row) const;

    // 取第 row 行的内容（不含换行符）；row 越界返回空串
    std::string getLine(int row) const;

    // 取第 row 行的内容的引用，避免拷贝；row 越界返回一个静态空串的引用
    const std::string& getLineRef(int row) const;

    // ---------------- 文件 IO 辅助 ----------------

    // 把整篇文本按换行符拆分后装入缓冲区（兼容 "\r\n" 和 "\n" 两种换行）。
    // 内容为空时保留 1 行空行；同时重置光标到 (0,0)、清空选中、清空撤销栈，
    // 并把脏标记置为 false（表示“刚从文件读入，尚未修改”）
    void loadFromString(const std::string& content);

    // 把缓冲区拼成一整篇文本，行间用 '\n' 连接，最后一行末尾不加换行符
    std::string saveToString() const;

    // 设置脏标记（供文件管理同学在保存成功后置 false）
    void setDirty(bool dirty);

    // 查询是否有未保存的修改
    bool isDirty() const;

    // 设置当前文件路径；空串表示未命名
    void setFilePath(const std::string& path);

    // 取当前文件路径；空串表示未命名
    std::string getFilePath() const;

    // ---------------- 选中与剪贴板 ----------------

    // 设置选中区域；内部会规范化为“起点 <= 终点”并修正越界坐标。
    // 若规范化后起点与终点重合（空选区），则视为“没有选中”
    void setSelection(int startX, int startY, int endX, int endY);

    // 取消选中
    void clearSelection();

    // 是否存在非空选中区域
    bool hasSelection() const;

    // 取规范化后的选区起点/终点坐标（写入四个 out 参数；无选中返回 false 且不改参数）
    bool getSelectionRange(int& outStartX, int& outStartY, int& outEndX, int& outEndY) const;

    // 取选中的文本；跨行时行间用 '\n' 连接；无选中返回空串
    std::string getSelectedText() const;

    // 删除选中的文本，删除后光标落在原选区起点，并取消选中；无选中则不做任何事
    void deleteSelection();

    // 在光标处粘贴文本，支持多行；粘贴后光标停在插入内容的末尾。
    // 注意：本函数不会自动删除已有选中内容，界面同学若需“粘贴替换选中”，
    // 请先调用 deleteSelection() 再调用本函数
    void pasteText(const std::string& text);

    // ---------------- 加分项 ----------------

    // 撤销上一步编辑操作（基于快照栈实现）
    void undo();

    // 重做被撤销的操作；一旦发生新的编辑，重做栈会被清空
    void redo();

    // 是否还有可撤销的操作
    bool canUndo() const;

    // 是否还有可重做的操作
    bool canRedo() const;

    // 查找 (posX, posY) 处括号的配对括号，支持 () [] {}。
    // 找到则把配对位置写入 outX / outY 并返回 true；
    // 该位置不是括号、或括号不匹配时返回 false。
    // 注意：只做简单的深度计数，不识别字符串和注释里的括号
    bool findMatchingBracket(int posX, int posY, int& outX, int& outY) const;

    // 返回所有可折叠区域，每项为 (起始行号, 结束行号)，以 '{' 与 '}' 配对为依据，
    // 只返回跨越多行的区域。扫描时会跳过 // 注释、/* */ 注释、字符串和字符常量
    std::vector<std::pair<int, int> > getFoldableRegions() const;

private:
    // ---------------- 数据成员（UML 规定部分） ----------------

    std::vector<std::string> lines;  // 每行存一个字符串，不含换行符
    int cursorX;                     // 光标列号，从 0 开始
    int cursorY;                     // 光标行号，从 0 开始
    std::string filePath;            // 当前文件路径，空串表示未命名
    bool dirty;                      // true 表示有未保存修改（UML 中名为 isDirty）

    // ---------------- 数据成员（实现选中与撤销所需，UML 未列出） ----------------

    bool selectionActive;            // 是否存在选中区域
    int selStartX, selStartY;        // 选区起点（已规范化，保证不晚于终点）
    int selEndX, selEndY;            // 选区终点

    // 一次编辑前的缓冲区快照，用于撤销/重做
    struct Snapshot
    {
        std::vector<std::string> lines;
        int cursorX;
        int cursorY;
    };

    std::vector<Snapshot> undoStack; // 撤销栈
    std::vector<Snapshot> redoStack; // 重做栈
    bool suppressUndo;               // 组合操作期间抑制重复压栈，保证一次操作只产生一个快照

    // 撤销合并：记录上一次编辑操作，连续字符输入合并成一个撤销单元。
    // mergeRow 记录上一段连续输入的所在行；同一行上连续 insertChar 视为一个单元
    int mergeRow;                    // 上一次连续输入所在的行号，-1 表示无连续输入

    // ---------------- 私有辅助函数 ----------------

    // 保证缓冲区至少有 1 行，并把光标修正到合法范围内
    void clampCursor();

    // 判断行号是否合法
    bool isValidRow(int row) const;

    // 把选区坐标修正到合法范围并重新规范化（编辑后选区可能失效）
    void clampSelection();

    // 记录当前状态到撤销栈，并清空重做栈；每个会修改内容的操作开头调用
    void pushUndoSnapshot();

    // 取当前状态的快照
    Snapshot makeSnapshot() const;

    // 用快照覆盖当前状态
    void applySnapshot(const Snapshot& snap);

    // 按 '\n' 拆分文本（并去掉行尾多余的 '\r'）
    static std::vector<std::string> splitLines(const std::string& text);
};

#endif // EDITOR_BUFFER_H
