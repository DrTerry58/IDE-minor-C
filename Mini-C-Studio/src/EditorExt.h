// ============================================================================
// 文件名：EditorExt.h / EditorExt.cpp
// 负责人：C（GUI 界面 + 交互控制）
//
// 这是 C 的【缓冲适配层】，存在的唯一目的：
//   把"界面需要的编辑能力"翻译成"B 的 EditorBuffer 已经提供的函数"。
//
// 为什么需要它？
//   B 在《三方对比.docx》里指出：C 的接口说明要了 6 个 B 没有的函数，
//   而且 moveCursor 的语义和 A 写在 MainWindow.cpp 里的用法不一致。
//   按组长裁定 —— 【以 A 的 GitHub 代码 + B 现有函数为准，C 改自己的】——
//   本文件就是 C 改自己的地方：
//     · 光标"左右跳行 / 上下保列"的语义，由 C 在这里用 setCursor + getLineLength 实现，
//       不再依赖 B 的 moveCursor 的行为，B 怎么改都不会影响界面；
//     · B 暂缺的 insertString / getRangeText / deleteRange / setSelectionText，
//       用 B 已经有的 insertChar / pasteText / 选区 API 组合出来；
//     · B 暂缺的查找替换，见 EditorExt.cpp（C 用 B 的只读接口 getLine 组合实现，
//       B 交付 findNext / replaceAll 后，只需把 EditorExt.cpp 里的实现删掉、
//       改成直接调 B 的函数即可）。
//
// 该文件只使用 B 的【公开成员函数】，不访问 B 的任何成员变量。
// ============================================================================

#pragma once
#ifndef MINIC_EDITOREXT_H
#define MINIC_EDITOREXT_H

#include "EditorBuffer.h"
#include <string>

// !!! 唯一可能需要改的一行 !!!
// B 同学已在《B同学答复清单》中确认：setCursor 真实签名是 setCursor(列, 行)，
// 与组长 UML 的 (x, y)（x=横=列）一致。故这里保持 0。
// （若将来 B 改成 (行,列)，再改回 1 即可，其余代码不用动。）
#define MINIC_SETCURSOR_IS_ROW_COL    0

// ============================================================================
//  一、光标移动：C 自己定义语义，与 B 的 moveCursor 行为解耦
// ============================================================================

// 所有代码统一走这里设置光标，不直接调 buffer->setCursor
inline void bufSetCursor(EditorBuffer* b, int row, int col)
{
    if (!b) return;
#if MINIC_SETCURSOR_IS_ROW_COL
    b->setCursor(row, col);
#else
    b->setCursor(col, row);
#endif
}

inline int  bufRow(EditorBuffer* b) { return b ? b->getCursorY() : 0; }
inline int  bufCol(EditorBuffer* b) { return b ? b->getCursorX() : 0; }

// 左：列>0 则左移；否则跳到上一行行尾；已在开头则不动
inline void bufMoveLeft(EditorBuffer* b)
{
    if (!b) return;
    int r = b->getCursorY(), c = b->getCursorX();
    if (c > 0)                      bufSetCursor(b, r, c - 1);
    else if (r > 0)                 bufSetCursor(b, r - 1, b->getLineLength(r - 1));
    else                            bufSetCursor(b, 0, 0);
}

// 右：列<行长 则右移；否则跳到下一行行首；已在末尾则不动
inline void bufMoveRight(EditorBuffer* b)
{
    if (!b) return;
    int r = b->getCursorY(), c = b->getCursorX();
    int len = b->getLineLength(r);
    if (c < len)                        bufSetCursor(b, r, c + 1);
    else if (r + 1 < b->getLineCount()) bufSetCursor(b, r + 1, 0);
    else                                bufSetCursor(b, r, len);
}

// 上：列号尽量保持（B 的 setCursor 会自动把列夹到新行行尾）
//   wantCol 传 -1 表示"用当前列"；传具体值用于连续按上下键时记住原始列
inline int  bufMoveUp(EditorBuffer* b, int wantCol)
{
    if (!b) return 0;
    int r = b->getCursorY(), c = b->getCursorX();
    if (wantCol < 0) wantCol = c;
    if (r > 0) bufSetCursor(b, r - 1, wantCol);
    else       bufSetCursor(b, 0, 0);
    return wantCol;
}

inline int  bufMoveDown(EditorBuffer* b, int wantCol)
{
    if (!b) return 0;
    int r = b->getCursorY(), c = b->getCursorX();
    if (wantCol < 0) wantCol = c;
    if (r + 1 < b->getLineCount()) bufSetCursor(b, r + 1, wantCol);
    else                           bufSetCursor(b, r, b->getLineLength(r));
    return wantCol;
}

inline void bufHome(EditorBuffer* b)
{
    if (b) bufSetCursor(b, b->getCursorY(), 0);
}

inline void bufEnd(EditorBuffer* b)
{
    if (b) bufSetCursor(b, b->getCursorY(), b->getLineLength(b->getCursorY()));
}

inline void bufDocHome(EditorBuffer* b) { if (b) bufSetCursor(b, 0, 0); }

inline void bufDocEnd(EditorBuffer* b)
{
    if (!b) return;
    int last = b->getLineCount() - 1;
    bufSetCursor(b, last, b->getLineLength(last));
}

// 按单词跳过（Ctrl + 左右方向键）
void bufMoveWordLeft(EditorBuffer* b);
void bufMoveWordRight(EditorBuffer* b);

// ============================================================================
//  二、文本输入：B 没有 insertString，用 pasteText 组合
// ============================================================================
// 在光标处插入一段文本（含 \n 会自动换行）。等价于 B 文档里的 insertString。
// 若 B 后续补了 insertString()，把本函数体改成 b->insertString(s) 即可。
inline void bufInsertString(EditorBuffer* b, const std::string& s)
{
    if (!b || s.empty()) return;
    b->pasteText(s);
}

// 用当前选区替换成一段文本（等价于 B 文档里的 setSelectionText）
inline void bufReplaceSelection(EditorBuffer* b, const std::string& s)
{
    if (!b) return;
    if (b->hasSelection()) b->deleteSelection();
    if (!s.empty()) b->pasteText(s);
}

// ============================================================================
//  三、选区：坐标由 C 维护，取文本/删除交给 B
// ============================================================================
// C 持有选区坐标（m_selR1..），这里负责"同步给 B"，让 B 的
// getSelectedText() / deleteSelection() 能正常工作。
inline void bufSyncSelection(EditorBuffer* b, bool active,
                             int r1, int c1, int r2, int c2)
{
    if (!b) return;
    if (!active) { b->clearSelection(); return; }
    // B 的 setSelection 是 (startX=列, startY=行, endX=列, endY=行)，
    // 这里入参是 (r1=行, c1=列, r2=行, c2=列)，故映射成 (c1,r1,c2,r2)。
    // （替身版曾是 (startRow,startCol,...) 顺序，换真实模块后必须交换！）
    b->setSelection(c1, r1, c2, r2);
}

inline std::string bufSelectedText(EditorBuffer* b)
{
    return b ? b->getSelectedText() : std::string();
}

inline void bufDeleteSelection(EditorBuffer* b)
{
    if (b && b->hasSelection()) b->deleteSelection();
}

// 取任意区间文本（等价于 B 文档里的 getRangeText）
// C 用 B 的只读接口 getLine 组合实现，不碰缓冲区内部。
std::string bufRangeText(EditorBuffer* b, int r1, int c1, int r2, int c2);

// 在 (row, col) 处插入文本（等价于 B 文档里的 insertAt）。
// 先把光标移到目标位置，再走 pasteText（B 的 pasteText 在光标处插入并移动光标）。
void bufInsertAt(EditorBuffer* b, int row, int col, const std::string& s);

// 删除任意矩形区间 (r1,c1)..(r2,c2)（等价于 B 文档里的 deleteRange）。
// 用 B 的 setSelection + deleteSelection 组合；B 会规范化“起点 <= 终点”。
void bufDeleteRange(EditorBuffer* b, int r1, int c1, int r2, int c2);

// 清空缓冲区（等价于 B 文档里的 clear）。
void bufClear(EditorBuffer* b);

// 跳到某一行行首（等价于 B 文档里的 gotoLine）。
void bufGotoLine(EditorBuffer* b, int row);

// ============================================================================
//  四、查找 / 替换：B 尚未提供，C 用 B 的只读接口组合实现
//      B 交付 findNext / replaceAll 后，把 EditorExt.cpp 里两个函数体
//      换成直接调用 B 的函数即可，调用方一行不改。
// ============================================================================
//
// 从 (fromR, fromC) 开始正向查找 pattern。
//   找到返回 true，并把位置写入 outR / outC，长度写入 outLen；
//   找不到（不回绕）返回 false；wrap = true 时会从头再来一圈。
bool bufFindNext(EditorBuffer* b,
                 const std::string& pattern,
                 bool matchCase,
                 int fromR, int fromC,
                 int& outR, int& outC, int& outLen,
                 bool wrap);

// 全文替换，返回替换处数
int  bufReplaceAll(EditorBuffer* b,
                   const std::string& pattern,
                   const std::string& replacement,
                   bool matchCase);

#endif // MINIC_EDITOREXT_H
