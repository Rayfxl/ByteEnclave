#include "backup.hpp"
#include <stack>
#include <vector>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace byte_enclave {

BackupManager::BackupManager() 
    : fs_(std::make_unique<FileSystem>()),
      packer_(std::make_unique<Packer>()),
      compressor_(std::make_unique<Compressor>()),
      encryptor_(std::make_unique<Encryptor>()) {}

bool BackupManager::backup(const fs::path& source_path,
                         const fs::path& backup_path,
                         const BackupOptions& options) {
    try {
        // 1. 基本检查
        std::cout << "[DEBUG] 检查源路径: " << source_path << std::endl;
        if (!fs::exists(source_path)) {
            std::cout << "[DEBUG] 源路径不存在" << std::endl;
            return false;
        }

        // 2. 构造备份文件路径
        fs::path actual_backup_path = backup_path;
        if (fs::is_directory(backup_path)) {
            // 如果目标是目录，在目录下创建备份文件
            std::string backup_name = source_path.filename().string() + ".backup";
            actual_backup_path = backup_path / backup_name;
            std::cout << "[DEBUG] 目标是目录，实际备份文件路径: " << actual_backup_path << std::endl;
        }

        // 3. 检查目标空间
        std::cout << "[DEBUG] 检查目标空间" << std::endl;
        auto space = fs::space(actual_backup_path.parent_path());
        auto required = calculateRequiredSpace(source_path);
        std::cout << "[DEBUG] 可用空间: " << space.available << " 字节" << std::endl;
        std::cout << "[DEBUG] 需要空间: " << required << " 字节" << std::endl;
        if (space.available < required) {
            std::cout << "[DEBUG] 空间不足" << std::endl;
            return false;
        }

        // 4. 收集要备份的文件
        std::cout << "[DEBUG] 开始收集文件" << std::endl;
        std::vector<fs::path> files_to_backup;
        if (fs::is_directory(source_path)) {
            collectFiles(source_path, files_to_backup, options);
        } else {
            files_to_backup.push_back(source_path);
        }

        if (files_to_backup.empty()) {
            std::cout << "[DEBUG] 没有找到要备份的文件" << std::endl;
            return false;
        }
        std::cout << "[DEBUG] 找到 " << files_to_backup.size() << " 个文件:" << std::endl;
        for (const auto& f : files_to_backup) {
            std::cout << "  - " << f << std::endl;
        }

        // 5. 创建临时文件
        std::cout << "[DEBUG] 创建临时文件" << std::endl;
        auto temp_file = actual_backup_path.string() + ".tmp";
        auto temp_gz = actual_backup_path.string() + ".tmp.gz";
        
        auto cleanup = [&]() {
            std::cout << "[DEBUG] 清理临时文件" << std::endl;
            if (fs::exists(temp_file)) {
                std::cout << "[DEBUG] 删除临时文件: " << temp_file << std::endl;
                fs::remove(temp_file);
            }
            if (fs::exists(temp_gz)) {
                std::cout << "[DEBUG] 删除临时文件: " << temp_gz << std::endl;
                fs::remove(temp_gz);
            }
        };

        try {
            // 先清理可能存在的旧临时文件
            cleanup();

            // 6. 打包文件
            std::cout << "[DEBUG] 开始打包文件" << std::endl;
            if (!packer_->pack(files_to_backup, temp_file)) {
                std::cout << "[DEBUG] 打包失败" << std::endl;
                cleanup();
                return false;
            }
            std::cout << "[DEBUG] 打包完成" << std::endl;

            // 7. 压缩文件
            std::cout << "[DEBUG] 开始压缩文件" << std::endl;
            if (!compressor_->compress(temp_file, temp_gz)) {
                std::cout << "[DEBUG] 压缩失败" << std::endl;
                cleanup();
                return false;
            }
            std::cout << "[DEBUG] 压缩完成" << std::endl;

            // 8. 加密文件（如果需要）
            if (!options.password.empty()) {
                std::cout << "[DEBUG] 开始加密文件" << std::endl;
                if (!encryptor_->encrypt(temp_gz, actual_backup_path, options.password)) {
                    std::cout << "[DEBUG] 加密失败" << std::endl;
                    cleanup();
                    if (fs::exists(actual_backup_path)) {
                        fs::remove(actual_backup_path);
                    }
                    return false;
                }
                std::cout << "[DEBUG] 加密完成" << std::endl;
            } else {
                // 如果不需要加密，直接移动压缩文件到最终位置
                fs::rename(temp_gz, actual_backup_path);
            }

            cleanup();
            std::cout << "[DEBUG] 备份完成" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cout << "[DEBUG] 发生异常: " << e.what() << std::endl;
            cleanup();
            if (fs::exists(actual_backup_path)) {
                fs::remove(actual_backup_path);
            }
            return false;
        }
    } catch (const std::exception& e) {
        std::cout << "[DEBUG] 外层异常: " << e.what() << std::endl;
        return false;
    }
}

bool BackupManager::restore(const fs::path& backup_path,
                          const fs::path& target_path,
                          const BackupOptions& options) {
    try {
        // 1. 基础检查
        if (!fs::exists(backup_path)) {
            return false;
        }

        // 2. 创建临时文件
        auto temp_decrypted = target_path.string() + ".dec.tmp";
        auto temp_decompressed = target_path.string() + ".pack.tmp";
        auto temp_dir = target_path.string() + ".dir.tmp";
        
        auto cleanup = [&]() {
            fs::remove(temp_decrypted);
            fs::remove(temp_decompressed);
            fs::remove_all(temp_dir);
        };

        try {
            // 3. 解密文件
            if (!options.password.empty()) {
                if (!encryptor_->decrypt(backup_path, temp_decrypted, options.password)) {
                    cleanup();
                    return false;
                }
            } else {
                // 如果没有密码，直接复制文件
                fs::copy_file(backup_path, temp_decrypted);
            }

            // 4. 解压文件
            if (!compressor_->decompress(temp_decrypted, temp_decompressed)) {
                cleanup();
                return false;
            }

            // 5. 创建临时目录
            fs::create_directories(temp_dir);

            // 6. 解包文件
            if (!packer_->unpack(temp_decompressed, temp_dir)) {
                cleanup();
                return false;
            }

            // 7. 原子性替换
            if (fs::exists(target_path)) {
                fs::remove_all(target_path);
            }
            fs::rename(temp_dir, target_path);

            cleanup();
            return true;
        } catch (...) {
            cleanup();
            return false;
        }
    } catch (...) {
        return false;
    }
}

std::vector<fs::path> BackupManager::listBackupContents(
    const fs::path& backup_path,
    const BackupOptions& options) {
    std::vector<fs::path> contents;
    
    try {
        // 创建临时文件
        auto temp_decrypted = backup_path.string() + ".dec.tmp";
        auto temp_decompressed = backup_path.string() + ".pack.tmp";
        
        auto cleanup = [&]() {
            fs::remove(temp_decrypted);
            fs::remove(temp_decompressed);
        };

        try {
            // 解密和解压文件
            if (!options.password.empty()) {
                if (!encryptor_->decrypt(backup_path, temp_decrypted, options.password)) {
                    cleanup();
                    return contents;
                }
            } else {
                // 如果没有密码，直接复制文件
                fs::copy_file(backup_path, temp_decrypted);
            }
            
            if (!compressor_->decompress(temp_decrypted, temp_decompressed)) {
                cleanup();
                return contents;
            }

            // 获取文件列表
            auto headers = packer_->listContents(temp_decompressed);
            for (const auto& h : headers) {
                contents.push_back(h.path);
            }

            cleanup();
        } catch (...) {
            cleanup();
            contents.clear();
        }
    } catch (...) {
        contents.clear();
    }
    
    return contents;
}

bool BackupManager::verifyBackup(const fs::path& backup_path, const BackupOptions& options) {
    try {
        // 创建临时文件
        auto temp_decrypted = backup_path.string() + ".dec.tmp";
        auto temp_decompressed = backup_path.string() + ".pack.tmp";
        
        auto cleanup = [&]() {
            fs::remove(temp_decrypted);
            fs::remove(temp_decompressed);
        };

        try {
            // 解密和解压文件
            if (!options.password.empty()) {
                std::cout << "[DEBUG] 使用密码进行解密: " << (options.password.empty() ? "empty" : "set") << std::endl;
                if (!encryptor_->decrypt(backup_path, temp_decrypted, options.password)) {
                    std::cout << "[DEBUG] 解密失败" << std::endl;
                    cleanup();
                    return false;
                }
                std::cout << "[DEBUG] 解密成功" << std::endl;
            } else {
                // 如果没有密码，直接复制文件
                std::cout << "[DEBUG] 无密码，直接复制文件" << std::endl;
                fs::copy_file(backup_path, temp_decrypted);
            }
            
            std::cout << "[DEBUG] 开始解压文件" << std::endl;
            if (!compressor_->decompress(temp_decrypted, temp_decompressed)) {
                std::cout << "[DEBUG] 解压失败" << std::endl;
                cleanup();
                return false;
            }
            std::cout << "[DEBUG] 解压成功" << std::endl;

            // 验证文件完整性
            std::cout << "[DEBUG] 开始验证校验和" << std::endl;
            bool result = packer_->verifyChecksum(temp_decompressed);
            std::cout << "[DEBUG] 校验和验证结果: " << std::boolalpha << result << std::endl;
            cleanup();
            return result;
        } catch (const std::exception& e) {
            std::cout << "[DEBUG] 异常: " << e.what() << std::endl;
            cleanup();
            return false;
        } catch (...) {
            std::cout << "[DEBUG] 未知异常" << std::endl;
            cleanup();
            return false;
        }
    } catch (...) {
        std::cout << "[DEBUG] 外层未知异常" << std::endl;
        return false;
    }
}

void BackupManager::collectFiles(const fs::path& dir_path,
                              std::vector<fs::path>& files,
                              const BackupOptions& options) {
    try {
        std::cout << "[DEBUG] 扫描目录: " << dir_path << std::endl;
        std::cout << "[DEBUG] 排除模式: ";
        for (const auto& pattern : options.exclude_patterns) {
            std::cout << pattern << " ";
        }
        std::cout << std::endl;
        
        std::vector<fs::directory_entry> entries;
        
        // 首先收集所有条目
        std::cout << "[DEBUG] 开始收集文件条目..." << std::endl;
        for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
            entries.push_back(entry);
        }
        std::cout << "[DEBUG] 共找到 " << entries.size() << " 个条目" << std::endl;
        
        // 然后处理每个条目
        for (const auto& entry : entries) {
            const auto& path = entry.path();
            std::string filename = path.filename().string();
            bool is_hidden = filename.front() == '.';
            
            std::cout << "[DEBUG] 处理文件: " << path << std::endl;
            
            // 检查是否包含隐藏文件
            if (is_hidden && !options.include_hidden_files) {
                std::cout << "[DEBUG] 跳过隐藏文件: " << path << std::endl;
                continue;
            }
            
            // 检查排除模式
            bool should_exclude = false;
            for (const auto& pattern : options.exclude_patterns) {
                if (!pattern.empty()) {
                    // 如果模式以*开头，只匹配后缀
                    if (pattern.front() == '*') {
                        std::string suffix = pattern.substr(1);  // 去掉*
                        if (filename.length() >= suffix.length() &&
                            filename.compare(filename.length() - suffix.length(), 
                                          suffix.length(), suffix) == 0) {
                            should_exclude = true;
                            std::cout << "[DEBUG] 文件 " << filename << " 匹配排除模式 " << pattern << std::endl;
                            break;
                        }
                    }
                    // 否则进行完整匹配
                    else if (filename.find(pattern) != std::string::npos) {
                        should_exclude = true;
                        std::cout << "[DEBUG] 文件 " << filename << " 匹配排除模式 " << pattern << std::endl;
                        break;
                    }
                }
            }
            
            if (should_exclude) {
                std::cout << "[DEBUG] 跳过排除的文件: " << path << std::endl;
                continue;
            }
            
            if (fs::is_regular_file(path)) {
                std::cout << "[DEBUG] 添加文件: " << path << std::endl;
                files.push_back(path);
            } else {
                std::cout << "[DEBUG] 跳过非普通文件: " << path << std::endl;
            }
        }
        
        std::cout << "[DEBUG] 文件收集完成，共 " << files.size() << " 个文件" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[DEBUG] 收集文件时发生错误: " << e.what() << std::endl;
    }
}

uint64_t BackupManager::calculateRequiredSpace(const fs::path& path) {
    try {
        uint64_t total_size = 0;
        
        if (fs::is_regular_file(path)) {
            total_size = fs::file_size(path);
        } else if (fs::is_directory(path)) {
            for (const auto& entry : fs::recursive_directory_iterator(path)) {
                if (fs::is_regular_file(entry)) {
                    total_size += fs::file_size(entry);
                }
            }
        }
        
        // 考虑压缩和加密的开销，预留 50% 的额外空间
        return total_size * 3 / 2;
    } catch (...) {
        return 0;
    }
}

} // namespace byte_enclave

