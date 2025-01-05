#pragma once

#include <vector>
#include <string>
#include <memory>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <filesystem>

namespace byte_enclave {

class Encryptor {
public:
    static constexpr size_t KEY_SIZE = 32;  // AES-256
    static constexpr size_t IV_SIZE = 16;   // AES block size
    static constexpr size_t SALT_SIZE = 32;

    Encryptor();
    ~Encryptor();

    // 使用密码初始化
    bool initialize(const std::string& password,
                   const std::vector<uint8_t>& salt = std::vector<uint8_t>());

    // 加密数据
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data);

    // 解密数据
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& encrypted_data);

    // 加密文件
    bool encrypt(const std::filesystem::path& input_path,
                const std::filesystem::path& output_path,
                const std::string& password);

    // 解密文件
    bool decrypt(const std::filesystem::path& input_path,
                const std::filesystem::path& output_path,
                const std::string& password);

    // 生成随机IV
    std::vector<uint8_t> generateIV();

    // 设置IV
    void setIV(const std::vector<uint8_t>& iv);

    // 获取盐值
    const std::vector<uint8_t>& getSalt() const { return salt_; }

private:
    // 密钥派生
    bool deriveKey(const std::string& password,
                  std::vector<uint8_t>& key,
                  std::vector<uint8_t>& salt);

    // 加密上下文
    EVP_CIPHER_CTX* ctx_;
    std::vector<uint8_t> key_;
    std::vector<uint8_t> iv_;
    std::vector<uint8_t> salt_;
    bool initialized_;
    bool iv_set_;  // 标记 IV 是否已经被设置
};

} // namespace byte_enclave 