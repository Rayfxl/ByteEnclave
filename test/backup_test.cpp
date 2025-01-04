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

    fs::path createTestFile(const std::string& name, const std::string& content = "test") {
        fs::path file_path = test_dir_ / name;
        fs::create_directories(file_path.parent_path());
        std::ofstream file(file_path);
        file << content;
        return file_path;
    }

    fs::path test_dir_;
    std::unique_ptr<BackupManager> backup_manager_;
};

// 1. 单文件备份测试
TEST_F(BackupTest, SingleFileBackup) {
    auto source_file = createTestFile("test.txt", "test content");
    auto backup_path = test_dir_ / "backup.bak";
    auto restore_path = test_dir_ / "restore";

    BackupOptions options;
    EXPECT_TRUE(backup_manager_->backup(source_file, backup_path, options));
    EXPECT_TRUE(fs::is_regular_file(backup_path));
    EXPECT_TRUE(backup_manager_->restore(backup_path, restore_path, options));

    auto restored_file = restore_path / "test.txt";
    EXPECT_TRUE(fs::exists(restored_file));
    
    std::ifstream file(restored_file);
    std::string content;
    std::getline(file, content);
    EXPECT_EQ(content, "test content");
}

// 2. 目录备份测试
TEST_F(BackupTest, DirectoryBackup) {
    // 创建测试目录结构
    auto source_dir = test_dir_ / "source";
    fs::create_directories(source_dir);
    createTestFile("source/file1.txt", "content1"); 
    createTestFile("source/sub/file2.txt", "content2");

    auto backup_path = test_dir_ / "backup.bak";
    auto restore_path = test_dir_ / "restore";

    BackupOptions options;
    EXPECT_TRUE(backup_manager_->backup(source_dir, backup_path, options));
    EXPECT_TRUE(backup_manager_->restore(backup_path, restore_path, options));

    // 验证目录结构和内容
    EXPECT_TRUE(fs::exists(restore_path / "file1.txt"));
    EXPECT_TRUE(fs::exists(restore_path / "sub/file2.txt"));

    std::ifstream file1(restore_path / "file1.txt");
    std::string content;
    std::getline(file1, content);
    EXPECT_EQ(content, "content1");
}

// 3. 链接处理测试 
TEST_F(BackupTest, SymlinkBackup) {
     // 创建目标文件和链接
    createTestFile("target.txt", "target content");
    fs::create_symlink(test_dir_ / "target.txt", test_dir_ / "link.txt");

    auto backup_path = test_dir_ / "backup.bak";
    auto restore_path = test_dir_ / "restore";

    BackupOptions options;
    options.include_symlinks = true;  // 启用符号链接支持
    EXPECT_TRUE(backup_manager_->backup(test_dir_, backup_path, options));
    EXPECT_TRUE(backup_manager_->restore(backup_path, restore_path, options));

    // 验证链接
    EXPECT_TRUE(fs::is_symlink(restore_path / "link.txt")); 
    auto target = fs::read_symlink(restore_path / "link.txt");
    EXPECT_EQ(target, test_dir_ / "target.txt");
}

// 4. 过滤测试
TEST_F(BackupTest, FilterBackup) {
    createTestFile("visible.txt");
    createTestFile(".hidden.txt");
    
    auto backup_path = test_dir_ / "backup.bak";
    auto restore_path = test_dir_ / "restore";

    BackupOptions options;
    options.include_hidden_files = false;
    EXPECT_TRUE(backup_manager_->backup(test_dir_, backup_path, options));
    EXPECT_TRUE(backup_manager_->restore(backup_path, restore_path, options));

    EXPECT_TRUE(fs::exists(restore_path / "visible.txt"));
    EXPECT_FALSE(fs::exists(restore_path / ".hidden.txt"));
}

// 5. 错误处理测试
TEST_F(BackupTest, ErrorHandling) {
    auto non_existent = test_dir_ / "non_existent";
    auto backup_path = test_dir_ / "backup.bak";
    auto restore_path = test_dir_ / "restore";

    BackupOptions options;
    EXPECT_FALSE(backup_manager_->backup(non_existent, backup_path, options));
    EXPECT_FALSE(backup_manager_->restore(non_existent, restore_path, options));
}

// 6. 空目录测试
TEST_F(BackupTest, EmptyDirectoryBackup) {
    auto empty_dir = test_dir_ / "empty";
    fs::create_directories(empty_dir);

    auto backup_path = test_dir_ / "backup.bak";
    auto restore_path = test_dir_ / "restore";

    BackupOptions options;
    EXPECT_FALSE(backup_manager_->backup(empty_dir, backup_path, options));
}

// 7. 大文件测试
TEST_F(BackupTest, LargeFileBackup) {
    auto large_file = test_dir_ / "large.dat";
    {
        std::ofstream file(large_file, std::ios::binary);
        std::vector<char> buffer(1024, 'X');
        for(int i = 0; i < 1024; ++i) {
            file.write(buffer.data(), buffer.size());
        }
    }

    auto backup_path = test_dir_ / "backup.bak";
    auto restore_path = test_dir_ / "restore";

    BackupOptions options;
    EXPECT_TRUE(backup_manager_->backup(large_file, backup_path, options));
    EXPECT_TRUE(backup_manager_->restore(backup_path, restore_path, options));

    EXPECT_TRUE(fs::exists(restore_path / "large.dat"));
    EXPECT_EQ(fs::file_size(restore_path / "large.dat"), 
              fs::file_size(large_file));
}

// 8. 完整性验证测试
TEST_F(BackupTest, BackupVerification) {
    auto source_file = createTestFile("test.txt", "test content");
    auto backup_path = test_dir_ / "backup.bak";
    auto restore_path = test_dir_ / "restore";
    
    BackupOptions options;
    EXPECT_TRUE(backup_manager_->backup(source_file, backup_path, options));
    
    // 破坏备份文件
    {
        std::ofstream file(backup_path, std::ios::binary | std::ios::app);
        file.write("corrupt", 7);
    }
    
    // 验证还原失败
    EXPECT_FALSE(backup_manager_->restore(backup_path, restore_path, options));
}