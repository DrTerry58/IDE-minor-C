#pragma once
#include <string>

class AIClient {
public:
    AIClient();
    std::string sendMessage(const std::string& prompt);
    bool isConfigured() const;
private:
    std::string apiKey;
};