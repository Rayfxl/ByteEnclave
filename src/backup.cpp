#include "backup.hpp"
#include <stack>
#include <vector>
#include <algorithm>
#include <fstream>

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
        if (!fs::exists(source_path)) {
            return false;
        }

        // 2. 检查目标空间
        auto space = fs::space(backup_path.parent_path());
        if (space.available < calculateRequiredSpace(source_path)) {
            return false;
        }

        // 3. 收集要备份的文件
        std::vector<fs::path> files_to_backup;
        if (fs::is_directory(source_path)) {
            collectFiles(source_path, files_to_backup, options);
        } else {
            files_to_backup.push_back(source_path);
        }

        if (files_to_backup.empty()) {
            return false;
        }

        // 4. 创建临时文件
        auto temp_pack = backup_path.string() + ".pack.tmp";
        auto temp_compressed = backup_path.string() + ".gz.tmp";
        auto temp_encrypted = backup_path.string() + ".enc.tmp";
        
        auto cleanup = [&]() {
            fs::remove(temp_pack);
            fs::remove(temp_compressed);
            fs::remove(temp_encrypted);
        };

        try {
            // 5. 打包文件
            if (!packer_->pack(files_to_backup, temp_pack)) {
                cleanup();
                return false;
            }

            // 6. 压缩文件
            if (!compressor_->compress(temp_pack, temp_compressed)) {
                cleanup();
                return false;
            }

            // 7. 加密文件
            if (!options.password.empty()) {
                if (!encryptor_->encrypt(temp_compressed, temp_encrypted, options.password)) {
                    cleanup();
                    return false;
                }
                // 原子性替换
                fs::rename(temp_encrypted, backup_path);
            } else {
                // 如果没有密码，直接使用压缩文件
                fs::rename(temp_compressed, backup_path);
            }

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
    const fs::path& backup_path) {
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
            if (!encryptor_->decrypt(backup_path, temp_decrypted, "")) {
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

bool BackupManager::verifyBackup(const fs::path& backup_path) {
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
            if (!encryptor_->decrypt(backup_path, temp_decrypted, "")) {
                fs::copy_file(backup_path, temp_decrypted);
            }
            
            if (!compressor_->decompress(temp_decrypted, temp_decompressed)) {
                cleanup();
                return false;
            }

            // 验证文件完整性
            bool result = packer_->verifyChecksum(temp_decompressed);
            cleanup();
            return result;
        } catch (...) {
            cleanup();
            return false;
        }
    } catch (...) {
        return false;
    }
}

void BackupManager::collectFiles(const fs::path& dir,
                               std::vector<fs::path>& files,
                               const BackupOptions& options) {
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            const auto& path = entry.path();
            
            // 跳过隐藏文件
            if (!options.include_hidden_files && 
                path.filename().string()[0] == '.') {
                continue;
            }
            
            // 检查排除模式
            bool should_exclude = false;
            for (const auto& pattern : options.exclude_patterns) {
                if (path.string().find(pattern) != std::string::npos) {
                    should_exclude = true;
                    break;
                }
            }
            
            if (should_exclude) continue;
            
            // 只收集常规文件
            if (fs::is_regular_file(path)) {
                files.push_back(path);
            }
        }
    } catch (...) {
        files.clear();
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

