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

// 解析 gcc 输出行：只接受严格匹配 file:line:col: level: msg 的行
static bool parseGccLine(const std::string& line, Diagnostic& diag) {
    int lineNum = 0, colNum = 0;
    char filePart[256] = "", levelPart[64] = "", msgPart[512] = "";

    int parsed = sscanf(line.c_str(), "%255[^:]:%d:%d: %63[^:]: %511[^\n]",
        filePart, &lineNum, &colNum, levelPart, msgPart);
    if (parsed < 5) return false;

    std::string lv = levelPart;
    bool isError = (lv.find("error") != std::string::npos || lv.find("Error") != std::string::npos);
    bool isWarning = (lv.find("warning") != std::string::npos || lv.find("Warning") != std::string::npos);
    bool isNote = (lv.find("note") != std::string::npos || lv.find("Note") != std::string::npos);
    if (!isError && !isWarning && !isNote) return false;

    diag.line = (lineNum > 0) ? (lineNum - 1) : 0;
    diag.column = (colNum > 0) ? (colNum - 1) : 0;
    if (isError) { diag.level = DIAG_ERROR;   diag.tag = "error"; }
    else if (isWarning) { diag.level = DIAG_WARNING; diag.tag = "warning"; }
    else { diag.level = DIAG_INFO;    diag.tag = "note"; }
    diag.message = msgPart;
    return true;
}

bool Compiler::available() {
    char buffer[1024] = { 0 };
    FILE* fp = _popen("where gcc 2>nul", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            gccPath = buffer;
            if (!gccPath.empty() && gccPath.back() == '\n') gccPath.pop_back();
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
        diag.line = 0; diag.column = 0; diag.level = DIAG_ERROR; diag.tag = "error";
        diag.message = "源文件不存在: " + srcPath;
        result.items.push_back(diag);
        result.state = CS_ERROR;
        result.raw = "ERROR: Source file not found: " + srcPath;
        return result;
    }

    deleteFileIfExists(exePath);
    std::string errorFile = "error.tmp";
    char cmd[1024];
    sprintf(cmd, "gcc \"%s\" -o \"%s\" 2> \"%s\"", srcPath.c_str(), exePath.c_str(), errorFile.c_str());
    system(cmd);

    std::string rawErr = readFileToString(errorFile);
    deleteFileIfExists(errorFile);
    result.raw = rawErr;

    int errorCount = 0, warningCount = 0;
    if (!rawErr.empty()) {
        std::istringstream stream(rawErr);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) continue;
            Diagnostic diag;
            if (!parseGccLine(line, diag)) continue;
            if (diag.level == DIAG_ERROR) errorCount++;
            else if (diag.level == DIAG_WARNING) warningCount++;
            result.items.push_back(diag);
        }
    }

    bool exeExists = fileExists(exePath);
    if (exeExists && errorCount == 0) {
        result.state = (warningCount > 0) ? CS_WARNING : CS_OK;
        result.exePath = exePath;
    }
    else if (!exeExists && rawErr.empty()) {
        Diagnostic diag;
        diag.line = 0; diag.column = 0; diag.level = DIAG_ERROR; diag.tag = "error";
        diag.message = "找不到 gcc 编译器，请确认 MinGW 已安装并配置环境变量。";
        result.items.push_back(diag);
        result.state = CS_NOCOMPILER;
    }
    else {
        result.state = CS_ERROR;
    }
    return result;
}