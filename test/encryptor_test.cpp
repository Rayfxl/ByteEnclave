#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "encryptor.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <random>

namespace fs = std::filesystem;
using namespace byte_enclave;

class EncryptorTest : public ::testing::Test {
protected:
    void SetUp() override {
        encryptor_ = std::make_unique<Encryptor>();
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

    // 生成随机密码
    std::string generateRandomPassword(size_t length = 16) {
        const std::string charset = 
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "!@#$%^&*()";
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, charset.size() - 1);
        
        std::string password;
        password.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            password += charset[dis(gen)];
        }
        return password;
    }

    std::unique_ptr<Encryptor> encryptor_;
};

// 测试初始化
TEST_F(EncryptorTest, Initialization) {
    std::string password = "test_password";
    EXPECT_TRUE(encryptor_->initialize(password));
    
    // 测试空密码
    EXPECT_FALSE(encryptor_->initialize(""));
}

// 测试基本加密解密
TEST_F(EncryptorTest, BasicEncryptDecrypt) {
    std::string password = "test_password";
    ASSERT_TRUE(encryptor_->initialize(password));
    
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    auto encrypted = encryptor_->encrypt(data);
    auto decrypted = encryptor_->decrypt(encrypted);
    
    EXPECT_EQ(data, decrypted);
    EXPECT_NE(data, encrypted); // 加密数据应该与原始数据不同
}

// 测试空数据
TEST_F(EncryptorTest, EmptyData) {
    ASSERT_TRUE(encryptor_->initialize("password"));
    
    std::vector<uint8_t> empty_data;
    auto encrypted = encryptor_->encrypt(empty_data);
    auto decrypted = encryptor_->decrypt(encrypted);
    
    EXPECT_TRUE(decrypted.empty());
}

// 测试大数据
TEST_F(EncryptorTest, LargeData) {
    ASSERT_TRUE(encryptor_->initialize("password"));
    
    auto data = generateRandomData(1024 * 1024); // 1MB
    auto encrypted = encryptor_->encrypt(data);
    auto decrypted = encryptor_->decrypt(encrypted);
    
    EXPECT_EQ(data, decrypted);
}

// 测试IV生成和设置
TEST_F(EncryptorTest, IVHandling) {
    ASSERT_TRUE(encryptor_->initialize("password"));
    
    // 生成IV
    auto iv = encryptor_->generateIV();
    EXPECT_EQ(iv.size(), 16); // AES块大小
    
    // 设置IV并加密解密
    encryptor_->setIV(iv);
    std::vector<uint8_t> data = {'T', 'e', 's', 't'};
    auto encrypted = encryptor_->encrypt(data);
    auto decrypted = encryptor_->decrypt(encrypted);
    
    EXPECT_EQ(data, decrypted);
}

// 测试不同密码
TEST_F(EncryptorTest, DifferentPasswords) {
    std::string password1 = generateRandomPassword();
    std::string password2 = generateRandomPassword();
    
    // 使用密码1加密
    ASSERT_TRUE(encryptor_->initialize(password1));
    std::vector<uint8_t> data = {'S', 'e', 'c', 'r', 'e', 't'};
    auto encrypted = encryptor_->encrypt(data);
    
    // 使用密码2尝试解密，应该抛出异常
    ASSERT_TRUE(encryptor_->initialize(password2));
    EXPECT_THROW(encryptor_->decrypt(encrypted), std::runtime_error);
}

// 测试多次加密
TEST_F(EncryptorTest, MultipleEncryption) {
    ASSERT_TRUE(encryptor_->initialize("password"));
    
    std::vector<uint8_t> data = {'M', 'u', 'l', 't', 'i'};
    std::vector<std::vector<uint8_t>> encrypted_results;
    
    // 多次加密同样的数据，结果应该不同（因为IV不同）
    for (int i = 0; i < 5; ++i) {
        encrypted_results.push_back(encryptor_->encrypt(data));
    }
    
    // 验证每次加密结果都不相同
    for (size_t i = 0; i < encrypted_results.size(); ++i) {
        for (size_t j = i + 1; j < encrypted_results.size(); ++j) {
            EXPECT_NE(encrypted_results[i], encrypted_results[j]);
        }
    }
    
    // 但是所有结果都能正确解密
    for (const auto& encrypted : encrypted_results) {
        auto decrypted = encryptor_->decrypt(encrypted);
        EXPECT_EQ(data, decrypted);
    }
}

// 测试错误处理
TEST_F(EncryptorTest, ErrorHandling) {
    ASSERT_TRUE(encryptor_->initialize("password"));
    
    // 测试解密损坏的数据
    std::vector<uint8_t> data = {'T', 'e', 's', 't'};
    auto encrypted = encryptor_->encrypt(data);
    encrypted[encrypted.size() / 2] ^= 0xFF; // 损坏数据
    
    EXPECT_THROW(encryptor_->decrypt(encrypted), std::runtime_error);
    
    // 测试无效的IV
    std::vector<uint8_t> invalid_iv(8, 0); // 错误的IV大小
    EXPECT_THROW(encryptor_->setIV(invalid_iv), std::invalid_argument);
}

// 测试密钥派生
TEST_F(EncryptorTest, KeyDerivation) {
    std::string password = "test_password";
    std::vector<uint8_t> data = {'t', 'e', 's', 't'};
    std::vector<uint8_t> fixed_iv(Encryptor::IV_SIZE, 0);
    
    // 第一次初始化和加��
    ASSERT_TRUE(encryptor_->initialize(password));
    auto salt = encryptor_->getSalt();  // 获取生成的盐值
    encryptor_->setIV(fixed_iv);
    auto encrypted1 = encryptor_->encrypt(data);
    
    // 重新初始化并使用相同的IV和盐值
    encryptor_ = std::make_unique<Encryptor>();
    ASSERT_TRUE(encryptor_->initialize(password, salt));  // 使用相同的盐值
    encryptor_->setIV(fixed_iv);
    auto encrypted2 = encryptor_->encrypt(data);
    
    // 使用相同的密码、盐值和IV时，加密结果应该相同
    EXPECT_EQ(encrypted1, encrypted2);
    
    // 验证两次加密的结果都能正确解密
    auto decrypted1 = encryptor_->decrypt(encrypted1);
    auto decrypted2 = encryptor_->decrypt(encrypted2);
    EXPECT_EQ(data, decrypted1);
    EXPECT_EQ(data, decrypted2);
} 