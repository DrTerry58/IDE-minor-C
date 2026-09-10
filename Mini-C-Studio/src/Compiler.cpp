// ============================================================
// 文件名: Compiler.cpp
// 职责: 编译调度 + 运行时托管 模块实现
// 负责人: D 同学
// ============================================================

#include "Compiler.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <windows.h>

// ---------- 工具函数：检查文件是否存在 ----------
static bool fileExists(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

// ---------- 工具函数：删除文件 ----------
static void deleteFileIfExists(const std::string& path) {
    if (fileExists(path)) {
        DeleteFileA(path.c_str());
    }
}

// ---------- 工具函数：读取整个文本文件 ----------
static std::string readFileToString(const std::string& path) {
    std::ifstream file(path.c_str());
    if (!file.is_open()) return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// ---------- 工具函数：解析 gcc 输出行 ----------
static Diagnostic parseGccLine(const std::string& line) {
    Diagnostic diag;
    diag.line = 0;
    diag.col = 0;
    diag.level = 2;      // 默认当作 error
    diag.code = "";
    diag.message = "";
    diag.file = "";

    // gcc 输出格式示例：
    // test.c:5:10: error: 'x' undeclared
    // test.c:5:10: warning: unused variable 'y'
    // test.c:5:10: note: use 'int' instead

    // 提取文件名、行号、列号
    int lineNum = 0, colNum = 0;
    char filePart[256] = "";
    char levelPart[64] = "";
    char msgPart[512] = "";

    // 尝试解析格式: 文件名:行号:列号: 级别: 信息
    int parsed = sscanf(line.c_str(), "%255[^:]:%d:%d: %63[^:]: %511[^\n]",
                        filePart, &lineNum, &colNum, levelPart, msgPart);

    if (parsed >= 3) {
        diag.file = filePart;
        // ★★★ 关键：行号减 1，变成 0-based ★★★
        diag.line = (lineNum > 0) ? (lineNum - 1) : 0;
        diag.col = (colNum > 0) ? (colNum - 1) : 0;

        // 识别级别
        std::string lv = levelPart;
        if (lv.find("error") != std::string::npos || lv.find("Error") != std::string::npos) {
            diag.level = 2;
        } else if (lv.find("warning") != std::string::npos || lv.find("Warning") != std::string::npos) {
            diag.level = 1;
        } else {
            diag.level = 0;
        }
        diag.message = msgPart;
        diag.code = "";
    } else {
        // 解析失败，整行作为原始信息
        diag.message = line;
        diag.level = 2;
    }

    return diag;
}

// ============================================================
// 编译调度（3.3 节）
// ============================================================

CompileResult compile(const std::string& srcPath, const std::string& exePath) {
    CompileResult result;
    result.success = false;
    result.errorCount = 0;
    result.warningCount = 0;
    result.rawOutput = "";
    result.diags.clear();

    // ---------- 1. 检查源文件是否存在 ----------
    if (!fileExists(srcPath)) {
        Diagnostic diag;
        diag.line = 0;
        diag.col = 0;
        diag.level = 2;
        diag.code = "";
        diag.message = "源文件不存在: " + srcPath;
        diag.file = srcPath;
        result.diags.push_back(diag);
        result.errorCount = 1;
        result.rawOutput = "ERROR: Source file not found: " + srcPath;
        return result;
    }

    // ---------- 2. 删除旧的 exe（防止残留干扰） ----------
    deleteFileIfExists(exePath);

    // ---------- 3. 构造编译命令 ----------
    // 使用 2> 重定向错误输出到临时文件
    std::string errorFile = "error.tmp";
    char cmd[1024];
    sprintf(cmd, "gcc \"%s\" -o \"%s\" 2> \"%s\"", srcPath.c_str(), exePath.c_str(), errorFile.c_str());

    // ---------- 4. 执行编译 ----------
    int ret = system(cmd);

    // ---------- 5. 读取错误输出 ----------
    std::string rawErr = readFileToString(errorFile);
    deleteFileIfExists(errorFile); // 清理临时文件

    result.rawOutput = rawErr;

    // ---------- 6. 解析错误输出 ----------
    if (rawErr.empty()) {
        // 没有错误输出，检查 exe 是否生成
        if (fileExists(exePath)) {
            result.success = true;
            result.errorCount = 0;
            result.warningCount = 0;
            // 添加一条成功信息
            Diagnostic diag;
            diag.line = 0;
            diag.col = 0;
            diag.level = 0;
            diag.code = "";
            diag.message = "编译成功！";
            diag.file = "";
            result.diags.push_back(diag);
        } else {
            // 没有错误输出但 exe 也没生成，可能是 gcc 没找到
            result.success = false;
            Diagnostic diag;
            diag.line = 0;
            diag.col = 0;
            diag.level = 2;
            diag.code = "";
            diag.message = "找不到 gcc 编译器，请确认 MinGW 已安装并配置环境变量。";
            diag.file = "";
            result.diags.push_back(diag);
            result.errorCount = 1;
        }
        return result;
    }

    // ---------- 7. 逐行解析 gcc 输出 ----------
    std::istringstream stream(rawErr);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        Diagnostic diag = parseGccLine(line);

        // 如果解析失败，把整行作为错误信息
        if (diag.message.empty()) {
            diag.message = line;
            diag.level = 2;
        }

        if (diag.level == 2) result.errorCount++;
        else if (diag.level == 1) result.warningCount++;

        result.diags.push_back(diag);
    }

    // ---------- 8. 判断是否成功 ----------
    // 如果 exe 存在且没有 error（只有 warning 也算成功）
    if (fileExists(exePath) && result.errorCount == 0) {
        result.success = true;
    } else {
        result.success = false;
    }

    return result;
}

// ---------- 检查 gcc 是否可用 ----------
bool compilerAvailable(std::string& outGccPath) {
    // 尝试在 PATH 中查找 gcc
    char buffer[1024] = {0};
    FILE* fp = _popen("where gcc 2>nul", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            outGccPath = buffer;
            // 去掉换行符
            if (!outGccPath.empty() && outGccPath.back() == '\n') {
                outGccPath.pop_back();
            }
            _pclose(fp);
            return true;
        }
        _pclose(fp);
    }
    outGccPath = "";
    return false;
}

// ============================================================
// 运行时托管（3.4 节，方案一：同步版）
// ============================================================

RunResult runSync(const std::string& exePath, const std::string& stdinText, int timeoutMs) {
    RunResult result;
    result.exitCode = -1;
    result.timeout = false;
    result.crashed = false;
    result.stdoutText = "";
    result.stderrText = "";

    // ---------- 1. 检查 exe 是否存在 ----------
    if (!fileExists(exePath)) {
        result.exitCode = -1;
        result.crashed = false;
        result.timeout = false;
        result.stderrText = "错误：可执行文件不存在，请先编译。";
        return result;
    }

    // ---------- 2. 构建带重定向的运行命令 ----------
    // 使用 > 重定向 stdout 和 stderr
    std::string stdoutFile = "stdout.tmp";
    std::string stderrFile = "stderr.tmp";
    char cmd[1024];
    sprintf(cmd, "\"%s\" > \"%s\" 2> \"%s\"", exePath.c_str(), stdoutFile.c_str(), stderrFile.c_str());

    // ---------- 3. 执行程序（同步） ----------
    DWORD startTime = GetTickCount();
    int ret = system(cmd);
    DWORD elapsed = GetTickCount() - startTime;

    // ---------- 4. 检查超时 ----------
    if (timeoutMs > 0 && elapsed > (DWORD)timeoutMs) {
        result.timeout = true;
        result.exitCode = -1;
        result.stderrText = "程序运行超时（" + std::to_string(timeoutMs) + "ms）";
        // 清理临时文件
        deleteFileIfExists(stdoutFile);
        deleteFileIfExists(stderrFile);
        return result;
    }

    // ---------- 5. 读取输出 ----------
    result.stdoutText = readFileToString(stdoutFile);
    result.stderrText = readFileToString(stderrFile);
    result.exitCode = ret;

    // ---------- 6. 检查是否崩溃（非 0 退出码且没有正常输出） ----------
    if (ret != 0) {
        // 如果 stderr 有内容，说明程序可能有错误输出
        if (!result.stderrText.empty()) {
            // 可能崩溃，也可能只是报错退出
            result.crashed = true;
        }
    } else {
        result.crashed = false;
    }

    // ---------- 7. 清理临时文件 ----------
    deleteFileIfExists(stdoutFile);
    deleteFileIfExists(stderrFile);

    return result;
}
