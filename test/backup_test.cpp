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
        test_dir_ = fs::temp_directory_path() / "byte_enclave_test";
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
        fs::create_directories(test_dir_);
        backup_manager_ = std::make_unique<BackupManager>();
    }

    void TearDown() override {
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    fs::path createTestFile(const std::string& name, const std::string& content = "test") {
        fs::path file_path = test_dir_ / name;
        fs::create_directories(file_path.parent_path());
        std::ofstream file(file_path);
        file << content;
        return file_path;
    }

    bool compareFiles(const fs::path& file1, const fs::path& file2) {
        std::ifstream f1(file1, std::ios::binary);
        std::ifstream f2(file2, std::ios::binary);
        if (!f1.is_open() || !f2.is_open()) return false;

        std::vector<char> buf1(8192), buf2(8192);
        while (f1.good() && f2.good()) {
            f1.read(buf1.data(), buf1.size());
            f2.read(buf2.data(), buf2.size());
            if (f1.gcount() != f2.gcount()) return false;
            if (f1.gcount() == 0) break;
            if (std::memcmp(buf1.data(), buf2.data(), f1.gcount()) != 0) return false;
        }
        return f1.eof() && f2.eof();
    }

    fs::path test_dir_;
    std::unique_ptr<BackupManager> backup_manager_;
};

// 测试完整的备份流程
TEST_F(BackupTest, CompleteBackupFlow) {
    // 1. 创建测试文件和目录结构
    auto source_dir = test_dir_ / "source";
    fs::create_directories(source_dir);
    
    // 普通文件
    auto file1 = createTestFile("source/file1.txt", "content1");
    auto file2 = createTestFile("source/subdir/file2.txt", "content2");
    
    // 大文件（用于测试压缩）
    auto large_file = createTestFile("source/large.txt", std::string(1024 * 1024, 'A'));
    
    // 隐藏文件（用于测试过滤）
    auto hidden_file = createTestFile("source/.hidden.txt", "hidden");
    
    // 2. 设置备份选项
    BackupOptions options;
    options.include_hidden_files = false;  // 不包含隐藏文件
    options.exclude_patterns = {".bak$"};  // 排除 .bak 文件
    options.password = "test_password";    // 设置加密密码
    
    // 3. 执行备份
    auto backup_path = test_dir_ / "backup.bak";
    EXPECT_TRUE(backup_manager_->backup(source_dir, backup_path, options));
    
    // 4. 验证备份文件
    EXPECT_TRUE(fs::exists(backup_path));
    EXPECT_GT(fs::file_size(backup_path), 0);
    EXPECT_TRUE(backup_manager_->verifyBackup(backup_path));
    
    // 5. 检查备份内容列表
    auto contents = backup_manager_->listBackupContents(backup_path);
    EXPECT_EQ(contents.size(), 3);  // file1.txt, file2.txt, large.txt
    EXPECT_TRUE(std::find(contents.begin(), contents.end(), "file1.txt") != contents.end());
    EXPECT_TRUE(std::find(contents.begin(), contents.end(), "subdir/file2.txt") != contents.end());
    EXPECT_TRUE(std::find(contents.begin(), contents.end(), "large.txt") != contents.end());
    EXPECT_FALSE(std::find(contents.begin(), contents.end(), ".hidden.txt") != contents.end());
    
    // 6. 执行还原
    auto restore_path = test_dir_ / "restore";
    EXPECT_TRUE(backup_manager_->restore(backup_path, restore_path, options));
    
    // 7. 验证还原的文件
    EXPECT_TRUE(fs::exists(restore_path / "file1.txt"));
    EXPECT_TRUE(fs::exists(restore_path / "subdir/file2.txt"));
    EXPECT_TRUE(fs::exists(restore_path / "large.txt"));
    EXPECT_FALSE(fs::exists(restore_path / ".hidden.txt"));
    
    EXPECT_TRUE(compareFiles(file1, restore_path / "file1.txt"));
    EXPECT_TRUE(compareFiles(file2, restore_path / "subdir/file2.txt"));
    EXPECT_TRUE(compareFiles(large_file, restore_path / "large.txt"));
}

// 测试错误处理
TEST_F(BackupTest, ErrorHandling) {
    BackupOptions options;
    options.password = "test_password";
    
    // 1. 测试不存在的源路径
    EXPECT_FALSE(backup_manager_->backup(
        test_dir_ / "nonexistent",
        test_dir_ / "backup.bak",
        options
    ));
    
    // 2. 测试空目录
    auto empty_dir = test_dir_ / "empty";
    fs::create_directories(empty_dir);
    EXPECT_FALSE(backup_manager_->backup(empty_dir, test_dir_ / "backup.bak", options));
    
    // 3. 测试不存在的备份文件
    EXPECT_FALSE(backup_manager_->restore(
        test_dir_ / "nonexistent.bak",
        test_dir_ / "restore",
        options
    ));
    
    // 4. 测试无效的备份文件
    auto invalid_backup = test_dir_ / "invalid.bak";
    {
        std::ofstream file(invalid_backup);
        file << "This is not a valid backup file";
    }
    EXPECT_FALSE(backup_manager_->restore(invalid_backup, test_dir_ / "restore", options));
    
    // 5. 测试密码错误
    auto source_file = createTestFile("test.txt", "test");
    auto backup_path = test_dir_ / "backup.bak";
    EXPECT_TRUE(backup_manager_->backup(source_file, backup_path, options));
    
    options.password = "wrong_password";
    EXPECT_FALSE(backup_manager_->restore(backup_path, test_dir_ / "restore", options));
}