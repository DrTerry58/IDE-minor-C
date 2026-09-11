// ============================================================
// 文件名: Runtime.cpp
// 职责: 程序运行时托管实现（异步轮询 + 管道 IO）
// 负责人: D 同学
// 说明: 使用 CreateProcess + 匿名管道，绕过 cmd /S /c 引号坑
// ============================================================

#include "Runtime.h"

Runtime::Runtime()
    : hProcess(NULL), hThread(NULL),
      hStdinWrite(NULL), hStdoutRead(NULL), hStderrRead(NULL),
      lastExitCode(-1), processRunning(false), processFinished(false)
{
}

Runtime::~Runtime() {
    stop();
    cleanup();
}

// ---------- 启动子进程 ----------
bool Runtime::start(const std::string& exePath) {
    // 若已在运行，先终止并清理
    if (processRunning) {
        stop();
    }
    cleanup();

    lastExitCode = -1;
    processRunning = false;
    processFinished = false;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hStdinRead   = NULL;
    HANDLE hStdoutWrite = NULL;
    HANDLE hStderrWrite = NULL;

    // 1. 创建 stdin 管道
    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
        return false;
    }
    // 写端不继承给子进程
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);

    // 2. 创建 stdout 管道
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        hStdinWrite = NULL;
        return false;
    }
    // 读端不继承给子进程
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    // 3. 创建 stderr 管道
    if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        hStdinWrite = NULL;
        hStdoutRead = NULL;
        return false;
    }
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

    // 4. 设置 STARTUPINFO（重定向三个标准句柄）
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput  = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError  = hStderrWrite;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // 5. 直接 CreateProcess（不经 cmd，避免 /S 引号坑）
    std::string cmdLine = "\"" + exePath + "\"";
    BOOL ok = CreateProcessA(
        NULL,
        (LPSTR)cmdLine.c_str(),
        NULL, NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL, NULL,
        &si, &pi
    );

    // 6. 关闭子进程端的句柄（本进程不再需要）
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);

    if (!ok) {
        CloseHandle(hStdinWrite);   hStdinWrite  = NULL;
        CloseHandle(hStdoutRead);   hStdoutRead  = NULL;
        CloseHandle(hStderrRead);   hStderrRead  = NULL;
        return false;
    }

    hProcess = pi.hProcess;
    hThread  = pi.hThread;
    processRunning = true;

    return true;
}

// ---------- 非阻塞轮询 ----------
bool Runtime::poll(std::string& out, int& exitCode, bool& finished) {
    exitCode = -1;
    finished = false;

    if (!processRunning && !processFinished) {
        return false;
    }

    // 1. 读尽 stdout 当前可用数据
    if (hStdoutRead) {
        DWORD bytesAvail = 0;
        while (PeekNamedPipe(hStdoutRead, NULL, 0, NULL, &bytesAvail, NULL)
               && bytesAvail > 0) {
            char buf[4096];
            DWORD toRead = (bytesAvail < sizeof(buf)) ? bytesAvail : (DWORD)sizeof(buf);
            DWORD bytesRead = 0;
            if (!ReadFile(hStdoutRead, buf, toRead, &bytesRead, NULL) || bytesRead == 0)
                break;
            out.append(buf, bytesRead);
        }
    }

    // 2. 读尽 stderr 当前可用数据
    if (hStderrRead) {
        DWORD bytesAvail = 0;
        while (PeekNamedPipe(hStderrRead, NULL, 0, NULL, &bytesAvail, NULL)
               && bytesAvail > 0) {
            char buf[4096];
            DWORD toRead = (bytesAvail < sizeof(buf)) ? bytesAvail : (DWORD)sizeof(buf);
            DWORD bytesRead = 0;
            if (!ReadFile(hStderrRead, buf, toRead, &bytesRead, NULL) || bytesRead == 0)
                break;
            out.append(buf, bytesRead);
        }
    }

    // 3. 检查进程是否已退出
    if (processRunning && hProcess) {
        DWORD waitResult = WaitForSingleObject(hProcess, 0);
        if (waitResult == WAIT_OBJECT_0) {
            DWORD ec = 0;
            GetExitCodeProcess(hProcess, &ec);
            lastExitCode = (int)ec;

            // 读取退出后残余输出
            if (hStdoutRead) {
                DWORD bytesAvail = 0;
                while (PeekNamedPipe(hStdoutRead, NULL, 0, NULL, &bytesAvail, NULL)
                       && bytesAvail > 0) {
                    char buf[4096];
                    DWORD toRead = (bytesAvail < sizeof(buf)) ? bytesAvail : (DWORD)sizeof(buf);
                    DWORD bytesRead = 0;
                    if (!ReadFile(hStdoutRead, buf, toRead, &bytesRead, NULL) || bytesRead == 0)
                        break;
                    out.append(buf, bytesRead);
                }
            }
            if (hStderrRead) {
                DWORD bytesAvail = 0;
                while (PeekNamedPipe(hStderrRead, NULL, 0, NULL, &bytesAvail, NULL)
                       && bytesAvail > 0) {
                    char buf[4096];
                    DWORD toRead = (bytesAvail < sizeof(buf)) ? bytesAvail : (DWORD)sizeof(buf);
                    DWORD bytesRead = 0;
                    if (!ReadFile(hStderrRead, buf, toRead, &bytesRead, NULL) || bytesRead == 0)
                        break;
                    out.append(buf, bytesRead);
                }
            }

            processRunning  = false;
            processFinished = true;
            exitCode = lastExitCode;
            finished = true;
            return true;
        }
    }

    // ★ 修复：如果进程已经结束了，返回上次记录的退出码，而不是 -1
    if (processFinished) {
        exitCode = lastExitCode;
        finished = true;
        return true;
    }

    exitCode = -1;
    finished = false;
    return true;
}

// ---------- 向子进程 stdin 写一行 ----------
void Runtime::sendInput(const std::string& line) {
    if (!processRunning || !hStdinWrite) return;

    std::string data = line + "\n";
    DWORD bytesWritten = 0;
    WriteFile(hStdinWrite, data.c_str(), (DWORD)data.size(), &bytesWritten, NULL);
}

// ---------- 终止子进程 ----------
void Runtime::stop() {
    if (processRunning && hProcess) {
        TerminateProcess(hProcess, -1);
        WaitForSingleObject(hProcess, 1000);
        processRunning  = false;
        processFinished = true;
        lastExitCode = -1;
    }
}

// ---------- 是否仍在运行 ----------
bool Runtime::isRunning() const {
    return processRunning;
}

// ---------- 清理句柄 ----------
void Runtime::cleanup() {
    if (hStdinWrite) { CloseHandle(hStdinWrite); hStdinWrite = NULL; }
    if (hStdoutRead) { CloseHandle(hStdoutRead); hStdoutRead = NULL; }
    if (hStderrRead) { CloseHandle(hStderrRead); hStderrRead = NULL; }
    if (hThread)     { CloseHandle(hThread);     hThread     = NULL; }
    if (hProcess)    { CloseHandle(hProcess);    hProcess    = NULL; }
}