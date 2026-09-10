// ============================================================
// 文件名: Compiler.h
// 职责: 编译调度 + 运行时托管 模块声明
// 负责人: D 同学
// 依据: 接口对接说明.docx 第 3.3、3.4 节
// 铁律: 行号 0-based | 同步返回 | 不弹窗/不抛异常
// ============================================================

#ifndef COMPILER_H
#define COMPILER_H

#include <string>
#include <vector>

// ---------- 诊断信息（编译器的错误/警告） ----------
struct Diagnostic {
    int line;              // 行号，0-based！！！（gcc 是 1-based，D 已减 1）
    int col;               // 列号，0-based，字节下标
    int level;             // 0=提示 1=警告 2=错误
    std::string code;      // 错误码，如 "C2143"，没有就空串
    std::string message;   // 错误/警告描述文本（中英文均可）
    std::string file;      // 出错文件名，单文件工程可空
};

// ---------- 编译结果 ----------
struct CompileResult {
    bool success;          // 是否成功生成了 exe
    int errorCount;        // 错误数量
    int warningCount;      // 警告数量
    std::string rawOutput; // gcc 原始输出全文
    std::vector<Diagnostic> diags; // 解析后的诊断列表
};

// ---------- 运行结果 ----------
struct RunResult {
    int exitCode;          // 0=正常退出，非0=异常
    bool timeout;          // 是否超时
    bool crashed;          // 是否崩溃
    std::string stdoutText; // 标准输出
    std::string stderrText; // 标准错误
};

// ============================================================
// 编译调度（对应 3.3 节）
// ============================================================

/**
 * 编译源文件
 * @param srcPath  .c 源文件路径（由 E/B 提供）
 * @param exePath  输出的 exe 文件路径
 * @return CompileResult 包含成功/失败标志、错误列表、原始输出
 */
CompileResult compile(const std::string& srcPath, const std::string& exePath);

/**
 * 检查 gcc 是否可用
 * @param outGccPath 返回 gcc 的路径（如果找到）
 * @return true 表示 gcc 可用，false 表示不可用
 */
bool compilerAvailable(std::string& outGccPath);

// ============================================================
// 运行时托管（对应 3.4 节，方案一：同步版）
// ============================================================

/**
 * 同步运行程序（阻塞直到程序退出或超时）
 * @param exePath    要执行的 exe 路径
 * @param stdinText  向程序标准输入发送的文本
 * @param timeoutMs  超时时间（毫秒），0 表示永不超时
 * @return RunResult 包含退出码、输出、超时/崩溃标志
 */
RunResult runSync(const std::string& exePath, const std::string& stdinText, int timeoutMs);

#endif
