#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace byte_enclave {

namespace fs = std::filesystem;

struct FileMetadata {
    std::string owner;
    std::string group;
    fs::perms permissions;
    fs::file_time_type access_time;
    fs::file_time_type modify_time;
    fs::file_time_type create_time;
    fs::file_type file_type;
};

class FileSystem {
public:
    FileSystem() = default;
    ~FileSystem() = default;

    // 读取文件元数据
    FileMetadata getFileMetadata(const fs::path& path);
    
    // 设置文件元数据
    bool setFileMetadata(const fs::path& path, const FileMetadata& metadata);
    
    // 复制文件（包括元数据）
    bool copyFile(const fs::path& src, const fs::path& dst);
    
    // 创建符号链接
    bool createSymlink(const fs::path& target, const fs::path& link);
    
    // 创建硬链接
    bool createHardlink(const fs::path& target, const fs::path& link);
    
    // 创建命名管道
    bool createNamedPipe(const fs::path& path);
    
    // 获取文件类型
    fs::file_type getFileType(const fs::path& path);
    
    // 检查文件是否可读
    bool isReadable(const fs::path& path);
    
    // 检查文件是否可写
    bool isWritable(const fs::path& path);

private:
    // 内部辅助函数
    bool copyFileMetadata(const fs::path& src, const fs::path& dst);
};

} // namespace byte_enclave 