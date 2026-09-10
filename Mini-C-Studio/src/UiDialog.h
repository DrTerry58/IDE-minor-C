// ============================================================================
// 文件名：UiDialog.h
// 职责：自绘弹窗（消息框 / 确认框 / 输入框），用于各类异常提示
// 负责人：C（GUI 界面 + 交互控制）
//
// 覆盖 PPT 52 页要求：文件读写失败、编译器未找到、源码为空、运行超时等场景
// 都要有弹窗提示。
// ============================================================================

#pragma once
#ifndef MINIC_UIDIALOG_H
#define MINIC_UIDIALOG_H

#include "MiniCConfig.h"

// 按钮组合
enum DlgBtn
{
    DLG_OK           = 1,
    DLG_OKCANCEL     = 2,
    DLG_YESNO        = 3,
    DLG_YESNOCANCEL  = 4
};

// 返回值
enum DlgRet
{
    RET_NONE   = 0,
    RET_OK     = 1,
    RET_CANCEL = 2,
    RET_YES    = 3,
    RET_NO     = 4
};

// 自绘模态消息框（自带消息循环，返回后主循环会重绘整屏）
DlgRet uiMessageBox(int winW, int winH, const Theme& th,
                    const std::string& title,
                    const std::string& text,
                    DlgBtn buttons = DLG_OK);

// 便捷：只带"确定"的提示框
inline void uiAlert(int winW, int winH, const Theme& th,
                    const std::string& title, const std::string& text)
{
    uiMessageBox(winW, winH, th, title, text, DLG_OK);
}

// 文本输入框（底层调用 EasyX 的 InputBox，支持中文 / 输入法）
// 弹出前会用 th/winW/winH 在 EasyX 画布上画一层网点遮罩，遮住背后的菜单/工具栏/编辑器（T6.7）
// 返回 false 表示用户点"取消"
bool uiInputBox(int winW, int winH, const Theme& th,
                const std::string& title, const std::string& prompt,
                std::string& out, const std::string& defValue = "");

// 选择文件保存/打开路径（简易版：手动输入路径）
//   真实版可换成 GetOpenFileName / GetSaveFileName，见 UiDialog.cpp 注释
bool uiPickPath(int winW, int winH, const Theme& th,
                const std::string& title, std::string& path, bool forSave);

#endif // MINIC_UIDIALOG_H
