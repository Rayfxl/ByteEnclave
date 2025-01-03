#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "filesystem.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>

namespace fs = std::filesystem;
using namespace byte_enclave;

class FileSystemTest : public ::testing::Test {
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
        
        fs_manager_ = std::make_unique<FileSystem>();
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
    std::unique_ptr<FileSystem> fs_manager_;
};

// 测试获取文件元数据
TEST_F(FileSystemTest, GetFileMetadata) {
    auto file_path = createTestFile("test.txt");
    
    auto metadata = fs_manager_->getFileMetadata(file_path);
    
    EXPECT_EQ(metadata.file_type, fs::file_type::regular);
    EXPECT_FALSE(metadata.owner.empty());
    EXPECT_FALSE(metadata.group.empty());
}

// 测试设置文件元数据
TEST_F(FileSystemTest, SetFileMetadata) {
    auto file_path = createTestFile("test_metadata.txt");
    
    auto original_metadata = fs_manager_->getFileMetadata(file_path);
    FileMetadata new_metadata = original_metadata;
    new_metadata.permissions = fs::perms::owner_read | fs::perms::owner_write;
    
    EXPECT_TRUE(fs_manager_->setFileMetadata(file_path, new_metadata));
    
    auto updated_metadata = fs_manager_->getFileMetadata(file_path);
    EXPECT_EQ(updated_metadata.permissions, new_metadata.permissions);
}

// 测试复制文件
TEST_F(FileSystemTest, CopyFile) {
    auto src_path = createTestFile("source.txt", "test content");
    auto dst_path = test_dir_ / "dest.txt";
    
    EXPECT_TRUE(fs_manager_->copyFile(src_path, dst_path));
    EXPECT_TRUE(fs::exists(dst_path));
    
    // 验证内容和元数据
    std::ifstream dst_file(dst_path);
    std::string content;
    std::getline(dst_file, content);
    EXPECT_EQ(content, "test content");
    
    auto src_metadata = fs_manager_->getFileMetadata(src_path);
    auto dst_metadata = fs_manager_->getFileMetadata(dst_path);
    EXPECT_EQ(dst_metadata.permissions, src_metadata.permissions);
}

// 测试创建符号链接
TEST_F(FileSystemTest, CreateSymlink) {
    // 测试正常的符号链接
    auto target_path = createTestFile("target.txt");
    auto link_path = test_dir_ / "link.txt";
    
    EXPECT_TRUE(fs_manager_->createSymlink(target_path, link_path));
    EXPECT_TRUE(fs::is_symlink(link_path));
    
    auto metadata = fs_manager_->getFileMetadata(link_path);
    EXPECT_EQ(metadata.file_type, fs::file_type::symlink);
    
    // 测试悬空链接
    auto dangling_target = test_dir_ / "non_existent.txt";
    auto dangling_link = test_dir_ / "dangling_link.txt";
    
    EXPECT_TRUE(fs_manager_->createSymlink(dangling_target, dangling_link));
    EXPECT_TRUE(fs::is_symlink(dangling_link));
    EXPECT_FALSE(fs::exists(fs::read_symlink(dangling_link)));
    
    // 测试先创建链接后创建目标
    auto future_target = test_dir_ / "future_target.txt";
    auto future_link = test_dir_ / "future_link.txt";
    
    EXPECT_TRUE(fs_manager_->createSymlink(future_target, future_link));
    EXPECT_TRUE(fs::is_symlink(future_link));
    EXPECT_FALSE(fs::exists(fs::read_symlink(future_link)));
    
    // 创建目标文件
    std::ofstream(future_target) << "test content";
    EXPECT_TRUE(fs::exists(fs::read_symlink(future_link)));
}

// 测试创建硬链接
TEST_F(FileSystemTest, CreateHardlink) {
    auto target_path = createTestFile("target.txt");
    auto link_path = test_dir_ / "hardlink.txt";
    
    EXPECT_TRUE(fs_manager_->createHardlink(target_path, link_path));
    EXPECT_TRUE(fs::exists(link_path));
    EXPECT_EQ(fs::hard_link_count(target_path), 2);
}

// 测试创建命名管道
TEST_F(FileSystemTest, CreateNamedPipe) {
    auto pipe_path = test_dir_ / "test.pipe";
    
    EXPECT_TRUE(fs_manager_->createNamedPipe(pipe_path));
    
    auto metadata = fs_manager_->getFileMetadata(pipe_path);
    EXPECT_EQ(metadata.file_type, fs::file_type::fifo);
}

// 测试获取文件类型
TEST_F(FileSystemTest, GetFileType) {
    // 测试普通文件
    auto regular_file = createTestFile("regular.txt");
    EXPECT_EQ(fs_manager_->getFileType(regular_file), fs::file_type::regular);
    
    // 测试目录
    EXPECT_EQ(fs_manager_->getFileType(test_dir_), fs::file_type::directory);
    
    // 测试符号链接
    auto target = createTestFile("target.txt");
    auto symlink = test_dir_ / "symlink.txt";
    fs_manager_->createSymlink(target, symlink);
    EXPECT_EQ(fs_manager_->getFileType(symlink), fs::file_type::symlink);
}

// 测试错误处理
TEST_F(FileSystemTest, ErrorHandling) {
    // 测试不存在的文件
    auto non_existent = test_dir_ / "non_existent.txt";
    EXPECT_THROW(fs_manager_->getFileMetadata(non_existent), std::runtime_error);
    
    // 测试无权限目录中创建链接
    auto no_perm_dir = test_dir_ / "no_perm_dir";
    fs::create_directory(no_perm_dir);
    fs::permissions(no_perm_dir, fs::perms::none);
    
    auto link_path = no_perm_dir / "link.txt";
    auto target_path = test_dir_ / "target.txt";
    EXPECT_FALSE(fs_manager_->createSymlink(target_path, link_path));
    
    // 恢复权限以便清理
    fs::permissions(no_perm_dir, fs::perms::owner_all);
} 