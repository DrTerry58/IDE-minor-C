// ============================================================================
// 文件名：SplashScreen.h
// 职责：启动欢迎界面（LOGO + 进度条，默认展示 2 秒后进入主界面）
// 负责人：C（GUI 界面 + 交互控制）
// ============================================================================

#pragma once
#ifndef MINIC_SPLASH_H
#define MINIC_SPLASH_H

#include "MiniCConfig.h"

// 显示启动欢迎界面。
//   totalMs : 展示时长（毫秒），默认 MINIC_SPLASH_MS = 2000
// 行为：
//   · 单独创建一个无边框感的图形窗口绘制 LOGO；
//   · 期间点击鼠标 / 按任意键可跳过；
//   · 结束（或跳过）后自动 closegraph()，主窗口随后重新 initgraph。
void showSplashScreen(int totalMs = MINIC_SPLASH_MS);

// 只画 LOGO 本体（供以后"关于"对话框复用）
//   cx, cy : LOGO 中心点；size : 边长（像素）
void drawMiniCLogo(int cx, int cy, int size, double progress = 1.0);

#endif // MINIC_SPLASH_H
