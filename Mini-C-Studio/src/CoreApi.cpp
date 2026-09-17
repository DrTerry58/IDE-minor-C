// ============================================================================
// 文件名：CoreApi.cpp
// 职责：门面层实现 —— 决定使用占位实现还是队友的真实实现
//
// 对接说明：
//   1. 文本缓冲区、文件管理、编译调度、运行时托管均已接入真实实现；
//   2. 未实现的模块可由宏开关切回 MiniStub 占位版本；
//   3. GUI 仅通过 CoreApi 访问底层模块，切换实现无需修改界面代码。
// ============================================================================

#include "CoreApi.h"

#include "AIClient.h"   // AI 客户端接口

#include <windows.h>    // UTF-8 → GBK 转换所需

// ----------------------------------------------------------------------------
// AIClient::sendMessage 返回 UTF-8 字符串，EasyX 面板按 GBK 渲染，
// 因此在写入 m_aiHistory 前统一转为 GBK，避免面板显示乱码。
// 转换失败时返回原字符串，保证调用方不会拿到空串。
// ----------------------------------------------------------------------------
namespace
{
    std::string utf8ToGbk(const std::string& utf8)
    {
        if (utf8.empty())
        {
            return utf8;
        }

        int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                       static_cast<int>(utf8.size()),
                                       NULL, 0);
        if (wlen <= 0)
        {
            return utf8;
        }

        std::wstring wstr(static_cast<size_t>(wlen), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                            static_cast<int>(utf8.size()),
                            &wstr[0], wlen);

        int glen = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wlen,
                                       NULL, 0, NULL, NULL);
        if (glen <= 0)
        {
            return utf8;
        }

        std::string gbk(static_cast<size_t>(glen), '\0');
        WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wlen,
                            &gbk[0], glen, NULL, NULL);
        return gbk;
    }
}

// ============================ 对接开关 ============================
#define USE_B_REAL_BUFFER    1    // 文本缓冲区：EditorBuffer
#define USE_E_REAL_FILEMGR   1    // 文件管理：FileManager
#define USE_D_REAL_COMPILER  1    // 编译调度：Compiler
#define USE_D_REAL_RUNTIME   1    // 运行时托管：Runtime

// ---------- 队友真实实现的头文件 ----------
#if USE_B_REAL_BUFFER
// EditorExt.h 内含 EditorBuffer.h，并提供桥接函数
// （bufInsertAt / bufDeleteRange / bufClear / bufGotoLine / bufSetCursor 等）
#include "EditorExt.h"
#endif
#if USE_E_REAL_FILEMGR
#include "FileManager.h"
#endif
#if USE_D_REAL_COMPILER
#include "Compiler.h"
#endif
#if USE_D_REAL_RUNTIME
#include "Runtime.h"
#endif

#include "MiniStub.h"

// ---------- 统一类型别名：一行切换实现 ----------
#if USE_B_REAL_BUFFER
typedef EditorBuffer   BufferImpl;
#else
typedef MiniBuffer     BufferImpl;
#endif

#if USE_E_REAL_FILEMGR
typedef FileManager    FileMgrImpl;
#else
typedef MiniFileManager<BufferImpl> FileMgrImpl;
#endif

#if USE_D_REAL_COMPILER
typedef Compiler       CompilerImpl;
#else
typedef MiniCompiler   CompilerImpl;
#endif

#if USE_D_REAL_RUNTIME
typedef Runtime        RuntimeImpl;
#else
typedef MiniRuntime    RuntimeImpl;
#endif

// ---------- 全局实例（GUI 只通过 CoreApi 访问） ----------
static BufferImpl   g_buffer;
static FileMgrImpl  g_fileMgr;
static CompilerImpl g_compiler;
static RuntimeImpl  g_runtime;

static AIClient     g_ai;       // AI 客户端实例

CoreApi& CoreApi::inst()
{
    static CoreApi api;
    return api;
}

// ============================================================================
//  文本缓冲区
// ============================================================================
int         CoreApi::bufLineCount()                  { return g_buffer.getLineCount(); }
std::string CoreApi::bufGetLine(int row)             { return g_buffer.getLine(row); }
void        CoreApi::bufInsertChar(char c)           { g_buffer.insertChar(c); }
void        CoreApi::bufDeleteBack()                 { g_buffer.deleteChar(); }
void        CoreApi::bufDeleteForward()              { g_buffer.deleteForward(); }
void        CoreApi::bufEnter()                      { g_buffer.enter(); }
void        CoreApi::bufMoveCursor(int dx, int dy)   { g_buffer.moveCursor(dx, dy); }
int         CoreApi::bufRow()                        { return g_buffer.getCursorY(); }
int         CoreApi::bufCol()                        { return g_buffer.getCursorX(); }
bool        CoreApi::bufDirty()                      { return g_buffer.isDirty(); }
void        CoreApi::bufSetDirty(bool d)             { g_buffer.setDirty(d); }
std::string CoreApi::bufPath()                       { return g_buffer.getFilePath(); }
void        CoreApi::bufSetPath(const std::string& p){ g_buffer.setFilePath(p); }
void        CoreApi::bufUndo()                       { g_buffer.undo(); }
void        CoreApi::bufRedo()                       { g_buffer.redo(); }
bool        CoreApi::bufCanUndo()                    { return g_buffer.canUndo(); }
bool        CoreApi::bufCanRedo()                    { return g_buffer.canRedo(); }
void        CoreApi::bufLoad(const std::string& t)   { g_buffer.loadFromString(t); }
std::string CoreApi::bufSave()                       { return g_buffer.saveToString(); }

// 以下方法 EditorBuffer 无同名函数，或需坐标转换，统一走 EditorExt 桥接层；
// 占位实现 MiniBuffer 有同名函数则直接调用。
#if USE_B_REAL_BUFFER
void CoreApi::bufInsertString(const std::string& s) { ::bufInsertString(&g_buffer, s); }
void CoreApi::bufInsertAt(int r, int c, const std::string& s) { ::bufInsertAt(&g_buffer, r, c, s); }
void CoreApi::bufDeleteRange(int r1,int c1,int r2,int c2)     { ::bufDeleteRange(&g_buffer, r1, c1, r2, c2); }
std::string CoreApi::bufGetRange(int r1,int c1,int r2,int c2) { return ::bufRangeText(&g_buffer, r1, c1, r2, c2); }
void CoreApi::bufClear()                             { ::bufClear(&g_buffer); }
void CoreApi::bufGotoLine(int row)                   { ::bufGotoLine(&g_buffer, row); }
// bufSetCursor 需经桥接层处理 (列,行) 顺序，避免行列颠倒
void CoreApi::bufSetCursor(int row, int col)         { ::bufSetCursor(&g_buffer, row, col); }
#else
void CoreApi::bufInsertString(const std::string& s) { g_buffer.insertString(s); }
void CoreApi::bufInsertAt(int r, int c, const std::string& s) { g_buffer.insertAt(r, c, s); }
void CoreApi::bufDeleteRange(int r1,int c1,int r2,int c2)     { g_buffer.deleteRange(r1, c1, r2, c2); }
std::string CoreApi::bufGetRange(int r1,int c1,int r2,int c2) { return g_buffer.getRangeText(r1, c1, r2, c2); }
void CoreApi::bufClear()                             { g_buffer.clear(); }
void CoreApi::bufGotoLine(int row)                   { g_buffer.gotoLine(row); }
void CoreApi::bufSetCursor(int row, int col)         { g_buffer.setCursor(row, col); }
#endif

// ============================================================================
//  文件生命周期
// ============================================================================
#if USE_E_REAL_FILEMGR
// FileManager 只负责磁盘读写，不操作缓冲区；
// "磁盘 <-> 缓冲区" 的转换在本层完成。
//
// 换行约定：saveToString() 的输出原样交给 saveFile，
// 由 FileManager 负责 \n 与 \r\n 的转换，此处不做预处理。

bool CoreApi::fileNew()
{
    m_lastError.clear();
    g_fileMgr.newFile();               // 磁盘上无操作，恒为 FILE_OK
    g_buffer.loadFromString("");       // 清空编辑区
    g_buffer.setFilePath("");
    g_buffer.setDirty(false);
    return true;
}

bool CoreApi::fileOpen(const std::string& path)
{
    m_lastError.clear();
    std::string text;
    FileErr e = g_fileMgr.openFile(path, text);
    if (e != FILE_OK)
    {
        m_lastError = FileManager::fileErrMsg(e);
        return false;                  // 失败时保留原缓冲区内容，不覆盖用户数据
    }
    g_buffer.loadFromString(text);
    g_buffer.setFilePath(path);
    g_buffer.setDirty(false);
    return true;
}

bool CoreApi::fileSave(const std::string& path)
{
    m_lastError.clear();
    FileErr e = g_fileMgr.saveFile(path, g_buffer.saveToString());
    if (e != FILE_OK)
    {
        m_lastError = FileManager::fileErrMsg(e);
        return false;
    }
    g_buffer.setDirty(false);
    g_buffer.setFilePath(path);
    return true;
}

const char* CoreApi::lastError() const
{
    return m_lastError.empty() ? "" : m_lastError.c_str();
}

#else
bool CoreApi::fileNew()                              { return g_fileMgr.newFile(g_buffer); }
bool CoreApi::fileOpen(const std::string& path)      { return g_fileMgr.openFile(path, g_buffer); }
bool CoreApi::fileSave(const std::string& path)      { return g_fileMgr.saveFile(path, g_buffer); }
const char* CoreApi::lastError() const               { return ""; }
#endif

// ============================================================================
//  编译
// ============================================================================
// 测试钩子：设置环境变量 MINIC_FORCE_NO_COMPILER 可强制返回"未找到编译器"，
// 便于本机装有 gcc 时验证异常分支。该钩子仅作用于门面层，不影响 Compiler 内部。
bool          CoreApi::compilerAvailable()
{
    if (std::getenv("MINIC_FORCE_NO_COMPILER")) return false;
    return g_compiler.available();
}
CompileResult CoreApi::compile(const std::string& srcPath) { return g_compiler.compile(srcPath); }
std::string   CoreApi::exePathOf(const std::string& src)  { return g_compiler.exePathOf(src); }

// ============================================================================
//  运行
// ============================================================================
bool CoreApi::runStart(const std::string& exePath)              { return g_runtime.start(exePath); }
bool CoreApi::runPoll(std::string& out, int& code, bool& fin)   { return g_runtime.poll(out, code, fin); }
void CoreApi::runSendInput(const std::string& line)             { g_runtime.sendInput(line); }
void CoreApi::runStop()                                         { g_runtime.stop(); }
bool CoreApi::runIsRunning()                                    { return g_runtime.isRunning(); }

// ============================================================================
//  AI 助手
// ============================================================================

// ----------------------------------------------------------------------------
// 查询 AI 是否可用。AIClient 通过环境变量 DEEPSEEK_API_KEY 判断配置状态。
// 未配置时返回 false，GUI 面板显示占位模式提示。
// ----------------------------------------------------------------------------
bool CoreApi::aiAvailable() const
{
    return g_ai.isConfigured();
}

// ----------------------------------------------------------------------------
// 发送提问。将 user / assistant 两条消息追加到 m_aiHistory，
// 供 GUI 面板读取以显示多轮对话。
// sendMessage 返回 UTF-8，此处转为 GBK 后再写入历史，避免面板乱码。
// ----------------------------------------------------------------------------
std::string CoreApi::aiAsk(const std::string& prompt)
{
    if (prompt.empty())
    {
        return std::string();
    }

    m_aiHistory.push_back(AIMessage("user", prompt));
    std::string reply = utf8ToGbk(g_ai.sendMessage(prompt));
    m_aiHistory.push_back(AIMessage("assistant", reply));
    return reply;
}

// ----------------------------------------------------------------------------
// 解释选中代码：构造解释类提示词后转交 aiAsk。
// ----------------------------------------------------------------------------
std::string CoreApi::aiExplainCode(const std::string& code)
{
    std::string prompt =
        "请解释以下 C 语言代码的功能和逻辑：\n"
        "```c\n" + code + "\n```\n"
        "请用中文回答，尽量简洁。";
    return aiAsk(prompt);
}

// ----------------------------------------------------------------------------
// 根据编译诊断修复错误：构造修复类提示词后转交 aiAsk。
// diag 已由 GUI 格式化为 "行:列 级别: 描述"（见 MainWindow::execCmd 的 CMD_AI_FIX），
// 此处按契约以 std::string 接收并直接嵌入提示词。
// ----------------------------------------------------------------------------
std::string CoreApi::aiFixError(const std::string& code, const std::string& diag)
{
    std::string prompt =
        "以下 C 语言代码编译出错了。\n"
        "错误信息：\n" + diag + "\n\n"
        "代码：\n```c\n" + code + "\n```\n"
        "请给出修复建议，并指出可能出错的行。请用中文回答。";
    return aiAsk(prompt);
}

// ----------------------------------------------------------------------------
// 多轮对话上下文：由 aiAsk 写入（每次追加 user / assistant 两条），
// UI 通过本方法读取以渲染对话历史，请勿重复 push。
// ----------------------------------------------------------------------------
const std::vector<AIMessage>& CoreApi::aiHistory() const
{
    return m_aiHistory;
}

// ----------------------------------------------------------------------------
// 清空对话历史（UI “清空”按钮调用）。
// ----------------------------------------------------------------------------
void CoreApi::aiClearHistory()
{
    m_aiHistory.clear();
}
