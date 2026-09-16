// ============================================================
// 文件名: AIClient.h
// 职责: DeepSeek AI 客户端（WinHTTP 实现）
// 负责人: D 同学
// ============================================================

#pragma once

#ifndef AI_CLIENT_H
#define AI_CLIENT_H

#include <string>

class AIClient {
public:
    AIClient();
    ~AIClient();

    // 是否已配置 API Key（读环境变量 DEEPSEEK_API_KEY）
    bool isConfigured() const { return !m_apiKey.empty(); }

    // 发送用户消息，返回 AI 回复内容；失败返回空串，错误见 lastError()
    std::string sendMessage(const std::string& prompt);

    std::string lastError() const { return m_lastError; }

private:
    std::string m_apiKey;
    std::string m_lastError;

    AIClient(const AIClient&);
    AIClient& operator=(const AIClient&);
};

#endif
