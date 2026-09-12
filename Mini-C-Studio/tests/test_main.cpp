// ============================================================
// test_main.cpp —— D 模块独立测试（本地跑完即可删除，不提交）
// ============================================================

#include "Compiler.h"
#include "Runtime.h"
#include <cstdio>
#include <cstdlib>
#include <string>
// ============================================================
// test_main.cpp -- D module standalone test
// Run locally only, do NOT commit to repo.
// ============================================================

#include "Compiler.h"
#include "Runtime.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <windows.h>

int main()
{
    printf("========== D Module Test Start ==========\n\n");

    // ---------------- 1. Test Compiler ----------------
    printf("[1] Test Compiler\n");
    Compiler compiler;

    // 1.1 Check gcc availability
    bool ok = compiler.available();
    printf("    gcc available: %s\n", ok ? "yes" : "no");
    if (!ok) {
        printf("    [FAIL] gcc not found. Please make sure MinGW is in PATH.\n");
        printf("    Press any key to exit...\n");
        getchar();
        return 1;
    }

    // 1.2 Test exePathOf
    std::string exePath = compiler.exePathOf("test.c");
    printf("    exePathOf(\"test.c\") = %s\n", exePath.c_str());

    // 1.3 Create a source file
    const char* srcCode =
        "#include <stdio.h>\n"
        "int main() {\n"
        "    printf(\"Hello from D module!\\n\");\n"
        "    return 0;\n"
        "}\n";
    FILE* fp = fopen("test.c", "w");
    if (!fp) {
        printf("    [FAIL] Cannot create test.c\n");
        return 1;
    }
    fwrite(srcCode, 1, strlen(srcCode), fp);
    fclose(fp);
    printf("    test.c generated\n");

    // 1.4 Compile
    printf("    Compiling...\n");
    CompileResult cr = compiler.compile("test.c");

    printf("    Compile state = %d  (0=NONE 1=OK 2=WARNING 3=ERROR 4=NOCOMPILER 5=TIMEOUT 6=INTERNAL)\n",
           (int)cr.state);
    printf("    exePath = %s\n", cr.exePath.c_str());
    printf("    Diagnostic count = %d\n", (int)cr.items.size());
    for (size_t i = 0; i < cr.items.size(); i++) {
        const Diagnostic& d = cr.items[i];
        printf("      - [level=%d] line=%d column=%d tag=%s msg=%s\n",
               (int)d.level, d.line, d.column, d.tag.c_str(), d.message.c_str());
    }
    printf("    raw output:\n%s\n", cr.raw.c_str());

    if (cr.state != CS_OK && cr.state != CS_WARNING) {
        printf("    [FAIL] Compilation was not successful\n");
        printf("    Press any key to exit...\n");
        getchar();
        return 1;
    }
    printf("    [PASS] Compilation succeeded\n\n");

    // ---------------- 2. Test Runtime ----------------
    printf("[2] Test Runtime\n");
    Runtime runtime;

    bool started = runtime.start("test.exe");
    printf("    start(): %s\n", started ? "success" : "failed");
    if (!started) {
        printf("    [FAIL] Cannot start test.exe\n");
        printf("    Press any key to exit...\n");
        getchar();
        return 1;
    }

    // Poll until finish
    std::string output;
    int exitCode = -1;
    bool finished = false;
    int loopCount = 0;

    while (runtime.isRunning() && loopCount < 200) {   // max ~6s
        std::string chunk;
        runtime.poll(chunk, exitCode, finished);
        output += chunk;
        Sleep(30);
        loopCount++;
    }

    // Final poll to get exit code
    {
        std::string chunk;
        runtime.poll(chunk, exitCode, finished);
        output += chunk;
    }

    printf("    Accumulated output:\n%s\n", output.c_str());
    printf("    Exit code = %d\n", exitCode);
    printf("    Finished = %s\n", finished ? "yes" : "no");

    if (exitCode == 0 && output.find("Hello from D module!") != std::string::npos) {
        printf("    [PASS] Runtime test passed\n\n");
    } else {
        printf("    [WARN] Runtime output not as expected\n\n");
    }

    // ---------------- 3. Test Compile Error Parsing ----------------
    printf("[3] Test Compile Error Parsing\n");
    const char* badCode = "int main() { return 0 }";   // missing semicolon
    fp = fopen("bad.c", "w");
    if (fp) {
        fwrite(badCode, 1, strlen(badCode), fp);
        fclose(fp);

        CompileResult bad = compiler.compile("bad.c");
        printf("    Compile state = %d (expect 3=ERROR)\n", (int)bad.state);
        printf("    Diagnostic count = %d\n", (int)bad.items.size());
        for (size_t i = 0; i < bad.items.size(); i++) {
            const Diagnostic& d = bad.items[i];
            printf("      - [level=%d] line=%d column=%d tag=%s msg=%s\n",
                   (int)d.level, d.line, d.column, d.tag.c_str(), d.message.c_str());
        }
        if (bad.state == CS_ERROR && !bad.items.empty()) {
            printf("    [PASS] Error parsing works\n");
        } else {
            printf("    [WARN] Error parsing has issues\n");
        }
    }

    printf("\n========== Test End ==========\n");
    printf("Press any key to exit...\n");
    getchar();
    return 0;
}