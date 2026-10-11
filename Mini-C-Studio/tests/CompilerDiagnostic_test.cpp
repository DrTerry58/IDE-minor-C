#include "../src/Compiler.h"
#include <cstdio>
#include <fstream>
#include <string>
#include <windows.h>

static std::string absolutePath(const char* path)
{
    char buffer[MAX_PATH];
    DWORD length = GetFullPathNameA(path, MAX_PATH, buffer, nullptr);
    return length > 0 && length < MAX_PATH ? std::string(buffer, length) : std::string();
}

static bool writeSource(const std::string& path, const char* source)
{
    std::ofstream file(path.c_str(), std::ios::binary);
    file << source;
    return file.good();
}

int main()
{
    Compiler compiler;
    if (!compiler.available()) {
        std::fprintf(stderr, "gcc not found\n");
        return 1;
    }

    const std::string errorPath = absolutePath("compiler_diagnostic_error.c");
    if (errorPath.empty() || !writeSource(errorPath, "int main() {\n    return 0\n}\n")) return 1;
    CompileResult error = compiler.compile(errorPath);
    bool errorFound = false;
    for (size_t i = 0; i < error.items.size(); i++) {
        const Diagnostic& diagnostic = error.items[i];
        if (diagnostic.level == DIAG_ERROR && diagnostic.line == 1 &&
            diagnostic.column == 12 && diagnostic.message.find("expected") != std::string::npos) {
            errorFound = true;
        }
    }
    std::remove(errorPath.c_str());
    std::remove(compiler.exePathOf(errorPath).c_str());
    if (error.state != CS_ERROR || !errorFound) {
        std::fprintf(stderr, "error location/message was not parsed correctly\n%s\n", error.raw.c_str());
        return 1;
    }

    const std::string warningPath = absolutePath("compiler_diagnostic_warning.c");
    if (warningPath.empty() || !writeSource(warningPath, "#warning parser warning check\nint main() { return 0; }\n")) return 1;
    CompileResult warning = compiler.compile(warningPath);
    bool warningFound = false;
    for (size_t i = 0; i < warning.items.size(); i++) {
        const Diagnostic& diagnostic = warning.items[i];
        if (diagnostic.level == DIAG_WARNING && diagnostic.line == 0 &&
            diagnostic.column == 1 && diagnostic.message.find("parser warning check") != std::string::npos) {
            warningFound = true;
        }
    }
    std::remove(warningPath.c_str());
    std::remove(compiler.exePathOf(warningPath).c_str());
    if (warning.state != CS_WARNING || !warningFound) {
        std::fprintf(stderr, "warning location/message was not parsed correctly\n%s\n", warning.raw.c_str());
        return 1;
    }

    std::puts("Compiler diagnostic tests passed.");
    return 0;
}