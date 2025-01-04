#pragma once

#include "filesystem.hpp"
#include "packer.hpp"
#include "compressor.hpp"
#include "encryptor.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>

namespace byte_enclave {

namespace fs = std::filesystem;

struct BackupOptions {
    bool include_hidden_files = false;
    std::vector<std::string> exclude_patterns;
    std::string password;  // 加密密码
};

class BackupManager {
public:
    BackupManager();
    
    bool backup(const fs::path& source_path,
               const fs::path& backup_path,
               const BackupOptions& options = BackupOptions{});
    
    bool restore(const fs::path& backup_path,
                const fs::path& target_path,
                const BackupOptions& options = BackupOptions{});
    
    std::vector<fs::path> listBackupContents(const fs::path& backup_path,
                                           const BackupOptions& options = BackupOptions{});
    
    bool verifyBackup(const fs::path& backup_path,
                     const BackupOptions& options = BackupOptions{});
    
protected:
    void collectFiles(const fs::path& dir,
                     std::vector<fs::path>& files,
                     const BackupOptions& options);
    
    uint64_t calculateRequiredSpace(const fs::path& path);

private:
    std::unique_ptr<FileSystem> fs_;
    std::unique_ptr<Packer> packer_;
    std::unique_ptr<Compressor> compressor_;
    std::unique_ptr<Encryptor> encryptor_;
};

} // namespace byte_enclave