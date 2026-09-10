// FileManager_test.cpp
// FileManager 的单元测试，控制台程序，全过返回 0。
// 编译（TDM-GCC，Dev-C++ 自带的）：
//   g++ -std=c++11 -Wall -Wextra -finput-charset=GBK -fexec-charset=GBK
//       FileManager.cpp FileManager_test.cpp -o FileManager_test.exe
// GBK 保存。

#include "FileManager.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <chrono>
#include <io.h>
#include <sys/stat.h>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* name)
{
    if (cond) { ++g_pass; std::printf("[PASS] %s\n", name); }
    else      { ++g_fail; std::printf("[FAIL] %s\n", name); }
}

// 读原始字节，用于验证磁盘上确实是 \r\n
static std::string readRaw(const char* path)
{
    std::string s;
    FILE* fp = std::fopen(path, "rb");
    if (!fp) return s;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, fp)) > 0) s.append(buf, n);
    std::fclose(fp);
    return s;
}

// 没有"前面不带 \r 的 \n"
static bool noBareLF(const std::string& s)
{
    for (std::size_t i = 0; i < s.size(); ++i)
        if (s[i] == '\n' && (i == 0 || s[i - 1] != '\r'))
            return false;
    return true;
}

int main()
{
    FileManager fm;

    // 0. 六个错误码都有非空提示
    {
        bool allNonEmpty = true;
        for (int e = FILE_OK; e <= FILE_CANCELLED; ++e)
        {
            const char* msg = FileManager::fileErrMsg((FileErr)e);
            if (!msg || std::strlen(msg) == 0) allNonEmpty = false;
        }
        check(allNonEmpty, "fileErrMsg: six codes all have non-empty message");
    }

    // 1. newFile 恒成功
    check(fm.newFile() == FILE_OK, "newFile returns FILE_OK");

    // 2. 不存在的文件
    {
        const char* ghost = "no_such_file_abc123.c";
        check(!fm.fileExists(ghost), "fileExists: ghost file -> false");
        std::string out = "should be cleared";
        FileErr e = fm.openFile(ghost, out);
        check(e == FILE_NOT_FOUND && out.empty(),
              "openFile: ghost -> FILE_NOT_FOUND, outText cleared");
    }

    // 3. 写读往返（含中文、多行）
    {
        const char* p = "test_roundtrip.c";
        const std::string text =
            "#include <stdio.h>\n"
            "int main()\n"
            "{\n"
            "    // 中文注释：你好，Mini-C Studio\n"
            "    printf(\"hello\\n\");\n"
            "    return 0;\n"
            "}\n";
        check(fm.saveFile(p, text) == FILE_OK, "saveFile: write source with Chinese -> OK");
        check(fm.fileExists(p), "fileExists: after save -> true");

        std::string back;
        check(fm.openFile(p, back) == FILE_OK && back == text,
              "openFile: round-trip text identical");

        const std::string raw = readRaw(p);
        check(raw.find("\r\n") != std::string::npos && noBareLF(raw),
              "disk bytes use CRLF");
    }

    // 4. 输入混有 \r\n 时不得写出 \r\r\n
    {
        const char* p = "test_mixed.c";
        check(fm.saveFile(p, "a\r\nb\nc") == FILE_OK, "saveFile: mixed endings -> OK");
        std::string back;
        fm.openFile(p, back);
        check(back == "a\nb\nc", "mixed input normalized");
        check(noBareLF(readRaw(p)), "mixed input: disk still pure CRLF");
    }

    // 5. 磁盘上的 \r\n 文件读入后不含 \r
    {
        const char* p = "test_crlf_src.c";
        FILE* fp = std::fopen(p, "wb");
        std::fputs("line1\r\nline2\r\n", fp);
        std::fclose(fp);
        std::string back;
        check(fm.openFile(p, back) == FILE_OK && back == "line1\nline2\n",
              "openFile: CRLF file read back without CR");
    }

    // 6. 空文件往返
    {
        const char* p = "test_empty.c";
        check(fm.saveFile(p, "") == FILE_OK, "saveFile: empty content -> OK");
        std::string back = "x";
        check(fm.openFile(p, back) == FILE_OK && back.empty(),
              "openFile: empty file round-trip");
    }

    // 7. saveAs：新路径有内容，原文件不动
    {
        const char* oldP = "test_saveas_old.c";
        const char* newP = "test_saveas_new.c";
        fm.saveFile(oldP, "old content\n");
        check(fm.saveAs(oldP, newP, "new content\n") == FILE_OK,
              "saveAs: write to new path -> OK");
        std::string o, n;
        fm.openFile(oldP, o);
        fm.openFile(newP, n);
        check(o == "old content\n" && n == "new content\n",
              "saveAs: old untouched, new written");
    }

    // 8. 打开目录 -> 报错但不崩
    {
        std::string out;
        check(fm.openFile(".", out) != FILE_OK && out.empty(),
              "openFile: directory path -> error, no crash");
    }

    // 9. 保存到不存在的盘符/目录 -> 报错但不崩
    check(fm.saveFile("Z:\\__no_such_dir_zz__\\a.c", "x") != FILE_OK,
          "saveFile: bad drive/dir -> error, no crash");

    // 10. 只读文件 -> FILE_NO_PERMISSION
    {
        const char* p = "test_readonly.c";
        fm.saveFile(p, "readonly\n");
        _chmod(p, _S_IREAD);
        FileErr e = fm.saveFile(p, "try to overwrite\n");
        _chmod(p, _S_IREAD | _S_IWRITE);
        check(e == FILE_NO_PERMISSION, "saveFile: read-only -> FILE_NO_PERMISSION");
    }

    // 11. 性能：2000 行读写 <= 1s（PPT 指标）
    {
        const char* p = "test_2000.c";
        std::string big;
        for (int i = 0; i < 2000; ++i)
        {
            char line[80];
            std::sprintf(line, "int x%d = %d;  // line %d with some padding text\n", i, i, i);
            big += line;
        }
        const auto t0 = std::chrono::steady_clock::now();
        FileErr e1 = fm.saveFile(p, big);
        std::string back;
        FileErr e2 = fm.openFile(p, back);
        const auto t1 = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::printf("       2000-line write+read: %.1f ms\n", ms);
        check(e1 == FILE_OK && e2 == FILE_OK && back == big && ms < 1000.0,
              "2000-line write+read under 1s");
    }

    // 清理
    const char* files[] = {
        "test_roundtrip.c", "test_mixed.c", "test_crlf_src.c", "test_empty.c",
        "test_saveas_old.c", "test_saveas_new.c", "test_readonly.c", "test_2000.c"
    };
    for (const char* f : files) std::remove(f);

    std::printf("----------------------------------------\n");
    std::printf("PASS %d, FAIL %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
