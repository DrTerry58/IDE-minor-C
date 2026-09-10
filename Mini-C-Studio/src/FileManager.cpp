// FileManager.cpp
// 用 fopen/fread/fwrite 是因为 errno 能分清"不存在/没权限/其他错误"，
// 正好对应三个错误码。全用二进制模式，换行自己处理。
// GBK 编码保存。

#include "FileManager.h"

#include <cstdio>
#include <cerrno>

FileErr FileManager::classifyErrno(int err)
{
    switch (err)
    {
    case ENOENT:  return FILE_NOT_FOUND;
    case EACCES:
    case EPERM:
    case EROFS:   return FILE_NO_PERMISSION;
    default:      return FILE_IO_ERROR;   // 磁盘满(ENOSPC)也落在这
    }
}

void FileManager::stripCR(std::string& s)
{
    std::string::size_type w = 0;
    for (std::string::size_type r = 0; r < s.size(); ++r)
        if (s[r] != '\r')
            s[w++] = s[r];
    s.resize(w);
}

FileErr FileManager::newFile()
{
    return FILE_OK;
}

FileErr FileManager::openFile(const std::string& path, std::string& outText)
{
    outText.clear();

    errno = 0;
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp)
        return classifyErrno(errno);

    char buf[65536];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, fp)) > 0)
        outText.append(buf, n);

    const bool readError = (std::ferror(fp) != 0);
    std::fclose(fp);

    if (readError)
    {
        outText.clear();
        return FILE_IO_ERROR;
    }

    stripCR(outText);
    return FILE_OK;
}

FileErr FileManager::saveFile(const std::string& path, const std::string& text)
{
    std::string t(text);
    stripCR(t);   // 先把已有的 \r 去掉，不然 \r\n 会展开成 \r\r\n

    errno = 0;
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp)
        return classifyErrno(errno);

    // 分块写，\n 前面补 \r。缓冲开 2 倍，最坏每字节都展开也够
    char buf[65536 * 2];
    std::size_t w = 0;
    bool writeError = false;

    for (std::string::size_type i = 0; i < t.size(); ++i)
    {
        if (t[i] == '\n')
            buf[w++] = '\r';
        buf[w++] = t[i];

        if (w >= sizeof buf - 2)
        {
            if (std::fwrite(buf, 1, w, fp) != w) { writeError = true; break; }
            w = 0;
        }
    }
    if (!writeError && w > 0)
        if (std::fwrite(buf, 1, w, fp) != w)
            writeError = true;

    if (std::fclose(fp) != 0)   // 磁盘满常常到 fclose 才报错
        writeError = true;

    return writeError ? FILE_IO_ERROR : FILE_OK;
}

FileErr FileManager::saveAs(const std::string& oldPath,
                            const std::string& newPath,
                            const std::string& text)
{
    (void)oldPath;
    return saveFile(newPath, text);
}

bool FileManager::fileExists(const std::string& path)
{
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;
    std::fclose(fp);
    return true;
}

const char* FileManager::fileErrMsg(FileErr e)
{
    switch (e)
    {
    case FILE_OK:            return "操作成功";
    case FILE_NOT_FOUND:     return "文件不存在，请检查路径";
    case FILE_NO_PERMISSION: return "权限不足或文件被其他程序占用";
    case FILE_IO_ERROR:      return "读写失败：磁盘已满、路径非法或文件已损坏";
    case FILE_BAD_EXT:       return "不是 .c 源文件";
    case FILE_CANCELLED:     return "操作已取消";
    default:                 return "未知错误";
    }
}
