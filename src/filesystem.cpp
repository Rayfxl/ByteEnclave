#include "filesystem.hpp"
#include <fstream>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace fs = std::filesystem;

namespace byte_enclave {

FileMetadata FileSystem::getFileMetadata(const fs::path& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        throw std::runtime_error("File does not exist: " + path.string());
    }

    struct stat st;
    if (lstat(path.c_str(), &st) != 0) {
        throw std::runtime_error("Failed to get file metadata: " + path.string());
    }

    FileMetadata metadata;
    
    // 获取所有者信息
    struct passwd* pw = getpwuid(st.st_uid);
    if (pw) {
        metadata.owner = pw->pw_name;
    }
    
    // 获取组信息
    struct group* gr = getgrgid(st.st_gid);
    if (gr) {
        metadata.group = gr->gr_name;
    }
    
    // 获取权限和时间戳（使用filesystem）
    metadata.permissions = fs::status(path).permissions();
    metadata.access_time = fs::last_write_time(path);
    metadata.modify_time = fs::last_write_time(path);
    metadata.create_time = fs::last_write_time(path); // C++17不直接支持创建时间
    metadata.file_type = fs::symlink_status(path).type();
    
    return metadata;
}

bool FileSystem::setFileMetadata(const fs::path& path, const FileMetadata& metadata) {
    try {
        // 设置权限（使用filesystem）
        fs::permissions(path, metadata.permissions,
                       fs::perm_options::replace);
        
        // 设置所有者和组
        if (!metadata.owner.empty() && !metadata.group.empty()) {
            struct passwd* pw = getpwnam(metadata.owner.c_str());
            struct group* gr = getgrnam(metadata.group.c_str());
            if (pw && gr) {
                if (chown(path.c_str(), pw->pw_uid, gr->gr_gid) != 0) {
                    return false;
                }
            }
        }
        
        // 设置时间戳（使用filesystem）
        fs::last_write_time(path, metadata.modify_time);
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::copyFile(const fs::path& src, const fs::path& dst) {
    try {
        // 获取源文件的元数据
        auto metadata = getFileMetadata(src);
        
        // 根据文件类型执行不同的复制操作
        switch (metadata.file_type) {
            case fs::file_type::regular:
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
                break;
                
            case fs::file_type::symlink:
                if (fs::exists(dst)) {
                    fs::remove(dst);
                }
                fs::create_symlink(fs::read_symlink(src), dst);
                break;
                
            default:
                return false;
        }
        
        // 复制元数据
        return copyFileMetadata(src, dst);
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::createSymlink(const fs::path& target, const fs::path& link) {
    try {
        if (fs::exists(link)) {
            fs::remove(link);
        }
        fs::create_symlink(target, link);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::createHardlink(const fs::path& target, const fs::path& link) {
    try {
        if (!fs::exists(target)) {
            return false;
        }
        
        if (fs::exists(link)) {
            fs::remove(link);
        }
        
        fs::create_hard_link(target, link);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::createNamedPipe(const fs::path& path) {
    try {
        if (fs::exists(path)) {
            fs::remove(path);
        }
        
        // 名管道仍需使用系统调用，因为filesystem没有直接支持
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
    auto status = fs::symlink_status(path, ec);
    if (ec) {
        throw std::runtime_error("Failed to get file type: " + path.string());
    }
    return status.type();
}

bool FileSystem::copyFileMetadata(const fs::path& src, const fs::path& dst) {
    try {
        auto metadata = getFileMetadata(src);
        return setFileMetadata(dst, metadata);
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::isReadable(const fs::path& path) {
    try {
        // 检查文件是否存在
        if (!fs::exists(path)) {
            return false;
        }
        
        // 尝试打开文件进行读取
        std::ifstream file(path);
        return file.good();
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::isWritable(const fs::path& path) {
    try {
        // 如果路径不存在，检查父目录是否可写
        if (!fs::exists(path)) {
            auto parent = path.parent_path();
            if (parent.empty()) {
                parent = ".";
            }
            return access(parent.c_str(), W_OK) == 0;
        }
        
        // 检查文件是否可写
        return access(path.c_str(), W_OK) == 0;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace byte_enclave
