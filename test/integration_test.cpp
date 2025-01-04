#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "filesystem.hpp"
#include "packer.hpp"
#include "backup.hpp"
#include "encryptor.hpp"
#include "compressor.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <random>
#include <thread>
#include <mutex>

namespace fs = std::filesystem;
using namespace byte_enclave;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试目录
        test_dir_ = fs::temp_directory_path() / "byte_enclave_integration_test";
        
        // 如果目录已存在，先删除它
        if (fs::exists(test_dir_)) {
            try {
                fs::remove_all(test_dir_);
            } catch (const fs::filesystem_error&) {
                std::string cmd = "rm -rf " + test_dir_.string();
                if (system(cmd.c_str()) != 0) {
                    throw std::runtime_error("Failed to remove test directory");
                }
            }
        }
        
        // 创建新的测试目录
        fs::create_directories(test_dir_);
        
        // 初始化所有组件
        fs_manager_ = std::make_unique<FileSystem>();
        packer_ = std::make_unique<Packer>();
        backup_manager_ = std::make_unique<BackupManager>();
        encryptor_ = std::make_unique<Encryptor>();
        compressor_ = std::make_unique<Compressor>();
    }

    void TearDown() override {
        try {
            fs::remove_all(test_dir_);
        } catch (const fs::filesystem_error&) {
            std::string cmd = "rm -rf " + test_dir_.string();
            system(cmd.c_str());
        }
    }

    // 创建测试文件
    fs::path createTestFile(const std::string& name, const std::string& content = "test") {
        fs::path file_path = test_dir_ / name;
        fs::create_directories(file_path.parent_path());
        std::ofstream file(file_path);
        file << content;
        return file_path;
    }

    // 比较两个文件内容是否相同
    bool compareFiles(const fs::path& file1, const fs::path& file2) {
        std::ifstream f1(file1, std::ios::binary);
        std::ifstream f2(file2, std::ios::binary);

        if (!f1.is_open() || !f2.is_open()) return false;

        constexpr size_t BUFFER_SIZE = 8192;
        std::vector<char> buf1(BUFFER_SIZE);
        std::vector<char> buf2(BUFFER_SIZE);

        while (f1.good() && f2.good()) {
            f1.read(buf1.data(), BUFFER_SIZE);
            f2.read(buf2.data(), BUFFER_SIZE);

            if (f1.gcount() != f2.gcount()) return false;
            if (f1.gcount() == 0) break;

            if (std::memcmp(buf1.data(), buf2.data(), f1.gcount()) != 0) return false;
        }

        return f1.eof() && f2.eof();
    }

    fs::path test_dir_;
    std::unique_ptr<FileSystem> fs_manager_;
    std::unique_ptr<Packer> packer_;
    std::unique_ptr<BackupManager> backup_manager_;
    std::unique_ptr<Encryptor> encryptor_;
    std::unique_ptr<Compressor> compressor_;
};

// 测试完整的备份加密流程
TEST_F(IntegrationTest, FullBackupEncryptionFlow) {
    // 1. 创建测试文件和目录结构
    auto source_dir = test_dir_ / "source";
    fs::create_directories(source_dir);
    
    auto file1 = createTestFile("source/file1.txt", "Important content 1");
    auto file2 = createTestFile("source/subdir/file2.txt", "Important content 2");
    auto file3 = createTestFile("source/large_file.dat", std::string(1024 * 1024, 'X')); // 1MB file

    // 2. 创建打包文件
    auto pack_path = test_dir_ / "backup.pack";
    EXPECT_TRUE(packer_->pack({file1, file2, file3}, pack_path));

    // 3. 压缩打包文件
    auto compressed_path = test_dir_ / "backup.pack.gz";
    EXPECT_TRUE(compressor_->compress(pack_path, compressed_path));

    // 4. 加密压缩后的文件
    auto encrypted_path = test_dir_ / "backup.pack.gz.enc";
    std::string password = "test_password123";
    EXPECT_TRUE(encryptor_->encrypt(compressed_path, encrypted_path, password));

    // 5. 创建备份
    auto backup_path = test_dir_ / "final_backup.bak";
    BackupOptions options;
    EXPECT_TRUE(backup_manager_->backup(encrypted_path, backup_path, options));

    // 6. 恢复过程
    auto restore_dir = test_dir_ / "restore";
    fs::create_directories(restore_dir);

    // 6.1 从备份中恢复加密文件
    auto restored_encrypted = restore_dir / "restored.enc";
    EXPECT_TRUE(backup_manager_->restore(backup_path, restored_encrypted, options));

    // 6.2 解密文件
    auto decrypted_path = restore_dir / "decrypted.gz";
    EXPECT_TRUE(encryptor_->decrypt(restored_encrypted, decrypted_path, password));

    // 6.3 解压文件
    auto decompressed_path = restore_dir / "decompressed.pack";
    EXPECT_TRUE(compressor_->decompress(decrypted_path, decompressed_path));

    // 6.4 解包文件
    auto final_restore_dir = restore_dir / "final";
    EXPECT_TRUE(packer_->unpack(decompressed_path, final_restore_dir));

    // 7. 验证恢复的文件
    EXPECT_TRUE(fs::exists(final_restore_dir / "file1.txt"));
    EXPECT_TRUE(fs::exists(final_restore_dir / "subdir/file2.txt"));
    EXPECT_TRUE(fs::exists(final_restore_dir / "large_file.dat"));

    // 验证文件内容
    EXPECT_TRUE(compareFiles(file1, final_restore_dir / "file1.txt"));
    EXPECT_TRUE(compareFiles(file2, final_restore_dir / "subdir/file2.txt"));
    EXPECT_TRUE(compareFiles(file3, final_restore_dir / "large_file.dat"));
}

// 测试错误处理和异常情况
TEST_F(IntegrationTest, ErrorHandlingFlow) {
    // 1. 创建测试文件
    auto source_file = createTestFile("source.txt", "Test content");
    auto pack_path = test_dir_ / "backup.pack";
    auto compressed_path = test_dir_ / "backup.pack.gz";
    auto encrypted_path = test_dir_ / "backup.pack.gz.enc";
    std::string password = "test_password123";

    // 2. 测试文件不存在的情况
    EXPECT_FALSE(packer_->pack({test_dir_ / "nonexistent.txt"}, pack_path));

    // 3. 测试空文件的处理
    auto empty_file = createTestFile("empty.txt", "");
    EXPECT_TRUE(packer_->pack({empty_file}, pack_path));
    EXPECT_TRUE(compressor_->compress(pack_path, compressed_path));
    EXPECT_TRUE(encryptor_->encrypt(compressed_path, encrypted_path, password));

    // 4. 测试错误的密码
    auto decrypted_path = test_dir_ / "decrypted.gz";
    EXPECT_FALSE(encryptor_->decrypt(encrypted_path, decrypted_path, "wrong_password"));

    // 5. 测试损坏的文件
    {
        // 损坏加密文件
        std::ofstream file(encrypted_path, std::ios::binary | std::ios::app);
        file.write("corrupt", 7);
    }
    EXPECT_FALSE(encryptor_->decrypt(encrypted_path, decrypted_path, password));
}

// 测试并发操作
TEST_F(IntegrationTest, ConcurrentOperations) {
    // 创建多个测试文件
    std::vector<fs::path> test_files;
    for (int i = 0; i < 5; ++i) {
        test_files.push_back(createTestFile(
            "file" + std::to_string(i) + ".txt",
            "Content " + std::to_string(i)
        ));
    }

    // 并发处理多个文件
    std::vector<std::thread> threads;
    std::vector<fs::path> pack_paths;
    std::mutex mtx;

    for (size_t i = 0; i < test_files.size(); ++i) {
        threads.emplace_back([this, i, &test_files, &pack_paths, &mtx]() {
            auto pack_path = test_dir_ / ("backup" + std::to_string(i) + ".pack");
            auto compressed_path = pack_path.string() + ".gz";
            auto encrypted_path = compressed_path + ".enc";
            std::string password = "password" + std::to_string(i);

            EXPECT_TRUE(packer_->pack({test_files[i]}, pack_path));
            EXPECT_TRUE(compressor_->compress(pack_path, compressed_path));
            EXPECT_TRUE(encryptor_->encrypt(compressed_path, encrypted_path, password));

            {
                std::lock_guard<std::mutex> lock(mtx);
                pack_paths.push_back(encrypted_path);
            }
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    // 验证所有文件都被正确处理
    EXPECT_EQ(pack_paths.size(), test_files.size());
    for (const auto& path : pack_paths) {
        EXPECT_TRUE(fs::exists(path));
        EXPECT_GT(fs::file_size(path), 0);
    }
} 