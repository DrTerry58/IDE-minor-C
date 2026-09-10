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

private:
    CoreApi() {}
    CoreApi(const CoreApi&);
    CoreApi& operator=(const CoreApi&);

    std::string m_lastError;   // 最近一次文件操作的错误原因（失败时写入，成功时清空）
};

#endif // MINIC_COREAPI_H
