// ============================================================
// 文件名: Runtime.h
// 职责: 程序运行时托管（异步轮询模型）
// 负责人: D 同学
// 依据: 接口对接说明.docx 第 3.4 节 / MiniStub.h MiniRuntime
// 铁律: 不弹窗 / 不抛异常 / 同步接口内部非阻塞
// ============================================================

#ifndef RUNTIME_H
#define RUNTIME_H

#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

class Runtime {
public:
    Runtime();
    ~Runtime();

    // 启动子进程，重定向 stdin/stdout/stderr
    bool start(const std::string& exePath);

    // 非阻塞取回累计输出、退出码、是否结束
    // out: 新读到的输出会追加到 out 末尾
    // exitCode: 进程结束时的退出码（未结束为 -1）
    // finished: 进程是否已结束
    bool poll(std::string& out, int& exitCode, bool& finished);

    // 向子进程 stdin 写一行
    void sendInput(const std::string& line);

    // 终止子进程（"停止运行"按钮）
    void stop();

    // 是否仍在运行
    bool isRunning() const;

private:
    // 禁止拷贝（句柄资源不可共享）
    Runtime(const Runtime&);
    Runtime& operator=(const Runtime&);

    void cleanup();

    HANDLE hProcess;       // 子进程句柄
    HANDLE hThread;        // 子进程主线程句柄
    HANDLE hStdinWrite;    // 写端 → 子进程 stdin
    HANDLE hStdoutRead;    // 读端 ← 子进程 stdout
    HANDLE hStderrRead;    // 读端 ← 子进程 stderr

    int  lastExitCode;
    bool processRunning;
    bool processFinished;
};

#endif