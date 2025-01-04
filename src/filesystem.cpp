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
    if (!fs::exists(path, ec)) {
        throw std::runtime_error("File does not exist: " + path.string());
    }

    // 检查文件权限
    if (access(path.c_str(), R_OK) != 0) {
        throw std::runtime_error(
            std::string("Permission denied: ") + std::strerror(errno)
        );
    }

    FileMetadata metadata;

    // 使用 stat 获取完整信息
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) {
        throw std::runtime_error(std::string("Failed to get file metadata: ") + std::strerror(errno));
    }

    // 获取所有者信息
    struct passwd* pw = getpwuid(st.st_uid);
    if (!pw) {
        throw std::runtime_error("Failed to get owner info: " + std::string(std::strerror(errno)));
    }
    metadata.owner = pw->pw_name;

    // 获取组信息
    struct group* gr = getgrgid(st.st_gid);
    if (!gr) {
        throw std::runtime_error("Failed to get group info: " + std::string(std::strerror(errno)));
    }
    metadata.group = gr->gr_name;

    // 时间戳转换
    auto to_file_time = [](time_t t) -> fs::file_time_type {
        auto sys_time = std::chrono::system_clock::from_time_t(t);
        auto file_time = fs::file_time_type::clock::now() + 
            (sys_time - std::chrono::system_clock::now());
        return file_time;
    };

    metadata.access_time = to_file_time(st.st_atime);
    metadata.modify_time = to_file_time(st.st_mtime);
    metadata.create_time = to_file_time(st.st_ctime);
    // // 正确设置时间戳
    // metadata.access_time = fs::file_time_type::clock::from_time_t(st.st_atime);
    // metadata.modify_time = fs::file_time_type::clock::from_time_t(st.st_mtime);
    // metadata.create_time = fs::file_time_type::clock::from_time_t(st.st_ctime);

    // 获取文件类型和权限
    metadata.file_type = fs::symlink_status(path, ec).type();
    if (ec) {
        throw std::runtime_error("Failed to get file type: " + ec.message());
    }

    metadata.permissions = fs::status(path, ec).permissions();
    if (ec) {
        throw std::runtime_error("Failed to get file permissions: " + ec.message());
    }

    return metadata;
}

bool FileSystem::setFileMetadata(const fs::path& path, const FileMetadata& metadata) {
    // 检查文件是否存在
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return false;
    }

    // 检查当前进程是否有权限修改
    if (access(path.c_str(), W_OK) != 0) {
        return false;
    }

    try {
        // 设置文件权限
        fs::permissions(path, metadata.permissions,
                       fs::perm_options::replace);
        
        // 设置文件所有者和组
        if (!metadata.owner.empty() && !metadata.group.empty()) {
            struct passwd* pw = getpwnam(metadata.owner.c_str());
            struct group* gr = getgrnam(metadata.group.c_str());
            
            if (!pw || !gr) {
                return false;
            }
            
            if (chown(path.c_str(), pw->pw_uid, gr->gr_gid) != 0) {
                return false;
            }
        }
        
        // 设置文件修改时间
        fs::last_write_time(path, metadata.modify_time);
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::copyFileMetadata(const fs::path& src, const fs::path& dst) {
    try {
        // 1. 检查文件存在性和权限
        std::error_code ec;
        if (!fs::exists(src, ec) || access(src.c_str(), R_OK) != 0) {
            return false;
        }
        if (!fs::exists(dst, ec) || access(dst.c_str(), W_OK) != 0) {
            return false;
        }

        // 2. 获取源文件元数据
        auto metadata = getFileMetadata(src);

        // 3. 应用到目标文件
        return setFileMetadata(dst, metadata);
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::copyFile(const fs::path& src, const fs::path& dst) {
    try {
        // 1. 权限检查
        std::error_code ec;
        if (!fs::exists(src, ec) || access(src.c_str(), R_OK) != 0) {
            return false;
        }

        if (fs::exists(dst.parent_path()) && access(dst.parent_path().c_str(), W_OK) != 0) {
            return false;
        }

        // 2. 获取元数据
        auto metadata = getFileMetadata(src);
        struct stat st;
        if (lstat(src.c_str(), &st) != 0) {
            return false;
        }
        
        // 3. 根据类型复制
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

            case fs::file_type::directory:
                fs::create_directories(dst);
                break;

            case fs::file_type::block:
            case fs::file_type::character:
                if (mknod(dst.c_str(), st.st_mode, st.st_rdev) != 0) {
                    return false;
                }
                break;

            case fs::file_type::fifo:
                if (mkfifo(dst.c_str(), st.st_mode) != 0) {
                    return false;
                }
                break;
                
            default:
                return false;
        }
        
        // 4. 复制元数据
        return copyFileMetadata(src, dst);
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::createSymlink(const fs::path& target, const fs::path& link) {
    try {
        // // 1. 不检查目标文件是否存在
        std::error_code ec;
        // if (!fs::exists(target, ec)) {
        //     return false;
        // }

        // 2. 检查目标目录写权限
        auto link_parent = link.parent_path();
        if (fs::exists(link_parent) && access(link_parent.c_str(), W_OK) != 0) {
            return false;
        }

        // 3. 如果链接已存在，先删除
        if (fs::exists(link, ec)) {
            fs::remove(link, ec);
            if (ec) {
                return false;
            }
        }

        // 4. 创建新的符号链接
        fs::create_symlink(target, link, ec);
        return !ec;
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::createHardlink(const fs::path& target, const fs::path& link) {
    try {
        // 1. 检查目标文件
        std::error_code ec;
        if (!fs::exists(target, ec) || 
            !fs::is_regular_file(target, ec) ||
            fs::is_symlink(target, ec)) {  // 添加符号链接检查
            return false;
        }

        // 2. 检查目标目录写权限
        auto link_parent = link.parent_path();
        if (fs::exists(link_parent) && access(link_parent.c_str(), W_OK) != 0) {
            return false;
        }

        // 3. 检查是否在同一文件系统
        struct stat st1, st2;
        if (stat(target.c_str(), &st1) != 0 || 
            stat(link_parent.c_str(), &st2) != 0) {
            return false;
        }
        if (st1.st_dev != st2.st_dev) {
            return false;  // 不在同一文件系统
        }

        // 4. 如果链接已存在，先删除
        if (fs::exists(link, ec)) {
            fs::remove(link, ec);
            if (ec) {
                return false;
            }
        }

        // 5. 创建硬链接
        fs::create_hard_link(target, link, ec);
        return !ec;
    } catch (const std::exception&) {
        return false;
    }
}

bool FileSystem::createNamedPipe(const fs::path& path) {
    try {
        // 1. 检查目标目录写权限
        auto parent_path = path.parent_path();
        if (fs::exists(parent_path) && access(parent_path.c_str(), W_OK) != 0) {
            return false;
        }

        // 2. 如果管道文件已存在，先删除
        std::error_code ec;
        if (fs::exists(path, ec)) {
            fs::remove(path, ec);
            if (ec) {
                return false;
            }
        }
        
        // 3. 创建命名管道
        if (mkfifo(path.c_str(), 0666) != 0) {
            return false;
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace byte_enclave
