// FileManager.h
// E: 文件管理（新建/打开/保存/另存为），只管磁盘读写。
// 不碰 EditorBuffer，脏标记在 B 那边，不归这里管。
// 接口按 C 的《接口对接说明》3.2 节。
//
// 换行：内存里统一 \n，写盘时转成 \r\n，读进来时把 \r 去掉。
// 转换在这边做，C 不要再转一次。
// 文件要用 GBK 编码保存，不然 fileErrMsg 的中文会乱码。

#ifndef MINIC_FILEMANAGER_H
#define MINIC_FILEMANAGER_H

#include <string>

// 错误码，C 拿到后调 fileErrMsg 弹窗
enum FileErr
{
    FILE_OK = 0,        // 成功
    FILE_NOT_FOUND,     // 文件不存在
    FILE_NO_PERMISSION, // 只读、被占用、没权限
    FILE_IO_ERROR,      // 剩下的读写失败（磁盘满、路径不合法之类）
    FILE_BAD_EXT,       // 不是 .c（暂时不检查，先留着）
    FILE_CANCELLED      // 用户取消（给 C 的对话框留的，这里用不到）
};

class FileManager
{
public:
    FileManager() {}

    // 新建。磁盘上没活，返回 FILE_OK 就行；
    // 清空编辑区由 C 调 B 的 loadFromString("")
    FileErr newFile();

    // 把整个文件读进 outText，失败就清空。读进来顺手把 \r 去掉
    FileErr openFile(const std::string& path, std::string& outText);

    // 把 text 写到 path（覆盖）。写之前把 \n 转成 \r\n
    FileErr saveFile(const std::string& path, const std::string& text);

    // 另存为。text 写到 newPath，老文件不动
    FileErr saveAs(const std::string& oldPath,
                   const std::string& newPath,
                   const std::string& text);

    // 文件在不在（CoreApi 那边要用）
    bool fileExists(const std::string& path);

    // 错误码翻译成中文，能直接显示
    static const char* fileErrMsg(FileErr e);

private:
    static FileErr classifyErrno(int err);
    static void stripCR(std::string& s);   // 删掉所有 \r
};

#endif // MINIC_FILEMANAGER_H
