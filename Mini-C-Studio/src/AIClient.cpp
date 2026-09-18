#define _CRT_SECURE_NO_WARNINGS

// ============================================================
// 文件名: AIClient.cpp
// 职责: DeepSeek AI 客户端实现（WinHTTP）
// 负责人: D 同学
// ============================================================

#include "AIClient.h"
#include <windows.h>
#include <winhttp.h>
#include <cstdlib>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

#include "json.hpp"
using json = nlohmann::json;

// 兼容宏：部分较旧的 MinGW winhttp.h 未定义 TLS1.2 标志，
// MSVC / 新版 Windows SDK 已自带，这里只在缺失时补定义，不影响 MSVC 行为。
#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 0x0800
#endif

static std::wstring utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring wstr(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size);
    return wstr;
}

AIClient::AIClient() {
    const char* key = std::getenv("DEEPSEEK_API_KEY");
    if (key) m_apiKey = key;
    else m_lastError = "Environment variable DEEPSEEK_API_KEY not set.";
}

AIClient::~AIClient() {}

std::string AIClient::sendMessage(const std::string& prompt) {
    m_lastError.clear();

    if (m_apiKey.empty()) {
        m_lastError = "API Key is empty. Please set DEEPSEEK_API_KEY.";
        return m_lastError;
    }
    if (prompt.empty()) {
        m_lastError = "Prompt is empty.";
        return m_lastError;
    }

    // 1. 构造 JSON 请求体
    json req;
    req["model"] = "deepseek-chat";
    req["messages"] = json::array();
    req["messages"].push_back({ {"role", "user"}, {"content", prompt} });
    std::string body = req.dump();

    // 2. 初始化 WinHTTP
    HINTERNET hSession = WinHttpOpen(L"Mini-C-Studio/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        m_lastError = "WinHttpOpen failed: " + std::to_string(GetLastError());
        return m_lastError;
    }
    DWORD dwProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &dwProtocols, sizeof(dwProtocols));

    // 3. 连接服务器
    HINTERNET hConnect = WinHttpConnect(hSession, L"api.deepseek.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        m_lastError = "WinHttpConnect failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(hSession);
        return m_lastError;
    }

    // 4. 打开 POST 请求
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/chat/completions",
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        m_lastError = "WinHttpOpenRequest failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return m_lastError;
    }

    // 5. 设置请求头
    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += L"Authorization: Bearer " + utf8ToWide(m_apiKey) + L"\r\n";

    // 6. 发送请求（UTF-8 字节流）
    BOOL ok = WinHttpSendRequest(hRequest,
        headers.c_str(), (DWORD)-1L,
        (LPVOID)body.c_str(), (DWORD)body.size(),
        (DWORD)body.size(), 0);
    if (!ok) {
        m_lastError = "WinHttpSendRequest failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return m_lastError;
    }

    // 7. 接收响应
    ok = WinHttpReceiveResponse(hRequest, NULL);
    if (!ok) {
        m_lastError = "WinHttpReceiveResponse failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return m_lastError;
    }

    // 8. 读取响应体
    std::string response;
    DWORD dwSize = 0;
    do {
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;
        std::vector<char> buf(dwSize + 1);
        DWORD downloaded = 0;
        if (!WinHttpReadData(hRequest, buf.data(), dwSize, &downloaded)) break;
        buf[downloaded] = '\0';
        response += buf.data();
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // 9. 解析 JSON 响应
    if (response.empty()) {
        m_lastError = "Empty response body.";
        return m_lastError;
    }

    try {
        json res = json::parse(response);

        // 先检查是否是错误响应（如 Insufficient Balance）
        if (res.contains("error") && res["error"].contains("message")) {
            m_lastError = "API error: " + res["error"]["message"].get<std::string>();
            return m_lastError;
        }

        // 正常响应
        if (res.contains("choices") && res["choices"].is_array() && !res["choices"].empty() &&
            res["choices"][0].contains("message") &&
            res["choices"][0]["message"].contains("content")) {
            return res["choices"][0]["message"]["content"].get<std::string>();
        }
        else {
            m_lastError = "Unexpected JSON structure. Raw: " + response;
            return m_lastError;
        }
    }
    catch (const std::exception& e) {
        m_lastError = "JSON parse error: " + std::string(e.what());
        return m_lastError;
    }
}