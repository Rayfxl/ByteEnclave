#include "filesystem.hpp"
#include <fstream>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <pwd.h>      // 用于用户信息查询
#include <grp.h>      // 用于组信息查询
#include <unistd.h>   // 用于文件访问检查
#include <sys/stat.h> // 用于文件状态
#include <fcntl.h>    // 用于文件控制选项

namespace fs = std::filesystem;

namespace byte_enclave {

FileMetadata FileSystem::getFileMetadata(const fs::path& path) {
    std::error_code ec;
    // 检查文件是否存在
    if (!fs::exists(path, ec)) {
        throw std::runtime_error("File does not exist: " + path.string());
    }

    // 获取文件状态信息
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) {
        throw std::runtime_error("Failed to get file metadata: " + path.string());
    }

    FileMetadata metadata;
    
    // 通过UID查询用户名
    struct passwd* pw = getpwuid(st.st_uid);
    if (pw) {
        metadata.owner = pw->pw_name;
    }
    
    // 通过GID查询组名
    struct group* gr = getgrgid(st.st_gid);
    if (gr) {
        metadata.group = gr->gr_name;
    }
    
    // 使用std::filesystem获取文件属性
    metadata.permissions = fs::status(path).permissions();
    metadata.access_time = fs::last_write_time(path);
    metadata.modify_time = fs::last_write_time(path);
    metadata.create_time = fs::last_write_time(path); // 注：C++17不直接支持获取创建时间
    metadata.file_type = fs::symlink_status(path).type();
    
    return metadata;
}

bool FileSystem::setFileMetadata(const fs::path& path, const FileMetadata& metadata) {
    try {
        // 设置文件权限
        fs::permissions(path, metadata.permissions,
                       fs::perm_options::replace);
        
        // 设置文件所有者和组
        if (!metadata.owner.empty() && !metadata.group.empty()) {
            struct passwd* pw = getpwnam(metadata.owner.c_str());
            struct group* gr = getgrnam(metadata.group.c_str());
            if (pw && gr) {
                if (chown(path.c_str(), pw->pw_uid, gr->gr_gid) != 0) {
                    return false;
                }
            }
        }
        
        // 设置文件修改时间
        fs::last_write_time(path, metadata.modify_time);
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::copyFile(const fs::path& src, const fs::path& dst) {
    try {
        // 首先获取源文件的元数据
        auto metadata = getFileMetadata(src);
        
        // 根据文件类型执行不同的复制操作
        switch (metadata.file_type) {
            case fs::file_type::regular:
                // 复制普通文件
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
                break;
                
            case fs::file_type::symlink:
                // 复制符号链接
                if (fs::exists(dst)) {
                    fs::remove(dst);
                }
                fs::create_symlink(fs::read_symlink(src), dst);
                break;
                
            default:
                return false;
        }
        
        // 复制文件的元数据
        return copyFileMetadata(src, dst);
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::createSymlink(const fs::path& target, const fs::path& link) {
    try {
        // 如果链接已存在，先删除
        if (fs::exists(link)) {
            fs::remove(link);
        }
        // 创建新的符号链接
        fs::create_symlink(target, link);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::createHardlink(const fs::path& target, const fs::path& link) {
    try {
        // 检查目标文件是否存在
        if (!fs::exists(target)) {
            return false;
        }
        
        // 如果链接已存在，先删除
        if (fs::exists(link)) {
            fs::remove(link);
        }
        
        // 创建硬链接
        fs::create_hard_link(target, link);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::createNamedPipe(const fs::path& path) {
    try {
        // 如果管道文件已存在，先删除
        if (fs::exists(path)) {
            fs::remove(path);
        }
        
        // 创建命名管道，权限设置为666
        if (mkfifo(path.c_str(), 0666) != 0) {
            return false;
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

fs::file_type FileSystem::getFileType(const fs::path& path) {
    std::error_code ec;
    // 获取文件状态（包括符号链接）
    auto status = fs::symlink_status(path, ec);
    if (ec) {
        throw std::runtime_error("Failed to get file type: " + path.string());
    }
    return status.type();
}

bool FileSystem::copyFileMetadata(const fs::path& src, const fs::path& dst) {
    try {
        // 获取源文件的元数据并应用到目标文件
        auto metadata = getFileMetadata(src);
        return setFileMetadata(dst, metadata);
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::isReadable(const fs::path& path) {
    try {
        // 首先检查文件是否存在
        if (!fs::exists(path)) {
            return false;
        }
        
        // 尝试打开文件检查是否可读
        std::ifstream file(path);
        return file.good();
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::isWritable(const fs::path& path) {
    try {
        // 如果文件不存在，检查父目录是否可写
        if (!fs::exists(path)) {
            auto parent = path.parent_path();
            if (parent.empty()) {
                parent = ".";
            }
            return access(parent.c_str(), W_OK) == 0;
        }
        
        // 使用access系统调用检查文件是否可写
        return access(path.c_str(), W_OK) == 0;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace byte_enclave
