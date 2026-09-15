#include "AIClient.h"
#include <cstdlib>

AIClient::AIClient() {
    const char* key = std::getenv("DEEPSEEK_API_KEY");
    if (key) apiKey = key;
}

bool AIClient::isConfigured() const {
    return !apiKey.empty();
}

std::string AIClient::sendMessage(const std::string& prompt) {
    // D 同学将用 WinHTTP 替换这里
    return "AI placeholder: " + prompt;
}