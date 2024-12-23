#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace byte_enclave {

// 使用 std::filesystem 的命名空间别名
namespace fs = std::filesystem;

/**
 * @brief 文件元数据结构体
 * 存储文件的基本属性信息，包括所有权、权限和时间戳
 */
struct FileMetadata {
    std::string owner;          // 文件所有者用户名
    std::string group;          // 文件所属组名
    fs::perms permissions;      // 文件权限
    fs::file_time_type access_time;    // 最后访问时间
    fs::file_time_type modify_time;    // 最后修改时间
    fs::file_time_type create_time;    // 创建时间
    fs::file_type file_type;    // 文件类型（普通文件、目录、链接等）
};

/**
 * @brief 文件系统操作类
 * 提供文件系统相关的基本操作，包括元数据管理、文件操作和权限检查
 */
class FileSystem {
public:
    FileSystem() = default;
    ~FileSystem() = default;

    /**
     * @brief 获取文件的元数据信息
     * @param path 文件路径
     * @return FileMetadata 包含文件元数据的结构体
     * @throw std::runtime_error 当文件不存在或无法访问时抛出异常
     */
    FileMetadata getFileMetadata(const fs::path& path);
    
    /**
     * @brief 设置文件的元数据信息
     * @param path 文件路径
     * @param metadata 要设置的元数据
     * @return bool 操作是否成功
     */
    bool setFileMetadata(const fs::path& path, const FileMetadata& metadata);
    
    /**
     * @brief 复制文件及其元数据
     * @param src 源文件路径
     * @param dst 目标文件路径
     * @return bool 操作是否成功
     */
    bool copyFile(const fs::path& src, const fs::path& dst);
    
    /**
     * @brief 创建符号链接
     * @param target 目标文件路径
     * @param link 链接文件路径
     * @return bool 操作是否成功
     */
    bool createSymlink(const fs::path& target, const fs::path& link);
    
    /**
     * @brief 创建硬链接
     * @param target 目标文件路径
     * @param link 链接文件路径
     * @return bool 操作是否成功
     */
    bool createHardlink(const fs::path& target, const fs::path& link);
    
    /**
     * @brief 创建命名管道
     * @param path 管道文件路径
     * @return bool 操作是否成功
     */
    bool createNamedPipe(const fs::path& path);
    
    /**
     * @brief 获取文件类型
     * @param path 文件路径
     * @return fs::file_type 文件类型枚举值
     * @throw std::runtime_error 当无法获取文件类型时抛出异常
     */
    fs::file_type getFileType(const fs::path& path);
    
    /**
     * @brief 检查文件是否可读
     * @param path 文件路径
     * @return bool 文件是否可读
     */
    bool isReadable(const fs::path& path);
    
    /**
     * @brief 检查文件是否可写
     * @param path 文件路径
     * @return bool 文件是否可写
     */
    bool isWritable(const fs::path& path);

private:
    /**
     * @brief 复制文件元数据的内部辅助函数
     * @param src 源文件路径
     * @param dst 目标文件路径
     * @return bool 操作是否成功
     */
    bool copyFileMetadata(const fs::path& src, const fs::path& dst);
};

} // namespace byte_enclave 