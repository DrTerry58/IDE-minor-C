#include "AIClient.h"
#include <iostream>
#include <windows.h>

int main() {
    SetConsoleOutputCP(CP_UTF8);
    AIClient client;
    std::string reply = client.sendMessage("你好");
    if (!reply.empty()) {
        std::cout << "Reply: " << reply << std::endl;
    }
    else {
        std::cerr << "Error: " << client.lastError() << std::endl;
    }
    return 0;
}