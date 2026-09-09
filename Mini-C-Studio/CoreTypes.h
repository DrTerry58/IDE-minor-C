// ============================================================================
// 文件名：CoreTypes.h
// 职责：GUI 层 与 编译/运行模块（D 同学）之间的【公共数据结构】
// 说明：这个文件原本定义 Diagnostic / CompileResult / RunResult，
//       现在 D 同学已经交付 Compiler.h/.cpp，里面已经定义了同名的权威类型，
//       全工程（A、C 都一样）统一用 D 的那一份，这里【不再重复定义】，
//       只保留 C 自己 UI 内部用的状态枚举（CompileState / RunState / DiagLevel）。
// ============================================================================

#pragma once
#ifndef MINIC_CORETYPES_H
#define MINIC_CORETYPES_H

#include <string>
#include <vector>

// ★ D 同学交付的权威类型定义（Diagnostic / CompileResult / RunResult）。
//   D 的 Diagnostic.level 是 int：0=提示 / 1=警告 / 2=错误，
//   与下面 DiagLevel 的取值一一对应，可以直接比较、直接赋值。
#include "Compiler.h"

// ---------------- 诊断级别（取值与 D 的 Diagnostic.level 一致） ----------------
enum DiagLevel
{
    DIAG_INFO    = 0,   // 提示 / note
    DIAG_WARNING = 1,   // 警告
    DIAG_ERROR   = 2    // 错误
};

// ---------------- 编译结果状态（C 的 UI 内部状态机，与 D 的 CompileResult 分开） ----------------
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

// ---------------- 运行状态（C 的 UI 内部状态机） ----------------
enum RunState
{
    RS_IDLE    = 0,   // 空闲
    RS_RUNNING = 1,   // 运行中
    RS_EXIT_OK = 2,   // 正常结束
    RS_ERROR   = 3,   // 非零退出 / 崩溃
    RS_TIMEOUT = 4    // 超时
};

#endif // MINIC_CORETYPES_H
