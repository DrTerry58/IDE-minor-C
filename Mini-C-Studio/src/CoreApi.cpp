// ============================================================================
// 文件名：CoreApi.cpp
// 职责：门面层的实现 —— 在这里决定"用占位实现"还是"用队友的真实实现"
//
// 【对接步骤（给组长 A 看）】
//   1. B 同学把 EditorBuffer.h/.cpp 放进工程（已实现，方法名与 MiniStub.h 中
//      MiniBuffer 大体一致；少数 MiniBuffer 有而 EditorBuffer 没有的函数，
//      由 C 的 EditorExt 桥接层用 B 的现有原语组合，无需 B 额外补函数）；
//   2. 把下面的 USE_B_REAL_BUFFER 改成 1，并 include "EditorExt.h"
//      （EditorExt.h 内含 EditorBuffer.h，并提供桥接函数）；
//   3. E / D 同理。
//   4. 注意：真实类的成员函数签名若与 MiniBuffer 不完全一致，由 EditorExt 在
//      桥接层适配，GUI / CoreApi 门面一行都不用改 —— 这正是"接口契约 + 桥接层"的设计。
// ============================================================================

#include "CoreApi.h"

// ============================ 对接开关 ============================
#define USE_B_REAL_BUFFER    1    // B 同学：自研文本缓冲区 EditorBuffer（已接入并编译验证通过）
#define USE_E_REAL_FILEMGR   1    // E 同学：文件管理 FileManager（已交付并接入）
#define USE_D_REAL_COMPILER  1    // D 同学：编译调度 Compiler（2026-09-11 已按契约改造为类成员函数，已接入）
#define USE_D_REAL_RUNTIME   1    // D 同学：运行时托管 Runtime（2026-09-11 新交付，异步轮询，已接入）

// ---------- 队友真实实现的头文件（改宏为 1 后取消注释） ----------
#if USE_B_REAL_BUFFER
// EditorExt.h 内含 EditorBuffer.h，并提供 C 侧的桥接函数
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

// ---------- 全局实例（GUI 只通过 CoreApi 访问它们） ----------
static BufferImpl   g_buffer;
static FileMgrImpl  g_fileMgr;
static CompilerImpl g_compiler;
static RuntimeImpl  g_runtime;

CoreApi& CoreApi::inst()
{
    static CoreApi api;
    return api;
}

// ============================================================================
//  B：文本缓冲区
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

// 以下方法：真实缓冲区(EditorBuffer)无同名函数，或需要坐标转换，
// 统一走 C 的 EditorExt 桥接层；占位实现(MiniBuffer)有同名函数则直接调。
#if USE_B_REAL_BUFFER
void CoreApi::bufInsertString(const std::string& s) { ::bufInsertString(&g_buffer, s); }
void CoreApi::bufInsertAt(int r, int c, const std::string& s) { ::bufInsertAt(&g_buffer, r, c, s); }
void CoreApi::bufDeleteRange(int r1,int c1,int r2,int c2)     { ::bufDeleteRange(&g_buffer, r1, c1, r2, c2); }
std::string CoreApi::bufGetRange(int r1,int c1,int r2,int c2) { return ::bufRangeText(&g_buffer, r1, c1, r2, c2); }
void CoreApi::bufClear()                             { ::bufClear(&g_buffer); }
void CoreApi::bufGotoLine(int row)                   { ::bufGotoLine(&g_buffer, row); }
// bufSetCursor 必须走桥接层处理 (列,行) 顺序，不能直调 B::setCursor(row,col) 以免行列颠倒
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
//  E：文件生命周期
// ============================================================================
#if USE_E_REAL_FILEMGR
// E 的真实 FileManager 按契约 3.2 交付：只管磁盘读写、不碰缓冲区，
// 因此"磁盘 <-> 缓冲区"的转换由 C 在此桥接（契约第 4 节：适配由 C 侧完成）。
//
// ⚠ 换行约定：saveToString() 的输出必须【原样】交给 E 的 saveFile，
//   E 写盘时自己把 \n 转成 \r\n、读盘时自己把 \r 去掉。
//   C 侧若预先转换，会出现 \r\r\n 双重转换错误。

bool CoreApi::fileNew()
{
    m_lastError.clear();
    g_fileMgr.newFile();               // 磁盘上无操作，恒为 FILE_OK
    g_buffer.loadFromString("");       // 清空编辑区（B 的缓冲区负责）
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
        return false;                  // 失败时缓冲区保持原样，不覆盖用户内容
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
//  D：编译
// ============================================================================
// 测试钩子：设了环境变量 MINIC_FORCE_NO_COMPILER 就强制返回"未找到编译器"，
// 便于本机装有 gcc 时仍能测到异常分支（详见测试流程文档 附录 A）。
// 放在门面层，D 的 Compiler 代码不受影响。
bool          CoreApi::compilerAvailable()
{
    if (std::getenv("MINIC_FORCE_NO_COMPILER")) return false;
    return g_compiler.available();
}
CompileResult CoreApi::compile(const std::string& srcPath) { return g_compiler.compile(srcPath); }
std::string   CoreApi::exePathOf(const std::string& src)  { return g_compiler.exePathOf(src); }

// ============================================================================
//  D：运行
// ============================================================================
bool CoreApi::runStart(const std::string& exePath)              { return g_runtime.start(exePath); }
bool CoreApi::runPoll(std::string& out, int& code, bool& fin)   { return g_runtime.poll(out, code, fin); }
void CoreApi::runSendInput(const std::string& line)             { g_runtime.sendInput(line); }
void CoreApi::runStop()                                         { g_runtime.stop(); }
bool CoreApi::runIsRunning()                                    { return g_runtime.isRunning(); }
