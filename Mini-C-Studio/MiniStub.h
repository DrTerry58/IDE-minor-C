// ============================================================================
// 文件名：MiniStub.h
// 职责：还没交付的队友模块的【占位实现】
// 负责人：C（只用于调通界面，不属于 C 的正式交付物）
//
// ★ 当前状态（2026-09-09 合并队友代码后）：
//   · D 同学已经交付 Compiler.h / Compiler.cpp（编译调度 + 运行托管，自由函数
//     compile / compilerAvailable / runSync），所以 MiniCompiler / MiniRuntime
//     这两个占位【已删除】，CoreApi.cpp 直接调 D 的真实实现。
//   · E 同学（文件管理）尚未交付，因此 MiniFileManager 这段占位【保留】。
//     E 交付后：把 CoreApi.cpp 顶部的 USE_E_REAL_FILEMGR 从 0 改成 1，
//     并 include E 的 FileManager.h 即可，GUI 层一行都不用改。
//
// ！！！给 E 同学看！！！
// 下面 MiniFileManager 的成员函数签名 = C 同学（GUI）对你模块的【全部调用需求】。
// 你只需要：自己新建同名的类（FileManager），把这些函数实现一遍；
// 然后打开 CoreApi.cpp，把 USE_E_REAL_FILEMGR 改成 1 并 include 你的头文件。
// 除此之外，GUI 层一行代码都不用改。
//
// 注意：B 同学（文本缓冲区）的占位实现在 EditorBuffer.h，不在这里。
// ============================================================================

#pragma once
#ifndef MINIC_STUB_H
#define MINIC_STUB_H

#include "MiniCConfig.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

// ============================================================================
//  E 同学：文件生命周期管理（占位：直接用 fstream；真实实现请加错误码）
//  铁律：只负责读写磁盘，不碰 EditorBuffer，不自己维护 isDirty。
// ============================================================================
class MiniFileManager
{
public:
    // 读文件到 outText，成功 true
    bool openFile(const std::string& path, std::string& outText)
    {
        std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
        if (!in.is_open()) return false;
        std::ostringstream ss;
        ss << in.rdbuf();
        outText = ss.str();
        return true;
    }

    // 写文件，成功 true
    bool saveFile(const std::string& path, const std::string& text)
    {
        std::ofstream out(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(text.data(), (std::streamsize)text.size());
        return out.good();
    }

    bool fileExists(const std::string& path)
    {
        std::ifstream in(path.c_str());
        return in.good();
    }
};

#endif // MINIC_STUB_H
