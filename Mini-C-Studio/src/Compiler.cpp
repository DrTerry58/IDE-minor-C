// ============================================================
// 文件名: Compiler.cpp
// 职责: 编译调度模块实现
// 负责人: D 同学
// 契约: CoreTypes.h
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

// ============================================================
//  解析 gcc 输出行
//  返回值：true = 有效诊断行；false = 上下文行/回显行/指示行（应丢弃）
//
//  gcc 一次报错的典型输出：
//      bad.c: In function 'main':                      ← 无效（parsed < 5）
//      bad.c:1:21: error: expected ';' before '}'      ← 有效
//          1 | int main() { return 0 }                 ← 无效
//            |                     ^~                  ← 无效
//            |                     ;                   ← 无效
// ============================================================
static bool parseGccLine(const std::string& line, Diagnostic& diag) {
    int lineNum = 0, colNum = 0;
    char filePart[256]  = "";
    char levelPart[64]  = "";
    char msgPart[512]   = "";

    // 要求 5 段全部匹配：文件名:行号:列号: 级别: 消息
    int parsed = sscanf(line.c_str(), "%255[^:]:%d:%d: %63[^:]: %511[^\n]",
                        filePart, &lineNum, &colNum, levelPart, msgPart);

    // ★ 关键：parsed < 5 说明格式不符（上下文行 / 回显行 / 指示行），丢弃
    if (parsed < 5) return false;

    // levelPart 必须是 error / warning / note 之一
    std::string lv = levelPart;
    bool isError   = (lv.find("error")   != std::string::npos ||
                      lv.find("Error")   != std::string::npos);
    bool isWarning = (lv.find("warning") != std::string::npos ||
                      lv.find("Warning") != std::string::npos);
    bool isNote    = (lv.find("note")    != std::string::npos ||
                      lv.find("Note")    != std::string::npos);

    if (!isError && !isWarning && !isNote) return false;

    // 填充诊断字段（行号/列号减 1，变成 0-based）
    diag.line   = (lineNum > 0) ? (lineNum - 1) : 0;
    diag.column = (colNum > 0)  ? (colNum - 1)  : 0;

    if (isError) {
        diag.level = DIAG_ERROR;
        diag.tag   = "error";
    } else if (isWarning) {
        diag.level = DIAG_WARNING;
        diag.tag   = "warning";
    } else {
        diag.level = DIAG_INFO;
        diag.tag   = "note";
    }
    diag.message = msgPart;
    return true;
}

// ============================================================
// Compiler 类实现
// ============================================================

// ---------- 检查 gcc 是否可用 ----------
bool Compiler::available() {
    char buffer[1024] = {0};
    FILE* fp = _popen("where gcc 2>nul", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            gccPath = buffer;
            if (!gccPath.empty() && gccPath.back() == '\n') {
                gccPath.pop_back();
            }
            _pclose(fp);
            return true;
        }
        _pclose(fp);
    }
    gccPath = "";
    return false;
}

// ---------- 根据源文件路径推导 exe 路径 ----------
std::string Compiler::exePathOf(const std::string& srcPath) {
    std::string exePath = srcPath;
    size_t dotPos = exePath.find_last_of('.');
    if (dotPos != std::string::npos) {
        exePath = exePath.substr(0, dotPos);
    }
    exePath += ".exe";
    return exePath;
}

// ---------- 编译源文件 ----------
CompileResult Compiler::compile(const std::string& srcPath) {
    // 内部推导 exe 路径
    std::string exePath = exePathOf(srcPath);

    CompileResult result;
    result.state = CS_NONE;
    result.raw = "";
    result.items.clear();
    result.exePath = "";

    // 1. 检查源文件是否存在
    if (!fileExists(srcPath)) {
        Diagnostic diag;
        diag.line   = 0;
        diag.column = 0;
        diag.level  = DIAG_ERROR;
        diag.tag    = "error";
        diag.message = "源文件不存在: " + srcPath;
        result.items.push_back(diag);
        result.state = CS_ERROR;
        result.raw = "ERROR: Source file not found: " + srcPath;
        return result;
    }

    // 2. 删除旧 exe（防止残留干扰）
    deleteFileIfExists(exePath);

    // 3. 构造编译命令
    std::string errorFile = "error.tmp";
    char cmd[1024];
    sprintf(cmd, "gcc \"%s\" -o \"%s\" 2> \"%s\"",
            srcPath.c_str(), exePath.c_str(), errorFile.c_str());

    // 4. 执行编译
    system(cmd);

    // 5. 读取错误输出
    std::string rawErr = readFileToString(errorFile);
    deleteFileIfExists(errorFile);
    result.raw = rawErr;

    // 6. 逐行解析（只保留有效诊断行，过滤上下文行/回显行/指示行）
    int errorCount = 0;
    int warningCount = 0;

    if (!rawErr.empty()) {
        std::istringstream stream(rawErr);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) continue;

            Diagnostic diag;
            // ★ 无效行直接跳过
            if (!parseGccLine(line, diag)) continue;

            if (diag.level == DIAG_ERROR)        errorCount++;
            else if (diag.level == DIAG_WARNING) warningCount++;

            result.items.push_back(diag);
        }
    }

    // 7. 判断编译状态
    bool exeExists = fileExists(exePath);

    if (exeExists && errorCount == 0) {
        // 生成成功：区分 OK / WARNING
        if (warningCount > 0) {
            result.state = CS_WARNING;
        } else {
            result.state = CS_OK;
        }
        result.exePath = exePath;
    }
    else if (!exeExists && rawErr.empty()) {
        // 没有错误输出、exe 也没生成 → gcc 找不到
        Diagnostic diag;
        diag.line   = 0;
        diag.column = 0;
        diag.level  = DIAG_ERROR;
        diag.tag    = "error";
        diag.message = "找不到 gcc 编译器，请确认 MinGW 已安装并配置环境变量。";
        result.items.push_back(diag);
        result.state = CS_NOCOMPILER;
    }
    else {
        // 有错，编译失败
        result.state = CS_ERROR;
    }

    return result;
}