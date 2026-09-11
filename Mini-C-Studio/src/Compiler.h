// ============================================================
// 文件名: Compiler.h
// 职责: 编译调度模块声明
// 负责人: D 同学
// 契约: CoreTypes.h（由 C 与 D 共同维护，不可单方面修改）
// 铁律: 行号 0-based | 同步返回 | 不弹窗/不抛异常
// ============================================================

#ifndef COMPILER_H
#define COMPILER_H

#include <string>
#include "CoreTypes.h"   // ★ 关键：结构体从契约文件引入

class Compiler {
public:
    // 检查 gcc 是否可用（路径内部记录）
    bool available();

    // 编译源文件（只收 srcPath，exe 路径由 exePathOf 推导）
    CompileResult compile(const std::string& srcPath);

    // 根据源文件路径推导 exe 路径（a.c -> a.exe）
    std::string exePathOf(const std::string& srcPath);

private:
    std::string gccPath;
};

#endif