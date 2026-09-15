// ============================================================================
// 文件名：CoreApi.h
// 职责：GUI 层（C 同学）访问所有核心功能的【唯一入口 / 门面】
//
// 为什么要这一层？
//   src/core 完全不依赖 GUI，src/gui 只依赖 CoreApi。
//   队友模块没写完时，CoreApi 走"占位实现"（MiniStub.h）；
//   队友写完后，在 CoreApi.cpp 里把宏改成 1 即可无缝切换，
//   MainWindow.cpp 一行都不用动。
// ============================================================================

#pragma once
#ifndef MINIC_COREAPI_H
#define MINIC_COREAPI_H

#include "MiniCConfig.h"
#include "CoreTypes.h"
#include <string>
#include <vector>

class CoreApi
{
public:
    static CoreApi& inst();

    // ================= B 同学：文本缓冲区 =================
    int         bufLineCount();
    std::string bufGetLine(int row);
    void        bufInsertChar(char c);
    void        bufInsertString(const std::string& s);
    void        bufInsertAt(int row, int col, const std::string& s);
    void        bufDeleteBack();                       // Backspace
    void        bufDeleteForward();                    // Delete
    void        bufDeleteRange(int r1, int c1, int r2, int c2);
    std::string bufGetRange(int r1, int c1, int r2, int c2);
    void        bufEnter();
    void        bufMoveCursor(int dx, int dy);
    void        bufSetCursor(int row, int col);
    void        bufGotoLine(int row);
    int         bufRow();
    int         bufCol();
    void        bufLoad(const std::string& text);
    std::string bufSave();
    void        bufClear();
    bool        bufDirty();
    void        bufSetDirty(bool d);
    std::string bufPath();
    void        bufSetPath(const std::string& p);
    void        bufUndo();
    void        bufRedo();
    bool        bufCanUndo();
    bool        bufCanRedo();

    // ================= E 同学：文件生命周期 =================
    bool fileNew();
    bool fileOpen(const std::string& path);            // 读盘 -> 重建缓冲区
    bool fileSave(const std::string& path);            // 缓冲区 -> 写盘
    const char* lastError() const;                     // 最近一次文件操作失败原因（中文，可直接弹窗）

    // ================= D 同学：编译 =================
    bool          compilerAvailable();
    CompileResult compile(const std::string& srcPath);
    std::string   exePathOf(const std::string& srcPath);

    // ================= D 同学：运行 =================
    bool runStart(const std::string& exePath);
    bool runPoll(std::string& out, int& exitCode, bool& finished);  // 非阻塞轮询
    void runSendInput(const std::string& line);
    void runStop();
    bool runIsRunning();

    // ================= AI 助手（C 声明接口，B 负责实现） =================
    // 分工：C 只声明下面这些门面函数并实现 aiHistory()/aiClearHistory()（供面板渲染）；
    //       aiAsk/aiExplainCode/aiFixError/aiAvailable 的真实逻辑由 B 在 CoreApi 接入
    //       AIClient 后实现（见 CoreApi.cpp 中 TODO(B)）。GUI 不直接 #include "AIClient.h"。
    bool          aiAvailable() const;              // AI 是否可用（B 接入后返回 g_ai.isConfigured()）
    std::string   aiAsk(const std::string& prompt); // 单轮问答，返回回复文本（B 实现）
    std::string   aiExplainCode(const std::string& code);        // 解释代码（B 实现）
    std::string   aiFixError(const std::string& code, const std::string& diag); // 根据诊断修复（B 实现）
    const std::vector<AIMessage>& aiHistory() const; // 多轮对话上下文（UI 展示用，B 的 aiAsk 写入）
    void          aiClearHistory();

private:
    CoreApi() {}
    CoreApi(const CoreApi&);
    CoreApi& operator=(const CoreApi&);

    std::string m_lastError;   // 最近一次文件操作的错误原因（失败时写入，成功时清空）

    std::vector<AIMessage> m_aiHistory;  // 多轮 AI 对话上下文（UI 展示用；由 B 的 aiAsk 写入）
};

#endif // MINIC_COREAPI_H
