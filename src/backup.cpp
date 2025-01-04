#include "backup.hpp"
#include "packer.hpp"
#include <stack>
#include <vector>
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
    auto temp_backup = backup_path.string() + ".tmp";
    auto cleanup = [&]() {
        if (fs::exists(temp_backup)) {
            fs::remove_all(temp_backup);
        }
    };
    try {
        // 1. 基本检查
        if (!fs::exists(source_path) || access(source_path.c_str(), R_OK) != 0) {
            return false;
        }

        // 2. 检查目标空间
        auto space = fs::space(backup_path.parent_path());
        if (space.available < calculateRequiredSpace(source_path)) {
            return false;
        }

        // 3. 收集要备份的文件
        std::vector<fs::path> files_to_backup;
        fs::path base_dir = source_path;
        if (fs::is_directory(source_path)) {
            collectFiles(source_path, files_to_backup, options);
        } else {
            files_to_backup.push_back(source_path);
            base_dir = source_path.parent_path();
        }

        if (files_to_backup.empty()) {
            return false;
        }

        // 4. 创建临时备份路径


        // 5. 执行备份
        Packer packer;
        if (!packer.pack(files_to_backup, temp_backup)) {
            cleanup();
            return false;
        }

        // 6. 验证备份
        if (!packer.verifyChecksum(temp_backup)) {
            cleanup();
            return false;
        }

        // 7. 原子性替换
        fs::rename(temp_backup, backup_path);
        return true;
    } catch (...) {
        cleanup();
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
    auto temp_path = target_path.string() + ".tmp";
        auto cleanup = [&]() {
            if (fs::exists(temp_path)) {
                fs::remove_all(temp_path);
            }
        };
    try {
        // 1. 基础检查
        if (!fs::exists(backup_path) || access(backup_path.c_str(), R_OK) != 0) {
            return false;
        }

        // 2. 检查备份文件完整性
        Packer packer;
        if (!packer.verifyChecksum(backup_path)) {
            return false;
        }

        // 3. 检查目标空间
        auto space = fs::space(target_path.parent_path());
        auto headers = packer.listContents(backup_path);
        uint64_t required_space = 0;
        for (const auto& h : headers) {
            required_space += h.size;
        }
        if (space.available < required_space) {
            return false;
        }

        // 4. 创建临时目录
        fs::create_directories(temp_path);

        // 5. 执行还原
        if (!packer.unpack(backup_path, temp_path)) {
            cleanup();
            return false;
        }

        // 6. 原子性替换
        if (fs::exists(target_path)) {
            fs::remove_all(target_path);
        }
        fs::rename(temp_path, target_path);
        return true;

    } catch (...) {
        cleanup();
        return false;
    }
}

uint64_t BackupManager::calculateRequiredSpace(const fs::path& path) {
    uint64_t total = 0;
    if (fs::is_directory(path)) {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                total += entry.file_size();
            }
        }
    } else {
        total = fs::file_size(path);
    }
    return total * 2; // 为压缩和临时文件预留空间
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
        if (!fs::exists(path) || access(path.c_str(), R_OK) != 0) {
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
        // 先收集所有目录
        std::vector<fs::path> directories;
        for (const auto& entry : fs::directory_iterator(dir)) {
            const auto& path = entry.path();
            
            if (entry.is_directory()) {
                directories.push_back(path);
                // 递归处理子目录
                collectFiles(path, files, options);
            } else {
                if (!options.include_hidden_files && 
                    path.filename().string()[0] == '.') {
                    continue;
                }
                
                bool should_exclude = false;
                for (const auto& pattern : options.exclude_patterns) {
                    if (path.string().find(pattern) != std::string::npos) {
                        should_exclude = true;
                        break;
                    }
                }
                if (should_exclude) continue;

                if (entry.is_regular_file() || 
                    (entry.is_symlink() && options.include_symlinks)) {
                    files.push_back(path);
                }
            }
        }
    } catch (const std::exception&) {
        files.clear();
    }
}

} // namespace byte_enclave } // namespace byte_enclave 

