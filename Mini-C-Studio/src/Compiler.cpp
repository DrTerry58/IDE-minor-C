// ============================================================
// 文件名: Compiler.cpp
// 职责: 编译调度模块实现
// 负责人: D 同学
// ============================================================

#include "Compiler.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <windows.h>

static bool fileExists(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

static void deleteFileIfExists(const std::string& path) {
    if (fileExists(path)) DeleteFileA(path.c_str());
}

static std::string readFileToString(const std::string& path) {
    std::ifstream file(path.c_str());
    if (!file.is_open()) return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// ============================================================
// 解析 gcc 输出行
// 格式：<path>:<line>:<col>: <level>: <msg>
// GCC 也可能省略列号，或输出不带源码位置的 fatal error。
// ============================================================
static bool parseNumber(const std::string& text, int& value) {
    if (text.empty()) return false;
    for (size_t i = 0; i < text.size(); i++)
        if (text[i] < '0' || text[i] > '9') return false;
    value = std::atoi(text.c_str());
    return true;
}

static bool parseLocation(const std::string& prefix, int& line, int& column) {
    size_t lastColon = prefix.rfind(':');
    if (lastColon == std::string::npos) return false;

    int lastNumber = 0;
    if (!parseNumber(prefix.substr(lastColon + 1), lastNumber)) return false;

    size_t previousColon = prefix.rfind(':', lastColon - 1);
    int previousNumber = 0;
    if (previousColon != std::string::npos &&
        parseNumber(prefix.substr(previousColon + 1, lastColon - previousColon - 1), previousNumber)) {
        line = previousNumber > 0 ? previousNumber - 1 : 0;
        column = lastNumber > 0 ? lastNumber - 1 : 0;
    }
    else {
        line = lastNumber > 0 ? lastNumber - 1 : 0;
        column = 0;
    }
    return true;
}

static bool parseGccLine(const std::string& line, Diagnostic& diag) {
    struct Severity {
        const char* marker;
        DiagLevel level;
        const char* tag;
    };
    static const Severity severities[] = {
        { ": fatal error: ", DIAG_ERROR, "error" },
        { ": error: ", DIAG_ERROR, "error" },
        { ": warning: ", DIAG_WARNING, "warning" },
        { ": note: ", DIAG_INFO, "note" }
    };

    for (size_t i = 0; i < sizeof(severities) / sizeof(severities[0]); i++) {
        const std::string marker = severities[i].marker;
        size_t position = line.find(marker);
        if (position == std::string::npos) continue;

        const std::string prefix = line.substr(0, position);
        int lineNumber = 0;
        int columnNumber = 0;
        parseLocation(prefix, lineNumber, columnNumber);

        std::string message = line.substr(position + marker.size());
        if (!message.empty() && message.back() == '\r') message.pop_back();
        diag.line = lineNumber;
        diag.column = columnNumber;
        diag.level = severities[i].level;
        diag.tag = severities[i].tag;
        diag.message = message;
        return true;
    }
    return false;
}

// ============================================================
// 兜底：判断这一行是否是链接错误或含 error/fatal 的关键行
// 这些行格式不标准，但应当作为通用错误显示
// ============================================================
static bool isFallbackErrorLine(const std::string& line) {
    if (line.find("undefined reference") != std::string::npos) return true;
    if (line.find("ld returned")         != std::string::npos) return true;
    if (line.find("fatal error")         != std::string::npos) return true;
    // 一般的 error 关键字（小写以避免匹配到 "Error" 类标识符）
    if (line.find("error:")              != std::string::npos) return true;
    return false;
}

bool Compiler::available() {
    char buffer[1024] = { 0 };
    FILE* fp = _popen("where gcc 2>nul", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            gccPath = buffer;
            if (!gccPath.empty() && gccPath.back() == '\n') gccPath.pop_back();
            if (!gccPath.empty() && gccPath.back() == '\r') gccPath.pop_back();
            _pclose(fp);
            return true;
        }
        _pclose(fp);
    }
    gccPath = "";
    return false;
}

std::string Compiler::exePathOf(const std::string& srcPath) {
    std::string exePath = srcPath;
    size_t dotPos = exePath.find_last_of('.');
    if (dotPos != std::string::npos) exePath = exePath.substr(0, dotPos);
    return exePath + ".exe";
}

CompileResult Compiler::compile(const std::string& srcPath) {
    std::string exePath = exePathOf(srcPath);

    CompileResult result;
    result.state = CS_NONE;
    result.raw = "";
    result.items.clear();
    result.exePath = "";

    if (!fileExists(srcPath)) {
        Diagnostic diag;
        diag.line = 0; diag.column = 0;
        diag.level = DIAG_ERROR; diag.tag = "error";
        diag.message = "源文件不存在: " + srcPath;
        result.items.push_back(diag);
        result.state = CS_ERROR;
        result.raw = "ERROR: Source file not found: " + srcPath;
        return result;
    }

    // 删除旧 exe
    deleteFileIfExists(exePath);

    // 编译命令
    std::string errorFile = "error.tmp";
    char cmd[1024];
    sprintf(cmd, "gcc \"%s\" -o \"%s\" 2> \"%s\"",
            srcPath.c_str(), exePath.c_str(), errorFile.c_str());
    system(cmd);

    // 读取 gcc 的原始错误输出
    std::string rawErr = readFileToString(errorFile);
    deleteFileIfExists(errorFile);
    result.raw = rawErr;

    // 逐行解析
    int errorCount = 0;
    int warningCount = 0;

    if (!rawErr.empty()) {
        std::istringstream stream(rawErr);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) continue;

            Diagnostic diag;
            if (parseGccLine(line, diag)) {
                if (diag.level == DIAG_ERROR)        errorCount++;
                else if (diag.level == DIAG_WARNING) warningCount++;
                result.items.push_back(diag);
            }
            else if (isFallbackErrorLine(line)) {
                // 兜底：格式不符但确是错误关键行
                diag.line = 0; diag.column = 0;
                diag.level = DIAG_ERROR; diag.tag = "error";
                diag.message = line;
                errorCount++;
                result.items.push_back(diag);
            }
            // 其他（gcc 上下文行、源码回显行、^ 指示行）丢弃
        }
    }

    // 判断编译状态
    bool exeExists = fileExists(exePath);

    if (exeExists && errorCount == 0) {
        result.state = (warningCount > 0) ? CS_WARNING : CS_OK;
        result.exePath = exePath;
    }
    else if (!exeExists && rawErr.empty()) {
        // 没有错误输出，exe 也没生成 → gcc 找不到
        Diagnostic diag;
        diag.line = 0; diag.column = 0;
        diag.level = DIAG_ERROR; diag.tag = "error";
        diag.message = "找不到 gcc 编译器，请确认 MinGW 已安装并配置环境变量。";
        result.items.push_back(diag);
        result.state = CS_NOCOMPILER;
    }
    else {
        result.state = CS_ERROR;
    }

    return result;
}