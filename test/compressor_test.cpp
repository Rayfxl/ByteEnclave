#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "compressor.hpp"
#include <filesystem>
#include <fstream>
#include <random>

namespace fs = std::filesystem;
using namespace byte_enclave;

class CompressorTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "byte_enclave_test";
        fs::create_directories(test_dir_);
        compressor_ = std::make_unique<Compressor>();
    }

    void TearDown() override {
        fs::remove_all(test_dir_);
    }

    // 创建测试文件
    fs::path createTestFile(const std::string& name, const std::string& content) {
        fs::path file_path = test_dir_ / name;
        std::ofstream file(file_path, std::ios::binary);
        file.write(content.c_str(), content.size());
        return file_path;
    }

    // 读取文件内容
    std::string readFile(const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        return std::string(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        );
    }

    // 验证压缩和解压缩结果
    void verifyCompression(const fs::path& input_path) {
        auto compressed_path = test_dir_ / "compressed.bin";
        auto decompressed_path = test_dir_ / "decompressed.txt";

        EXPECT_TRUE(compressor_->compress(input_path.string(), compressed_path.string()));
        EXPECT_TRUE(compressor_->decompress(compressed_path.string(), decompressed_path.string()));
        EXPECT_EQ(readFile(input_path), readFile(decompressed_path));
    }

    fs::path test_dir_;
    std::unique_ptr<Compressor> compressor_;
};

// 测试空文件
TEST_F(CompressorTest, EmptyFile) {
    auto input_path = createTestFile("empty.txt", "");
    verifyCompression(input_path);
}

// 测试基本压缩
TEST_F(CompressorTest, BasicCompression) {
    const std::string input = "Hello, World!";
    auto input_path = createTestFile("input.txt", input);
    verifyCompression(input_path);
}

// 测试重复模式
TEST_F(CompressorTest, RepeatingPattern) {
    const std::string input = "abcabcabcabc";
    auto input_path = createTestFile("input.txt", input);
    verifyCompression(input_path);
}

// 测试长重复序列
TEST_F(CompressorTest, LongRepeatingSequence) {
    std::string input;
    for (int i = 0; i < 100; ++i) {
        input += "ABCDEF";
    }
    auto input_path = createTestFile("input.txt", input);
    verifyCompression(input_path);
}

// 测试随机数据
TEST_F(CompressorTest, RandomData) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::string input;
    input.reserve(1024);  // 1KB的随机数据
    for (int i = 0; i < 1024; ++i) {
        input.push_back(static_cast<char>(dis(gen)));
    }
    
    auto input_path = createTestFile("random.bin", input);
    verifyCompression(input_path);
} 