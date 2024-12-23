#pragma once

#include "filesystem.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <stdexcept>
#include <cstdint>

namespace byte_enclave {

namespace fs = std::filesystem;

struct BackupOptions {
    bool include_symlinks = true;
    bool include_hidden_files = false;
    std::vector<std::string> exclude_patterns;
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
    
    std::vector<fs::path> listBackupContents(const fs::path& backup_path);
    
    bool verifyBackup(const fs::path& backup_path);
    
protected:
    bool verifyFile(const fs::path& path);
    uint32_t calculateChecksum(const fs::path& path);
    void collectFiles(const fs::path& dir,
                     std::vector<fs::path>& files,
                     const BackupOptions& options);
    
    uint32_t calculateExpectedChecksum(const fs::path& path);
    
private:
    std::unique_ptr<FileSystem> fs_;
};

} // namespace byte_enclave