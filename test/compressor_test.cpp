#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "compressor.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <random>

namespace fs = std::filesystem;
using namespace byte_enclave;

class CompressorTest : public ::testing::Test {
protected:
    void SetUp() override {
        compressor_ = std::make_unique<Compressor>();
    }

    // 生成重复模式的测试数据
    std::vector<uint8_t> generateRepeatingData(size_t size, size_t pattern_length = 64) {
        std::vector<uint8_t> data(size);
        std::vector<uint8_t> pattern(pattern_length);
        
        // 生成随机模式
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (auto& byte : pattern) {
            byte = static_cast<uint8_t>(dis(gen));
        }
        
        // 重复模式填充数据
        for (size_t i = 0; i < size; i += pattern_length) {
            size_t copy_size = std::min(pattern_length, size - i);
            std::copy_n(pattern.begin(), copy_size, data.begin() + i);
        }
        
        return data;
    }

    // 生成随机数据
    std::vector<uint8_t> generateRandomData(size_t size) {
        std::vector<uint8_t> data(size);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (auto& byte : data) {
            byte = static_cast<uint8_t>(dis(gen));
        }
        return data;
    }

    std::unique_ptr<Compressor> compressor_;
};

// 测试空数据压缩
TEST_F(CompressorTest, CompressEmptyData) {
    std::vector<uint8_t> empty_data;
    auto compressed = compressor_->compress(empty_data);
    auto decompressed = compressor_->decompress(compressed);
    
    EXPECT_TRUE(compressed.empty());
    EXPECT_TRUE(decompressed.empty());
}

// 测试小数据压缩
TEST_F(CompressorTest, CompressSmallData) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto compressed = compressor_->compress(data);
    auto decompressed = compressor_->decompress(compressed);
    
    EXPECT_EQ(data, decompressed);
}

// 测试重复数据压缩
TEST_F(CompressorTest, CompressRepeatingData) {
    auto data = generateRepeatingData(1024 * 1024); // 1MB的重复数据
    auto compressed = compressor_->compress(data);
    auto decompressed = compressor_->decompress(compressed);
    
    EXPECT_EQ(data, decompressed);
    
    // 检查压缩率，重复数据应该有较高的压缩率
    double ratio = compressor_->getCompressionRatio(data.size(), compressed.size());
    EXPECT_GT(ratio, 2.0); // 压缩率应该大于2
}

// 测试随机数据压缩
TEST_F(CompressorTest, CompressRandomData) {
    auto data = generateRandomData(1024 * 1024); // 1MB的随机数据
    auto compressed = compressor_->compress(data);
    auto decompressed = compressor_->decompress(compressed);
    
    EXPECT_EQ(data, decompressed);
    
    // 随机数据压缩率应该较低
    double ratio = compressor_->getCompressionRatio(data.size(), compressed.size());
    EXPECT_LT(ratio, 1.2); // 压缩率应该小于1.2
}

// 测试大数据压缩
TEST_F(CompressorTest, CompressLargeData) {
    auto data = generateRepeatingData(10 * 1024 * 1024); // 10MB
    auto compressed = compressor_->compress(data);
    auto decompressed = compressor_->decompress(compressed);
    
    EXPECT_EQ(data, decompressed);
}

// 测试混合数据压缩
TEST_F(CompressorTest, CompressMixedData) {
    // 创建一个包含重复和随机数据的混合数据集
    std::vector<uint8_t> data;
    auto repeating = generateRepeatingData(512 * 1024); // 512KB重复数据
    auto random = generateRandomData(512 * 1024);       // 512KB随机数据
    
    data.insert(data.end(), repeating.begin(), repeating.end());
    data.insert(data.end(), random.begin(), random.end());
    
    auto compressed = compressor_->compress(data);
    auto decompressed = compressor_->decompress(compressed);
    
    EXPECT_EQ(data, decompressed);
}

// 测试压缩-解压多次
TEST_F(CompressorTest, MultipleCompressionCycles) {
    auto original_data = generateRepeatingData(1024 * 1024);
    
    for (int i = 0; i < 5; ++i) {
        auto compressed = compressor_->compress(original_data);
        auto decompressed = compressor_->decompress(compressed);
        EXPECT_EQ(original_data, decompressed);
    }
}

// 测试压缩率计算
TEST_F(CompressorTest, CompressionRatio) {
    // 测试完全重复的数据
    std::vector<uint8_t> repeating_data(1024, 'A');
    auto compressed_repeating = compressor_->compress(repeating_data);
    double ratio_repeating = compressor_->getCompressionRatio(
        repeating_data.size(), compressed_repeating.size());
    
    // 测试随机数据
    auto random_data = generateRandomData(1024);
    auto compressed_random = compressor_->compress(random_data);
    double ratio_random = compressor_->getCompressionRatio(
        random_data.size(), compressed_random.size());
    
    // 重复数据的压缩率应该明显高于随机数据
    EXPECT_GT(ratio_repeating, ratio_random);
}

// 测试错误处理
TEST_F(CompressorTest, ErrorHandling) {
    // 测试解压损坏的数据
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto compressed = compressor_->compress(data);
    
    // 损坏压缩数据
    if (!compressed.empty()) {
        compressed[compressed.size() / 2] ^= 0xFF;
    }
    
    // 解压损坏的数据应该抛出异常
    EXPECT_THROW(compressor_->decompress(compressed), std::runtime_error);
}

// 测试边界情况
TEST_F(CompressorTest, EdgeCases) {
    // 测试单字节数据
    std::vector<uint8_t> single_byte = {42};
    auto compressed_single = compressor_->compress(single_byte);
    auto decompressed_single = compressor_->decompress(compressed_single);
    EXPECT_EQ(single_byte, decompressed_single);
    
    // 测试全零数据
    std::vector<uint8_t> zeros(1024, 0);
    auto compressed_zeros = compressor_->compress(zeros);
    auto decompressed_zeros = compressor_->decompress(compressed_zeros);
    EXPECT_EQ(zeros, decompressed_zeros);
    
    // 测试全相同字节数据
    std::vector<uint8_t> same_bytes(1024, 0xFF);
    auto compressed_same = compressor_->compress(same_bytes);
    auto decompressed_same = compressor_->decompress(compressed_same);
    EXPECT_EQ(same_bytes, decompressed_same);
} 