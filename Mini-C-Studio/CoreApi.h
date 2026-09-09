// ============================================================================
// 文件名：CoreApi.h
// 负责人：C（GUI 界面 + 交互控制）
//
// 职责：GUI 层访问【E 同学文件模块 / D 同学编译与运行模块】的唯一入口（门面）。
//
// 注意：B 同学（文本缓冲区）不走这里 —— MainWindow 直接用 A 代码里的
//       buffer 成员（EditorBuffer*），见 MainWindow.cpp。
//       B 相关的适配在 EditorExt.h。
//
// 队友交付后怎么接：打开 CoreApi.cpp，把顶部三个宏从 0 改成 1，
//                   并把队友的头文件 include 进去。GUI 层一行都不用改。
// ============================================================================

#pragma once
#ifndef MINIC_COREAPI_H
#define MINIC_COREAPI_H

#include "MiniCConfig.h"
#include "CoreTypes.h"
#include <string>
#include <vector>

class CoreApi
{
public:
    static CoreApi& inst();

    // ================= E 同学：文件生命周期 =================
    // 只负责读写磁盘，不碰缓冲区。失败返回 false，由 C 负责弹窗。
    bool fileRead(const std::string& path, std::string& outText);
    bool fileWrite(const std::string& path, const std::string& text);
    bool fileExists(const std::string& path);

    // ================= D 同学：编译 =================
    bool          compilerAvailable();
    CompileResult compile(const std::string& srcPath);
    std::string   exePathOf(const std::string& srcPath);

    // ================= D 同学：运行 =================
    bool runStart(const std::string& exePath);
    bool runPoll(std::string& out, int& exitCode, bool& finished);
    void runSendInput(const std::string& line);
    void runStop();
    bool runIsRunning();

private:
    CoreApi() {}
    CoreApi(const CoreApi&);
    CoreApi& operator=(const CoreApi&);
};

#endif // MINIC_COREAPI_H
