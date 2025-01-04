#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sys/sysmacros.h>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <thread>
#include "filesystem.hpp"

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
    // 准备测试文件
    auto file_path = createTestFile("test.txt", "test content");
    
    // 设置特定权限用于测试
    fs::permissions(file_path, 
                   fs::perms::owner_read | fs::perms::owner_write,
                   fs::perm_options::replace);
    
    // 1. 正常情况测试
    {
        auto metadata = fs_manager_->getFileMetadata(file_path);
        
        // 基本信息验证
        EXPECT_EQ(metadata.file_type, fs::file_type::regular);
        EXPECT_FALSE(metadata.owner.empty());
        EXPECT_FALSE(metadata.group.empty());
        
        // 权限验证
        EXPECT_TRUE((metadata.permissions & fs::perms::owner_read) != fs::perms::none);
        EXPECT_TRUE((metadata.permissions & fs::perms::owner_write) != fs::perms::none);
        
        // 时间戳验证（增加下限检查）
        auto current_time = fs::file_time_type::clock::now();
        auto time_margin = std::chrono::seconds(1);
        auto min_time = current_time - std::chrono::hours(1);
        
        EXPECT_LE(metadata.create_time, current_time + time_margin);
        EXPECT_GE(metadata.create_time, min_time);
        EXPECT_LE(metadata.modify_time, current_time + time_margin);
        EXPECT_GE(metadata.modify_time, min_time);
        EXPECT_LE(metadata.access_time, current_time + time_margin);
        EXPECT_GE(metadata.access_time, min_time);
    }
    
    // 2. 异常情况测试
    {
        // 文件不存在
        EXPECT_THROW(fs_manager_->getFileMetadata(test_dir_ / "nonexistent.txt"),
                     std::runtime_error);
                     
        // 权限不足
        fs::permissions(file_path, fs::perms::none, fs::perm_options::replace);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT_THROW(fs_manager_->getFileMetadata(file_path), std::runtime_error);
    }
    
}

// 测试设置文件元数据
TEST_F(FileSystemTest, SetFileMetadata) {
    // 1. 基本权限测试
    {
        auto file_path = createTestFile("test_metadata.txt");
        auto original_metadata = fs_manager_->getFileMetadata(file_path);
        FileMetadata new_metadata = original_metadata;
        new_metadata.permissions = fs::perms::owner_read | fs::perms::owner_write;
        
        EXPECT_TRUE(fs_manager_->setFileMetadata(file_path, new_metadata));
        
        auto updated_metadata = fs_manager_->getFileMetadata(file_path);
        EXPECT_EQ(updated_metadata.permissions, new_metadata.permissions);
    }

    // 2. 修改时间测试
    {
        auto file_path = createTestFile("test_time.txt");
        auto original_metadata = fs_manager_->getFileMetadata(file_path);
        FileMetadata new_metadata = original_metadata;
        
        auto new_time = fs::file_time_type::clock::now() + std::chrono::hours(1);
        new_metadata.modify_time = new_time;
        
        EXPECT_TRUE(fs_manager_->setFileMetadata(file_path, new_metadata));
        
        auto updated_metadata = fs_manager_->getFileMetadata(file_path);
        auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(
            updated_metadata.modify_time.time_since_epoch() - 
            new_time.time_since_epoch()
        ).count();
        
        EXPECT_LE(std::abs(time_diff), 1);
    }

    // 3. 异常情况测试
    {
        // 文件不存在
        EXPECT_FALSE(fs_manager_->setFileMetadata(
            test_dir_ / "nonexistent.txt", FileMetadata{}));
            
        // 权限不足
        auto file_path = createTestFile("test_perm.txt");
        fs::permissions(file_path, fs::perms::none, fs::perm_options::replace);
        EXPECT_FALSE(fs_manager_->setFileMetadata(file_path, FileMetadata{}));
    }
}

// 测试复制文件
TEST_F(FileSystemTest, CopyFile) {
    // 1. 普通文件测试
    {
        auto src_path = createTestFile("source.txt", "test content");
        auto dst_path = test_dir_ / "dest.txt";
        
        EXPECT_TRUE(fs_manager_->copyFile(src_path, dst_path));
        
        // 验证内容
        std::ifstream dst_file(dst_path);
        std::string content;
        std::getline(dst_file, content);
        EXPECT_EQ(content, "test content");
        
        // 验证元数据
        auto src_metadata = fs_manager_->getFileMetadata(src_path);
        auto dst_metadata = fs_manager_->getFileMetadata(dst_path);
        EXPECT_EQ(dst_metadata.permissions, src_metadata.permissions);
    }

    // 2. 符号链接测试
    {
        auto target = createTestFile("target.txt", "target content");
        auto src_link = test_dir_ / "src_link";
        auto dst_link = test_dir_ / "dst_link";
        
        fs::create_symlink(target, src_link);
        EXPECT_TRUE(fs_manager_->copyFile(src_link, dst_link));
        EXPECT_EQ(fs::read_symlink(dst_link), target);
    }

    // 3. 目录测试
    {
        auto src_dir = test_dir_ / "src_dir";
        auto dst_dir = test_dir_ / "dst_dir";
        fs::create_directory(src_dir);
        
        EXPECT_TRUE(fs_manager_->copyFile(src_dir, dst_dir));
        EXPECT_TRUE(fs::is_directory(dst_dir));
    }

    // 4. FIFO测试
    {
        auto src_fifo = test_dir_ / "src.fifo";
        auto dst_fifo = test_dir_ / "dst.fifo";
        ASSERT_EQ(mkfifo(src_fifo.c_str(), 0666), 0);
        
        EXPECT_TRUE(fs_manager_->copyFile(src_fifo, dst_fifo));
        
        struct stat st;
        ASSERT_EQ(lstat(dst_fifo.c_str(), &st), 0);
        EXPECT_TRUE(S_ISFIFO(st.st_mode));
    }

    // 5. 设备文件测试 (仅root可执行)
    if (geteuid() == 0) {
        // 字符设备测试
        {
            auto src_char = test_dir_ / "char_dev";
            auto dst_char = test_dir_ / "char_dev_copy";
            
            // 创建字符设备 (例如null设备)
            ASSERT_EQ(mknod(src_char.c_str(), S_IFCHR | 0666, makedev(1, 3)), 0);
            
            EXPECT_TRUE(fs_manager_->copyFile(src_char, dst_char));
            
            struct stat st;
            ASSERT_EQ(lstat(dst_char.c_str(), &st), 0);
            EXPECT_TRUE(S_ISCHR(st.st_mode));
            EXPECT_EQ(major(st.st_rdev), 1);
            EXPECT_EQ(minor(st.st_rdev), 3);
        }

        // 块设备测试
        {
            auto src_block = test_dir_ / "block_dev";
            auto dst_block = test_dir_ / "block_dev_copy";
            
            // 创建块设备
            ASSERT_EQ(mknod(src_block.c_str(), S_IFBLK | 0666, makedev(7, 0)), 0);
            
            EXPECT_TRUE(fs_manager_->copyFile(src_block, dst_block));
            
            struct stat st;
            ASSERT_EQ(lstat(dst_block.c_str(), &st), 0);
            EXPECT_TRUE(S_ISBLK(st.st_mode));
            EXPECT_EQ(major(st.st_rdev), 7);
            EXPECT_EQ(minor(st.st_rdev), 0);
        }
    }

    // 6. 错误情况测试
    {
        EXPECT_FALSE(fs_manager_->copyFile(
            test_dir_ / "nonexistent.txt",
            test_dir_ / "dest.txt"
        ));

        auto no_read = createTestFile("no_read.txt");
        fs::permissions(no_read, fs::perms::none);
        EXPECT_FALSE(fs_manager_->copyFile(
            no_read,
            test_dir_ / "dest.txt"
        ));
    }
}

// 测试创建符号链接
TEST_F(FileSystemTest, CreateSymlink) {
    // 1. 正常符号链接测试
    {
        auto target_path = createTestFile("target.txt", "test content");
        auto link_path = test_dir_ / "link.txt";
        EXPECT_TRUE(fs_manager_->createSymlink(target_path, link_path));
        EXPECT_TRUE(fs::is_symlink(link_path));
        EXPECT_EQ(fs::read_symlink(link_path), target_path);
    }

    // 2. 悬空链接测试
    {
        auto nonexist_target = test_dir_ / "nonexist.txt";
        auto link_path = test_dir_ / "dangling_link.txt";
        EXPECT_TRUE(fs_manager_->createSymlink(nonexist_target, link_path));
        EXPECT_TRUE(fs::is_symlink(link_path));
    }

    // 3. 覆盖已有链接测试
    {
        auto target1 = createTestFile("target1.txt");
        auto target2 = createTestFile("target2.txt");
        auto link_path = test_dir_ / "overwrite_link.txt";
        
        EXPECT_TRUE(fs_manager_->createSymlink(target1, link_path));
        EXPECT_TRUE(fs_manager_->createSymlink(target2, link_path));
        EXPECT_EQ(fs::read_symlink(link_path), target2);
    }

    // 4. 目标目录权限不足测试
    {
        auto target = createTestFile("target3.txt");
        auto no_perm_dir = test_dir_ / "no_perm";
        fs::create_directory(no_perm_dir);
        fs::permissions(no_perm_dir, fs::perms::none);
        
        EXPECT_FALSE(fs_manager_->createSymlink(
            target,
            no_perm_dir / "link.txt"
        ));
    }

    // 5. 无效目标目录测试
    {
        auto target = createTestFile("target4.txt");
        auto link_path = test_dir_ / "nonexist_dir" / "link.txt";
        EXPECT_FALSE(fs_manager_->createSymlink(target, link_path));
    }
}

// 测试创建硬链接
TEST_F(FileSystemTest, CreateHardlink) {
    // 1. 基本硬链接测试
    {
        auto target_path = createTestFile("target.txt", "test content");
        auto link_path = test_dir_ / "hardlink.txt";
        
        EXPECT_TRUE(fs_manager_->createHardlink(target_path, link_path));
        EXPECT_TRUE(fs::exists(link_path));
        EXPECT_EQ(fs::hard_link_count(target_path), 2);
        
        // 验证内容
        std::ifstream link_file(link_path);
        std::string content;
        std::getline(link_file, content);
        EXPECT_EQ(content, "test content");
    }

    // 2. 非常规文件测试
    {
        // 目录
        auto dir_path = test_dir_ / "test_dir";
        fs::create_directory(dir_path);
        EXPECT_FALSE(fs_manager_->createHardlink(
            dir_path,
            test_dir_ / "dir_link"
        ));

        // 符号链接
        auto symlink_target = createTestFile("symlink_target.txt");
        auto symlink_path = test_dir_ / "symlink.txt";
        fs::create_symlink(symlink_target, symlink_path);
        EXPECT_FALSE(fs_manager_->createHardlink(
            symlink_path,
            test_dir_ / "hardlink_to_symlink"
        ));
    }

    // 3. 目标不存在测试
    {
        EXPECT_FALSE(fs_manager_->createHardlink(
            test_dir_ / "nonexistent.txt",
            test_dir_ / "bad_link.txt"
        ));
    }

    // 4. 权限测试
    {
        auto target_path = createTestFile("no_perm_target.txt");
        auto no_perm_dir = test_dir_ / "no_perm";
        fs::create_directory(no_perm_dir);
        fs::permissions(no_perm_dir, fs::perms::none);
        
        EXPECT_FALSE(fs_manager_->createHardlink(
            target_path,
            no_perm_dir / "link.txt"
        ));
    }

    // 5. 跨文件系统测试
    {
        auto target_path = createTestFile("cross_fs_target.txt");
        auto tmp_link_path = fs::temp_directory_path() / "cross_fs_link.txt";
        
        struct stat st1, st2;
        if (stat(target_path.c_str(), &st1) == 0 && 
            stat(fs::temp_directory_path().c_str(), &st2) == 0 && 
            st1.st_dev != st2.st_dev) {
            EXPECT_FALSE(fs_manager_->createHardlink(target_path, tmp_link_path));
        }
    }
}

// 测试创建命名管道
TEST_F(FileSystemTest, CreateNamedPipe) {
    // 1. 基本功能测试
    {
        auto pipe_path = test_dir_ / "test.pipe";
        EXPECT_TRUE(fs_manager_->createNamedPipe(pipe_path));
        
        auto metadata = fs_manager_->getFileMetadata(pipe_path);
        EXPECT_EQ(metadata.file_type, fs::file_type::fifo);
        EXPECT_EQ(metadata.permissions & fs::perms::owner_read, fs::perms::owner_read);
        EXPECT_EQ(metadata.permissions & fs::perms::owner_write, fs::perms::owner_write);
    }

    // 2. 覆盖已存在管道测试
    {
        auto pipe_path = test_dir_ / "existing.pipe";
        EXPECT_TRUE(fs_manager_->createNamedPipe(pipe_path));
        EXPECT_TRUE(fs_manager_->createNamedPipe(pipe_path));
        EXPECT_TRUE(fs::is_fifo(pipe_path));
    }

    // 3. 权限不足测试
    {
        auto no_perm_dir = test_dir_ / "no_perm";
        fs::create_directory(no_perm_dir);
        fs::permissions(no_perm_dir, fs::perms::none);
        
        EXPECT_FALSE(fs_manager_->createNamedPipe(
            no_perm_dir / "test.pipe"
        ));
    }

    // 4. 目标目录不存在测试
    {
        EXPECT_FALSE(fs_manager_->createNamedPipe(
            test_dir_ / "nonexistent_dir" / "test.pipe"
        ));
    }
}
