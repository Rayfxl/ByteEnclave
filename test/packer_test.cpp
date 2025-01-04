#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "packer.hpp"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstring>

namespace fs = std::filesystem;
using namespace byte_enclave;

class PackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试目录
        test_dir_ = fs::temp_directory_path() / "byte_enclave_test";
        fs::create_directories(test_dir_);
        packer_ = std::make_unique<Packer>();
    }

    void TearDown() override {
        // 清理测试目录
        fs::remove_all(test_dir_);
    }

    // 创建测试文件
    fs::path createTestFile(const std::string& name, const std::string& content = "test") {
        fs::path file_path = test_dir_ / name;
        fs::create_directories(file_path.parent_path());
        std::ofstream file(file_path, std::ios::binary);
        file << content;
        file.close();
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
    std::unique_ptr<Packer> packer_;
};

// 测试打包单个文件
TEST_F(PackerTest, PackSingleFile) {
    auto src_path = createTestFile("source.txt", "test content");
    auto pack_path = test_dir_ / "test.pack";
    
    EXPECT_TRUE(packer_->pack({src_path}, pack_path));
    EXPECT_TRUE(fs::exists(pack_path));
    EXPECT_GT(fs::file_size(pack_path), 0);
}

// 测试打包多个文件
TEST_F(PackerTest, PackMultipleFiles) {
    auto file1 = createTestFile("file1.txt", "content1");
    auto file2 = createTestFile("file2.txt", "content2");
    auto pack_path = test_dir_ / "test.pack";
    
    EXPECT_TRUE(packer_->pack({file1, file2}, pack_path));
    EXPECT_TRUE(fs::exists(pack_path));
    
    auto extract_dir = test_dir_ / "extract";
    EXPECT_TRUE(packer_->unpack(pack_path, extract_dir));
    
    EXPECT_TRUE(fs::exists(extract_dir / "file1.txt"));
    EXPECT_TRUE(fs::exists(extract_dir / "file2.txt"));
    EXPECT_TRUE(compareFiles(file1, extract_dir / "file1.txt"));
    EXPECT_TRUE(compareFiles(file2, extract_dir / "file2.txt"));
}

// 测试打包目录结构
TEST_F(PackerTest, PackDirectoryStructure) {
    auto file1 = createTestFile("file1.txt", "content1");
    auto file2 = createTestFile("subdir/file2.txt", "content2");
    auto pack_path = test_dir_ / "test.pack";
    auto extract_dir = test_dir_ / "extract";

    std::vector<fs::path> files_to_pack = {file1, file2};
    EXPECT_TRUE(packer_->pack(files_to_pack, pack_path));
    EXPECT_TRUE(packer_->unpack(pack_path, extract_dir));

    EXPECT_TRUE(fs::exists(extract_dir / "file1.txt"));
    EXPECT_TRUE(fs::exists(extract_dir / "subdir/file2.txt"));
    EXPECT_TRUE(compareFiles(file1, extract_dir / "file1.txt"));
    EXPECT_TRUE(compareFiles(file2, extract_dir / "subdir/file2.txt"));
}

// 测试提取单个文件
TEST_F(PackerTest, ExtractSingleFile) {
    auto src_path = createTestFile("source.txt", "test content");
    auto pack_path = test_dir_ / "test.pack";
    auto extract_path = test_dir_ / "extracted.txt";
    
    EXPECT_TRUE(packer_->pack({src_path}, pack_path));
    EXPECT_TRUE(packer_->extractFile(pack_path, "source.txt", extract_path));
    EXPECT_TRUE(compareFiles(src_path, extract_path));
}

// 测试校验和验证
TEST_F(PackerTest, ChecksumVerification) {
    auto src_path = createTestFile("source.txt", "test content");
    auto pack_path = test_dir_ / "test.pack";
    
    EXPECT_TRUE(packer_->pack({src_path}, pack_path));
    EXPECT_TRUE(packer_->verifyChecksum(pack_path));
    
    // 破坏包文件
    {
        std::ofstream file(pack_path, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(-1, std::ios::end);  // 定位到倒数第二个字节
        char corrupt = 0xFF;
        file.write(&corrupt, 1);  // 写入一个错误的字节
    }
    
    EXPECT_FALSE(packer_->verifyChecksum(pack_path));
}

// 测试错误处理
TEST_F(PackerTest, ErrorHandling) {
    // 测试不存在的文件
    auto non_existent = test_dir_ / "non_existent.txt";
    auto pack_path = test_dir_ / "test.pack";
    
    EXPECT_FALSE(packer_->pack({non_existent}, pack_path));
    
    // 测试无效的包文件
    auto extract_dir = test_dir_ / "extract";
    EXPECT_FALSE(packer_->unpack(non_existent, extract_dir));
    
    // 测试提取不存在的文件
    auto src_path = createTestFile("source.txt", "test content");
    EXPECT_TRUE(packer_->pack({src_path}, pack_path));
    auto extract_path = test_dir_ / "extracted.txt";
    EXPECT_FALSE(packer_->extractFile(pack_path, "non_existent.txt", extract_path));
} 