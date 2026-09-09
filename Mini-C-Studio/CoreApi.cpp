// ============================================================================
// 文件名：CoreApi.cpp
// 负责人：C（GUI 界面 + 交互控制）
//
// ★★★ 队友交付后，只需要改本文件最上面这三个宏 ★★★
//
// 当前（2026-09-09 合并队友代码）：
//   · D 同学已交付 Compiler.h / Compiler.cpp → USE_D_REAL_COMPILER = 1，
//     USE_D_REAL_RUNTIME = 1。编译直接调 D 的 ::compile()，运行用 RealRuntime
//     把 D 的同步 runSync 包成 GUI 需要的"启动/轮询/停止/发输入"异步接口。
//   · E 同学尚未交付 → USE_E_REAL_FILEMGR = 0，仍用 MiniFileManager 占位。
// ============================================================================

#include "CoreApi.h"

// ---------------------------------------------------------------------------
//  【对接开关】队友把模块写完并把头文件发给我之后，把对应的 0 改成 1
// ---------------------------------------------------------------------------
#define USE_E_REAL_FILEMGR    0     // E 同学交付后改 1
#define USE_D_REAL_COMPILER   1     // D 同学已交付 Compiler.h/.cpp
#define USE_D_REAL_RUNTIME     1     // D 同学已交付（runSync，由 RealRuntime 包成异步）

// ---------------------------------------------------------------------------
//  【头文件区】
// ---------------------------------------------------------------------------
#if USE_D_REAL_COMPILER
#include "Compiler.h"         // D 同学：compile / compilerAvailable / runSync（自由函数）
#endif
#if USE_E_REAL_FILEMGR
// #include "FileManager.h"      // E 同学的头文件
#endif

#include <string>
#include <thread>

#if !USE_E_REAL_FILEMGR
#  include "MiniStub.h"          // 仅 MiniFileManager（E 未交付时的占位）
#endif

// ---------------------------------------------------------------------------
//  内部对象
// ---------------------------------------------------------------------------
namespace
{
#if USE_E_REAL_FILEMGR
    FileManager  g_fileMgr;
#else
    MiniFileManager g_fileMgr;
#endif

// D 同学的运行时是【同步】runSync（阻塞到程序退出）；
// 但 C 的 GUI 是"事件循环 + 每帧轮询"的异步模型（实时显示输出、支持停止按钮）。
// 这里用一条后台线程把 runSync 包成 GUI 需要的异步接口。
// 限制：runSync 在启动时就把 stdin 一次性传进去，运行中调 sendInput 不会真正送达进程，
//       停止按钮也无法真正 kill 进程（D 的同步接口没有进程句柄）。这两点在代码里已注明。
class RealRuntime
{
public:
    RealRuntime() : m_code(0), m_running(false), m_done(false) {}
    ~RealRuntime() { if (m_thread.joinable()) m_thread.join(); }

    bool start(const std::string& exePath)
    {
        if (m_running) return false;
        if (m_thread.joinable()) m_thread.join();   // 防止上次运行残留的线程被覆盖
        m_running = true; m_done = false; m_code = 0; m_out.clear();
        std::string stdinText = m_pendingInput; m_pendingInput.clear();
        m_thread = std::thread([this, exePath, stdinText]() {
            RunResult r = ::runSync(exePath, stdinText, 0);
            m_out = r.stdoutText;
            if (!r.stderrText.empty())
            {
                if (!m_out.empty()) m_out += "\n";
                m_out += r.stderrText;
            }
            m_code = r.exitCode;
            m_running = false; m_done = true;
        });
        return true;
    }

    // 非阻塞轮询：返回 true 表示本次取到了新数据 / 状态变化
    bool poll(std::string& out, int& exitCode, bool& finished)
    {
        if (!m_done) return false;
        out = m_out; exitCode = m_code; finished = true;
        m_done = false; m_out.clear();
        return true;
    }

    // 注：D 的同步接口不支持运行中交互输入，这里仅做记录（实际不会送达进程）。
    void sendInput(const std::string& line) { m_pendingInput += line + "\n"; }

    void stop()
    {
        m_running = false; m_done = true;
        m_out += "\n[用户已请求停止：D 的同步运行接口无法真正中断进程，请等待程序自行退出]";
    }

    bool isRunning() const { return m_running; }

private:
    std::thread m_thread;
    std::string m_out;
    std::string m_pendingInput;
    int  m_code;
    bool m_running;
    bool m_done;
};

#if USE_D_REAL_COMPILER && USE_D_REAL_RUNTIME
    // D 已交付：编译直接调 ::compile，运行用 RealRuntime 包 runSync
    RealRuntime g_runtime;
#else
    MiniCompiler g_compiler;
    MiniRuntime  g_runtime;
#endif
}

CoreApi& CoreApi::inst()
{
    static CoreApi api;
    return api;
}

// ---------------- E：文件 ----------------
bool CoreApi::fileRead(const std::string& path, std::string& outText)
{
    return g_fileMgr.openFile(path, outText);
}

bool CoreApi::fileWrite(const std::string& path, const std::string& text)
{
    return g_fileMgr.saveFile(path, text);
}

bool CoreApi::fileExists(const std::string& path)
{
    return g_fileMgr.fileExists(path);
}

// ---------------- D：编译 ----------------
bool CoreApi::compilerAvailable()
{
#if USE_D_REAL_COMPILER
    std::string p;
    return ::compilerAvailable(p);
#else
    return g_compiler.available();
#endif
}

CompileResult CoreApi::compile(const std::string& srcPath)
{
#if USE_D_REAL_COMPILER
    // D 的接口是 compile(src, exe)；这里按"同名 .exe"推导输出路径，
    // 与下面 exePathOf() 保持一致，运行阶段才能找到同一个 exe。
    return ::compile(srcPath, exePathOf(srcPath));
#else
    return g_compiler.compile(srcPath);
#endif
}

std::string CoreApi::exePathOf(const std::string& srcPath)
{
    std::string base = srcPath;
    size_t p = base.find_last_of("\\/");
    std::string dir, name = base;
    if (p != std::string::npos) { dir = base.substr(0, p + 1); name = base.substr(p + 1); }
    size_t d = name.find_last_of('.');
    if (d != std::string::npos) name = name.substr(0, d);
    return dir + name + ".exe";
}

// ---------------- D：运行 ----------------
bool CoreApi::runStart(const std::string& exePath)
{
    return g_runtime.start(exePath);
}

bool CoreApi::runPoll(std::string& out, int& exitCode, bool& finished)
{
    return g_runtime.poll(out, exitCode, finished);
}

void CoreApi::runSendInput(const std::string& line)
{
    g_runtime.sendInput(line);
}

void CoreApi::runStop()
{
    g_runtime.stop();
}

bool CoreApi::runIsRunning()
{
    return g_runtime.isRunning();
}
