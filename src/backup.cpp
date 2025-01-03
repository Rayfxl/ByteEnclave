#include "backup.hpp"
#include <stack>
#include <algorithm>
#include <fstream>
#include <zlib.h>

namespace fs = std::filesystem;

namespace byte_enclave {

/**
 * @brief 构造函数，创建文件系统接口实例
 * 
 * 使用智能指针管理 FileSystem 实例的生命周期
 */
BackupManager::BackupManager() : fs_(std::make_unique<FileSystem>()) {}

/**
 * @brief 备份文件或目录
 * 
 * 该方法实现了完整的备份流程：
 * 1. 权限和路径检查
 * 2. 文件收集和过滤
 * 3. 文件复制和验证
 * 4. 元数据保存
 */
bool BackupManager::backup(const fs::path& source_path,
                         const fs::path& backup_path,
                         const BackupOptions& options) {
    try {
        // 检查源路径是否存在
        if (!fs::exists(source_path)) {
            return false;
        }
        
        // 检查源路径是否可读
        // 这是必要的，因为文件存在不代表当前用户有读权限
        if (!fs_->isReadable(source_path)) {
            return false;
        }
        
        // 检查目标路径是否可写
        // 如果目标���径存在，需要确保我们可以覆盖它
        if (fs::exists(backup_path)) {
            if (!fs_->isWritable(backup_path)) {
                return false;
            }
        }
        
        // 创建备份文件的父目录
        // 这是必要的，因为目标路径的父目录可能不存在
        auto backup_parent = backup_path.parent_path();
        if (!backup_parent.empty()) {
            if (!fs::exists(backup_parent)) {
                if (!fs::create_directories(backup_parent)) {
                    return false;
                }
            }
            // 检查父目录是否可写
            // 这是必要的，因为创建目录成功不代表我们有写权限
            if (!fs_->isWritable(backup_parent)) {
                return false;
            }
        }
        
        // 收集要备份的文件
        // 如果是目录，递归收集所有符合条件的文件
        // 如果是单个文件，直接添加到列表
        std::vector<fs::path> files_to_backup;
        if (fs::is_directory(source_path)) {
            collectFiles(source_path, files_to_backup, options);
        } else {
            files_to_backup.push_back(source_path);
        }
        
        // 如果没有文件需要备份，直接返回成功
        // 可能发生在：
        // 1. 空目录
        // 2. 所有文件都被过滤掉
        if (files_to_backup.empty()) {
            return true;
        }
        
        // 创建相对路径映射
        // 这确保了在还原时能保持原始的目录结构
        std::map<fs::path, fs::path> path_mapping;
        for (const auto& file : files_to_backup) {
            if (fs::is_directory(source_path)) {
                // 如果源是目录，保持相对路径结构
                path_mapping[file] = fs::relative(file, source_path);
            } else {
                // 如果源是文件，只使用文件名
                path_mapping[file] = file.filename();
            }
        }
        
        // 备份每个文件
        for (const auto& [src, rel_path] : path_mapping) {
            // 构造目标路径
            auto dst = backup_path;
            if (!fs::exists(backup_path) || fs::is_directory(backup_path)) {
                dst = backup_path / rel_path;
            }
            
            // 创建目标文件的父目录
            auto parent_path = dst.parent_path();
            if (!fs::exists(parent_path)) {
                if (!fs::create_directories(parent_path)) {
                    return false;
                }
            }
            
            // 检查目标目录是否可写
            if (!fs_->isWritable(parent_path)) {
                return false;
            }
            
            // 根据文件类型进行不同处理
            auto file_type = fs_->getFileType(src);
            
            switch (file_type) {
                case std::filesystem::file_type::regular: {
                    // 计算源文件的 CRC32
                    // 这是为了后续验证文件完整性
                    std::ifstream file(src, std::ios::binary);
                    if (!file) {
                        return false;
                    }
                    uLong src_crc = crc32(0L, Z_NULL, 0);
                    std::vector<uint8_t> buffer(1024);
                    while (true) {
                        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
                        size_t bytes_read = file.gcount();
                        if (bytes_read == 0) {
                            break;
                        }
                        src_crc = crc32(src_crc, buffer.data(), bytes_read);
                        if (file.eof()) {
                            break;
                        }
                        if (file.fail()) {
                            return false;
                        }
                    }
                    
                    // 复制文件
                    // 再次检查可读性是为了确保在计算 CRC32 后文件仍然可访问
                    if (!fs_->isReadable(src) || !fs_->copyFile(src, dst)) {
                        return false;
                    }
                    
                    // 计算备份文件的 CRC32
                    // 这是为了确保复制过程中没有发生错误
                    std::ifstream dst_file(dst, std::ios::binary);
                    if (!dst_file) {
                        return false;
                    }
                    uLong dst_crc = crc32(0L, Z_NULL, 0);
                    while (true) {
                        dst_file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
                        size_t bytes_read = dst_file.gcount();
                        if (bytes_read == 0) {
                            break;
                        }
                        dst_crc = crc32(dst_crc, buffer.data(), bytes_read);
                        if (dst_file.eof()) {
                            break;
                        }
                        if (dst_file.fail()) {
                            return false;
                        }
                    }
                    
                    // 验证 CRC32 是否一致
                    // 如果不一致，说明复制过程中文件被修改
                    if (src_crc != dst_crc) {
                        fs::remove(dst);  // 如果不一致，删除备份文件
                        return false;
                    }
                    
                    // 保存 CRC32 到文件
                    // 这用于后续的备份验证
                    auto crc_file = dst.string() + ".crc";
                    std::ofstream crc_out(crc_file);
                    if (!crc_out) {
                        fs::remove(dst);  // 如果无法创建 CRC 文件，删除备份文件
                        return false;
                    }
                    crc_out << src_crc;
                    if (crc_out.fail()) {
                        fs::remove(dst);  // 如果写入 CRC 失败，删除备份文件
                        fs::remove(crc_file);
                        return false;
                    }
                    crc_out.close();
                    break;
                }
                    
                case std::filesystem::file_type::symlink:
                    // 处理符号链接
                    if (options.include_symlinks) {
                        // 保持原始链接目标路径
                        // 不解析链接，保持原始路径可能是相对路径）
                        auto target = fs::read_symlink(src);
                        if (!fs_->createSymlink(target, dst)) {
                            return false;
                        }
                    }
                    break;
                    
                default:
                    // 忽略其他类型的文件
                    // 这包括：目录、设备文件、管道等
                    continue;
            }
        }
        
        return true;
    } catch (const std::exception&) {
        // 捕获所有异常，确保函数总是返回 bool
        return false;
    }
}

/**
 * @brief 还原备份
 * 
 * 该方法实现了完整的还原流程：
 * 1. 权限和路径检查
 * 2. 目标路径准备
 * 3. 文件还原
 * 4. 元数据恢复
 */
bool BackupManager::restore(const fs::path& backup_path,
                          const fs::path& target_path,
                          const BackupOptions& options) {
    try {
        // 检查备份是否存在
        if (!fs::exists(backup_path)) {
            return false;
        }
        
        // 检查备份路径是否可读
        // 这是必要的，因为文件存在不代表当前用户有读权限
        if (!fs_->isReadable(backup_path)) {
            return false;
        }
        
        // 创建目标目录的父目录
        // 这是必要的，因为目标路径的父目录可能不存在
        auto target_parent = target_path.parent_path();
        if (!target_parent.empty()) {
            if (!fs::exists(target_parent)) {
                if (!fs::create_directories(target_parent)) {
                    return false;
                }
            }
            // 检查父目录是否可写
            if (!fs_->isWritable(target_parent)) {
                return false;
            }
        }
        
        // 如果目标路径存在，先检查是否可写清理
        // 这是必要的，因为我们需要覆盖或清理现有内容
        if (fs::exists(target_path)) {
            if (!fs_->isWritable(target_path)) {
                return false;
            }
            fs::remove_all(target_path);
        }
        
        // 如果备份文件是符号链接，直接还原
        if (fs::is_symlink(backup_path)) {
            if (!options.include_symlinks) {
                return false;
            }
            // 直接使用原始链接目标，不做任何路径调整
            // 这保持了原始链接的相对路径特性
            auto target = fs::read_symlink(backup_path);
            fs::create_symlink(target, target_path);
            return true;
        }
        
        // 如果备份文件是普通文件，直接复制
        // 这处理单文件备份的情况
        if (fs::is_regular_file(backup_path)) {
            return fs_->copyFile(backup_path, target_path);
        }
        
        // 创建目标目录
        // 这是必要的，因为后续需要在此目录下还原文件
        if (!fs::create_directories(target_path)) {
            return false;
        }
        
        // 检查目标目录是否可写
        if (!fs_->isWritable(target_path)) {
            return false;
        }
        
        // 收集要还原的文件
        // 这会递归遍历备份目录，收集所有需要还原的文件
        std::vector<fs::path> files_to_restore;
        collectFiles(backup_path, files_to_restore, options);
        
        // 如果没有文件需要还原，返回成功
        // 这可能是因为：
        // 1. 备份目录为空
        // 2. 所有文件都被过滤掉
        if (files_to_restore.empty()) {
            return true;
        }
        
        // 创建相对路径映射
        // 这确保了还原时保持原始的目录结构
        std::map<fs::path, fs::path> path_mapping;
        for (const auto& file : files_to_restore) {
            path_mapping[file] = fs::relative(file, backup_path);
        }
        
        // 还原文件
        for (const auto& [src, rel_path] : path_mapping) {
            auto dst = target_path / rel_path;
            
            // 创建目标文件的父目录
            // 这是必要的，因为文件的父目录可能还不存在
            auto parent_path = dst.parent_path();
            if (!fs::exists(parent_path)) {
                if (!fs::create_directories(parent_path)) {
                    return false;
                }
            }
            
            // 处理符号链接
            if (fs::is_symlink(src)) {
                if (!options.include_symlinks) {
                    continue;
                }
                // 直接使用原始链接目标，不做任何路径调整
                // 这保持了原始链接的相对路径特性
                auto target = fs::read_symlink(src);
                fs::create_symlink(target, dst);
                continue;
            }
            
            // 复制文件
            // 这会同时复制文件内容和元数据
            if (!fs_->copyFile(src, dst)) {
                return false;
            }
        }
        
        return true;
    } catch (const std::exception&) {
        // 捕获所有异常，确保函数总是返回 bool
        return false;
    }
}

std::vector<fs::path> BackupManager::listBackupContents(
    const fs::path& backup_path) {
    std::vector<fs::path> contents;
    
    try {
        if (!fs::exists(backup_path)) {
            return contents;
        }
        
        for (const auto& entry : fs::recursive_directory_iterator(backup_path)) {
            if (entry.is_regular_file() || entry.is_symlink()) {
                contents.push_back(fs::relative(entry.path(), backup_path));
            }
        }
    } catch (const std::exception&) {
        contents.clear();
    }
    
    return contents;
}

/**
 * @brief 验证备份的完整性
 * 
 * 该方法会验证备份的完整性：
 * 1. 对于目录备份：
 *    - 验证所有文件的完整性
 *    - 跳过 .crc 文件（这些是校验和文件）
 * 2. 对于单文件备份：
 *    - 直接验证文件的完整性
 * 
 * @param backup_path 备份的路径
 * @return true 备份完整
 * @return false 备份已损坏或不完整
 */
bool BackupManager::verifyBackup(const fs::path& backup_path) {
    try {
        // 检查备份是否存在
        if (!fs::exists(backup_path)) {
            return false;
        }
        
        // 如果是目录，验证所有文件
        if (fs::is_directory(backup_path)) {
            // 收集所有需要验证的文件
            std::vector<fs::path> files;
            collectFiles(backup_path, files, BackupOptions{});
            
            // 如果没有文件需要验证，返回 true
            // 这可能是因为：
            // 1. 目录为空
            // 2. 所有文件都被过滤掉
            if (files.empty()) {
                return true;
            }
            
            // 验证每个文件
            for (const auto& file : files) {
                // 跳过 .crc 文件
                // 这些是校验和文件，不需要验证
                if (file.extension() == ".crc") {
                    continue;
                }
                if (!verifyFile(file)) {
                    return false;
                }
            }
            return true;
        }
        
        // 如果是单个文件，直接验证
        return verifyFile(backup_path);
    } catch (const std::exception&) {
        // 捕获所有异常，确保函数总是返回 bool
        return false;
    }
}

/**
 * @brief 验证单个文件的完整性
 * 
 * 该方法会根据文件类型进行不同的验证：
 * 1. 对于普通文件：
 *    - 检查 .crc 文件是否存在
 *    - 读取存储的 CRC32
 *    - 计算当前文件的 CRC32
 *    - 比较两个 CRC32 值
 * 2. 对于符号链接：
 *    - 检查链接目标是否有���
 * 3. 对于其他类型：
 *    - 返回 true（不进行验证）
 * 
 * @param path 文件路径
 * @return true 文件完整
 * @return false 文件已损坏或不完整
 */
bool BackupManager::verifyFile(const fs::path& path) {
    try {
        // 检查文件是否存在且可读
        if (!fs::exists(path) || !fs_->isReadable(path)) {
            return false;
        }
        
        // 如果是符号链接，验证链接目标是否有效
        if (fs::is_symlink(path)) {
            auto target = fs::read_symlink(path);
            return !target.empty();  // 只要能读取到目标就认为是有效的
        }
        
        // 如果是普通文件，计算并验证 CRC32
        if (fs::is_regular_file(path)) {
            // 读取保存的 CRC32
            auto crc_file = path.string() + ".crc";
            if (!fs::exists(crc_file)) {
                return false;  // 如果没有 CRC 文件，说明文件可能被破坏或不是通过备份系统创建的
            }
            std::ifstream crc_in(crc_file);
            if (!crc_in) {
                return false;  // 如果无法打开 CRC 文件，认为验证失败
            }
            uLong expected_crc;
            if (!(crc_in >> expected_crc)) {
                return false;  // 如果无法读取 CRC 值，认为验证���败
            }
            
            // 计算当前文件的 CRC32
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                return false;
            }
            
            uLong crc = crc32(0L, Z_NULL, 0);
            std::vector<uint8_t> buffer(1024);
            while (true) {
                file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
                size_t bytes_read = file.gcount();
                if (bytes_read == 0) {
                    break;
                }
                crc = crc32(crc, buffer.data(), bytes_read);
                if (file.eof()) {
                    break;
                }
                if (file.fail()) {
                    return false;  // 如果读取失败，认为验证失败
                }
            }
            
            // 比较计算的 CRC32 和存储的 CRC32
            return crc == expected_crc;
        }
        
        return true;  // 其他类型的文件默认为有效
    } catch (const std::exception&) {
        // 捕获所有异常，确保函数总是返回 bool
        return false;
    }
}

/**
 * @brief 计算文件的预期校验和
 * 
 * 该方法用于计算文件的 CRC32 校验和：
 * 1. 以二进制模式打开文件
 * 2. 分块读取文件内容
 * 3. 累积计算 CRC32
 * 
 * @param path 文件路径
 * @return uint32_t 文件的 CRC32 校验和
 */
uint32_t BackupManager::calculateExpectedChecksum(const fs::path& path) {
    // 计算文件的 CRC32
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return 0;  // 如果无法打开文件，返回 0
    }
    
    uLong crc = crc32(0L, Z_NULL, 0);  // 初始化 CRC32
    std::vector<uint8_t> buffer(1024);  // 使用 1KB 的缓冲区
    while (file.read(reinterpret_cast<char*>(buffer.data()), buffer.size())) {
        size_t bytes_read = file.gcount();
        crc = crc32(crc, buffer.data(), bytes_read);
    }
    
    return static_cast<uint32_t>(crc);
}

/**
 * @brief 收集目录中的文件
 * 
 * 该方法会递归遍历目录，收集符合条件的文件：
 * 1. 根据选项过滤文件：
 *    - 是否包含隐藏文件
 *    - 是否包含符号链接
 *    - 是否匹配排除模式
 * 2. 只收集普通文件和符号链接
 * 3. 忽略其他类型的文件（目录、设备等）
 * 
 * @param dir 要遍历的目录
 * @param files 收集到的文件列表
 * @param options 控制收集行为的选项
 */
void BackupManager::collectFiles(const fs::path& dir,
                               std::vector<fs::path>& files,
                               const BackupOptions& options) {
    try {
        // 递归遍历目录
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            const auto& path = entry.path();
            
            // 检查是否是隐藏文件
            // 在 Unix 系统中，以 . 开头的文件被视为隐藏文件
            if (!options.include_hidden_files && path.filename().string()[0] == '.') {
                continue;
            }
            
            // 检查是否匹配排除模式
            // 如果文件路径包含任何排除模式，就跳过该文件
            bool should_exclude = false;
            for (const auto& pattern : options.exclude_patterns) {
                if (path.string().find(pattern) != std::string::npos) {
                    should_exclude = true;
                    break;
                }
            }
            if (should_exclude) {
                continue;
            }
            
            // 根据文件类型和选项决定是否包含
            if (entry.is_regular_file()) {
                // 普通文件总是被包含
                files.push_back(path);
            } else if (entry.is_symlink() && options.include_symlinks) {
                // 符号链接只在选项允许时被包含
                files.push_back(path);
            }
        }
    } catch (const std::exception&) {
        // 如果遍历过程中发生错误，清空文件列表
        // 这确保了要么收集所有文件，要么一个都不收集
        files.clear();
    }
}

} // namespace byte_enclave } // namespace byte_enclave 

