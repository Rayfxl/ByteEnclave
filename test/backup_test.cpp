#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "backup.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>

namespace fs = std::filesystem;
using namespace byte_enclave;

class BackupTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试目录
        test_dir_ = fs::temp_directory_path() / "byte_enclave_test";
        
        // 如果目录已存在，先删除它
        if (fs::exists(test_dir_)) {
            try {
                fs::remove_all(test_dir_);
            } catch (const fs::filesystem_error&) {
                // 如果删除失败，可能是权限问题，尝试使用系统命令删除
                std::string cmd = "rm -rf " + test_dir_.string();
                if (system(cmd.c_str()) != 0) {
                    throw std::runtime_error("Failed to remove test directory");
                }
            }
        }
        
        // 创建新的测试目录
        fs::create_directories(test_dir_);
        
        // 设置目录权限为 777
        fs::permissions(test_dir_,
                       fs::perms::owner_all | fs::perms::group_all | fs::perms::others_all,
                       fs::perm_options::replace);
        
        backup_manager_ = std::make_unique<BackupManager>();
    }

    void TearDown() override {
        try {
            // 清理测试目录
            fs::remove_all(test_dir_);
        } catch (const fs::filesystem_error&) {
            // 如果删除失败，可能是权限问题，尝试使用系统命令删除
            std::string cmd = "rm -rf " + test_dir_.string();
            system(cmd.c_str());  // 忽略返回值，因为这是清理阶段
        }
    }

    // 创建测试文件
    fs::path createTestFile(const std::string& name, const std::string& content = "test") {
        fs::path file_path = test_dir_ / name;
        std::ofstream file(file_path);
        file << content;
        file.close();
        return file_path;
    }

    fs::path test_dir_;
    std::unique_ptr<BackupManager> backup_manager_;
};

// 测试基本备份功能
TEST_F(BackupTest, BasicBackup) {
    auto source_file = createTestFile("source.txt", "test content");
    auto backup_path = test_dir_ / "backup";
    auto restore_path = test_dir_ / "restore";
    
    BackupOptions options;
    EXPECT_TRUE(backup_manager_->backup(source_file, backup_path, options));
    EXPECT_TRUE(backup_manager_->restore(backup_path, restore_path, options));
    
    auto restored_file = restore_path / "source.txt";
    EXPECT_TRUE(fs::exists(restored_file));
    
    std::ifstream file(restored_file);
    std::string content;
    std::getline(file, content);
    EXPECT_EQ(content, "test content");
}

// 测试元数据保留
TEST_F(BackupTest, MetadataPreservation) {
    auto source_file = createTestFile("source.txt");
    auto backup_path = test_dir_ / "backup";
    auto restore_path = test_dir_ / "restore";
    
    // 设置源文件的元数据
    auto fs_manager = std::make_unique<FileSystem>();
    auto original_metadata = fs_manager->getFileMetadata(source_file);
    original_metadata.permissions = fs::perms::owner_read | fs::perms::owner_write;
    fs_manager->setFileMetadata(source_file, original_metadata);
    
    // 备份和还原
    BackupOptions options;
    EXPECT_TRUE(backup_manager_->backup(source_file, backup_path, options));
    EXPECT_TRUE(backup_manager_->restore(backup_path, restore_path, options));
    
    // 验证元数据
    auto restored_file = restore_path / "source.txt";
    auto restored_metadata = fs_manager->getFileMetadata(restored_file);
    EXPECT_EQ(restored_metadata.permissions, original_metadata.permissions);
}

// 测试符号链接处理
TEST_F(BackupTest, SymlinkHandling) {
    // ���建目标文件（使用相对路径）
    createTestFile("target.txt");
    auto symlink_path = test_dir_ / "link.txt";
    
    // 使用相对路径创建符号链接
    fs::create_symlink("target.txt", symlink_path);
    
    auto backup_path = test_dir_ / "backup";
    auto restore_path = test_dir_ / "restore";
    
    BackupOptions options;
    options.include_symlinks = true;
    EXPECT_TRUE(backup_manager_->backup(symlink_path, backup_path, options));
    EXPECT_TRUE(backup_manager_->restore(backup_path, restore_path, options));
    
    auto restored_link = restore_path / "link.txt";
    EXPECT_TRUE(fs::is_symlink(restored_link));
    EXPECT_EQ(fs::read_symlink(restored_link), "target.txt");
}

// 测试文件过滤
TEST_F(BackupTest, FileFiltering) {
    // 创建测试文件
    auto file1 = createTestFile("file1.txt");
    auto file2 = createTestFile(".hidden.txt");
    auto backup_path = test_dir_ / "backup";
    auto restore_path = test_dir_ / "restore";
    
    BackupOptions options;
    options.include_hidden_files = false;
    options.exclude_patterns = {".hidden"};
    
    EXPECT_TRUE(backup_manager_->backup(test_dir_, backup_path, options));
    EXPECT_TRUE(backup_manager_->restore(backup_path, restore_path, options));
    
    // 验证过滤结果
    EXPECT_TRUE(fs::exists(restore_path / "file1.txt"));
    EXPECT_FALSE(fs::exists(restore_path / ".hidden.txt"));
}

// 测试备份验证
TEST_F(BackupTest, BackupVerification) {
    auto source_file = createTestFile("source.txt");
    auto backup_path = test_dir_ / "backup";
    
    BackupOptions options;
    EXPECT_TRUE(backup_manager_->backup(source_file, backup_path, options));
    EXPECT_TRUE(backup_manager_->verifyBackup(backup_path));
    
    // 破坏备份文件
    auto backup_file_path = backup_path / "source.txt";
    std::ofstream backup_file(backup_file_path, std::ios::binary | std::ios::app);
    backup_file << "corrupted";
    backup_file.close();
    
    EXPECT_FALSE(backup_manager_->verifyBackup(backup_path));
}

// 测试错误处理
TEST_F(BackupTest, ErrorHandling) {
    auto backup_path = test_dir_ / "backup";
    auto restore_path = test_dir_ / "restore";
    
    // 测试不存在的源文件
    auto non_existent = test_dir_ / "non_existent.txt";
    BackupOptions options;
    EXPECT_FALSE(backup_manager_->backup(non_existent, backup_path, options));
    
    // 测试无效的备份文件
    EXPECT_FALSE(backup_manager_->restore(non_existent, restore_path, options));
    
    // 测试无权限的目标目录
    auto no_perm_dir = test_dir_ / "no_perm";
    fs::create_directories(no_perm_dir);
    fs::permissions(no_perm_dir, fs::perms::none);
    EXPECT_FALSE(backup_manager_->backup(test_dir_, no_perm_dir, options));
} 