// ============================================================================
// 文件名：CoreTypes.h
// 职责：GUI 层 与 编译/运行模块（D 同学）之间的【公共数据结构】
// 说明：这个文件是 C 与 D 的"契约"，双方都不能单方面改。
// ============================================================================

#pragma once
#ifndef MINIC_CORETYPES_H
#define MINIC_CORETYPES_H

#include <string>
#include <vector>

// ---------------- 诊断级别 ----------------
enum DiagLevel
{
    DIAG_INFO    = 0,   // 提示 / note
    DIAG_WARNING = 1,   // 警告
    DIAG_ERROR   = 2    // 错误
};

// ---------------- 单条诊断信息 ----------------
struct Diagnostic
{
    int         line;      // 行号，0-based（状态栏/面板显示时 +1）
    int         column;    // 列号，0-based
    DiagLevel   level;     // 级别
    std::string message;   // 描述文本
    std::string tag;       // 原始标签字符串，如 "error" / "warning" / "note"

    Diagnostic() : line(0), column(0), level(DIAG_ERROR) {}
};

// ---------------- 编译结果状态 ----------------
enum CompileState
{
    CS_NONE       = 0,   // 尚未编译
    CS_OK         = 1,   // 编译成功，无警告
    CS_WARNING    = 2,   // 编译成功，有警告
    CS_ERROR      = 3,   // 编译失败
    CS_NOCOMPILER = 4,   // 找不到编译器
    CS_TIMEOUT    = 5,   // 编译超时
    CS_INTERNAL   = 6    // IDE 内部错误（如源文件为空 / 路径非法）
};

// ---------------- 编译结果 ----------------
struct CompileResult
{
    CompileState          state;     // 状态
    std::string           raw;       // 编译器原始输出（整段文本，控制台用）
    std::vector<Diagnostic> items;   // 结构化诊断列表（诊断面板用）
    std::string           exePath;   // 生成的可执行文件路径

    CompileResult() : state(CS_NONE) {}
};

// ---------------- 运行状态 ----------------
enum RunState
{
    RS_IDLE    = 0,   // 空闲
    RS_RUNNING = 1,   // 运行中
    RS_EXIT_OK = 2,   // 正常结束
    RS_ERROR   = 3,   // 非零退出 / 崩溃
    RS_TIMEOUT = 4    // 超时
};

#endif // MINIC_CORETYPES_H
