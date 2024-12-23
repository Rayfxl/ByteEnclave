#include "encryptor.hpp"
#include <fstream>
#include <cstring>
#include <filesystem>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <stdexcept>

namespace fs = std::filesystem;

namespace byte_enclave {

namespace {
    // 获取OpenSSL错误信息
    std::string getOpenSSLError() {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        return err_buf;
    }
}

Encryptor::Encryptor() : ctx_(nullptr), initialized_(false), iv_set_(false) {
    // 初始化OpenSSL
    ctx_ = EVP_CIPHER_CTX_new();
    if (!ctx_) {
        throw std::runtime_error("Failed to create cipher context");
    }
}

Encryptor::~Encryptor() {
    if (ctx_) {
        EVP_CIPHER_CTX_free(ctx_);
    }
}

bool Encryptor::initialize(const std::string& password,
                         const std::vector<uint8_t>& salt) {
    if (password.empty()) return false;
    
    try {
        // 如果提供了盐值,使用提供的盐值;否则生成新的盐值
        if (!salt.empty()) {
            if (salt.size() != SALT_SIZE) {
                throw std::invalid_argument("Invalid salt size");
            }
            salt_ = salt;
        } else {
            salt_.resize(SALT_SIZE);
            if (RAND_bytes(salt_.data(), salt_.size()) != 1) {
                throw std::runtime_error("Failed to generate salt: " + getOpenSSLError());
            }
        }
        
        // 派生密钥
        key_.resize(KEY_SIZE);
        if (!deriveKey(password, key_, salt_)) {
            return false;
        }
        
        // 生成初始化向量
        iv_ = generateIV();
        
        initialized_ = true;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<uint8_t> Encryptor::encrypt(const std::vector<uint8_t>& data) {
    if (!initialized_) {
        throw std::runtime_error("Encryptor not initialized");
    }
    
    // 如果 setIV 被调用过，使用设置的 IV；否则生成新的 IV
    std::vector<uint8_t> iv;
    if (iv_set_) {
        iv = iv_;
        iv_set_ = false;  // 重置标志，下次加密将使用新的 IV
    } else {
        iv = generateIV();
    }
    
    // 重置加密上下文
    if (EVP_EncryptInit_ex(ctx_, EVP_aes_256_gcm(), nullptr, key_.data(), iv.data()) != 1) {
        throw std::runtime_error("Failed to initialize encryption: " + getOpenSSLError());
    }
    
    // 分配输出缓冲区（包括IV、认证标签和加密数据）
    std::vector<uint8_t> encrypted;
    encrypted.reserve(iv.size() + data.size() + 16); // 16是认证标签的大小
    
    // 添加IV
    encrypted.insert(encrypted.end(), iv.begin(), iv.end());
    
    // 加密数据
    std::vector<uint8_t> cipher_text(data.size() + EVP_MAX_BLOCK_LENGTH);
    int out_len;
    
    if (EVP_EncryptUpdate(ctx_, cipher_text.data(), &out_len,
                         data.data(), data.size()) != 1) {
        throw std::runtime_error("Failed to encrypt data: " + getOpenSSLError());
    }
    
    cipher_text.resize(out_len);
    
    // 完成加密
    std::vector<uint8_t> final_block(EVP_MAX_BLOCK_LENGTH);
    int final_len;
    
    if (EVP_EncryptFinal_ex(ctx_, final_block.data(), &final_len) != 1) {
        throw std::runtime_error("Failed to finalize encryption: " + getOpenSSLError());
    }
    
    cipher_text.insert(cipher_text.end(),
                      final_block.begin(),
                      final_block.begin() + final_len);
    
    // 获取认证标签
    std::vector<uint8_t> tag(16);
    if (EVP_CIPHER_CTX_ctrl(ctx_, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
        throw std::runtime_error("Failed to get authentication tag: " + getOpenSSLError());
    }
    
    // 组合最终结果
    encrypted.insert(encrypted.end(), cipher_text.begin(), cipher_text.end());
    encrypted.insert(encrypted.end(), tag.begin(), tag.end());
    
    return encrypted;
}

std::vector<uint8_t> Encryptor::decrypt(const std::vector<uint8_t>& encrypted_data) {
    if (!initialized_) {
        throw std::runtime_error("Encryptor not initialized");
    }
    
    if (encrypted_data.size() < IV_SIZE + 16) { // IV + 认证标签
        throw std::runtime_error("Invalid encrypted data size");
    }
    
    // 提取IV
    std::vector<uint8_t> iv(encrypted_data.begin(),
                           encrypted_data.begin() + IV_SIZE);
    
    // 提取认证标签
    std::vector<uint8_t> tag(encrypted_data.end() - 16, encrypted_data.end());
    
    // 提取加密数据
    std::vector<uint8_t> cipher_text(encrypted_data.begin() + IV_SIZE,
                                   encrypted_data.end() - 16);
    
    // 初始化解密上下文
    if (EVP_DecryptInit_ex(ctx_, EVP_aes_256_gcm(), nullptr,
                          key_.data(), iv.data()) != 1) {
        throw std::runtime_error("Failed to initialize decryption: " + getOpenSSLError());
    }
    
    // 设置认证标签
    if (EVP_CIPHER_CTX_ctrl(ctx_, EVP_CTRL_GCM_SET_TAG, 16, tag.data()) != 1) {
        throw std::runtime_error("Failed to set authentication tag: " + getOpenSSLError());
    }
    
    // 解密数据
    std::vector<uint8_t> decrypted(cipher_text.size());
    int out_len;
    
    if (EVP_DecryptUpdate(ctx_, decrypted.data(), &out_len,
                         cipher_text.data(), cipher_text.size()) != 1) {
        throw std::runtime_error("Failed to decrypt data: " + getOpenSSLError());
    }
    
    decrypted.resize(out_len);
    
    // 完成解密
    std::vector<uint8_t> final_block(EVP_MAX_BLOCK_LENGTH);
    int final_len;
    
    if (EVP_DecryptFinal_ex(ctx_, final_block.data(), &final_len) != 1) {
        throw std::runtime_error("Authentication failed or data corrupted");
    }
    
    decrypted.insert(decrypted.end(),
                    final_block.begin(),
                    final_block.begin() + final_len);
    
    return decrypted;
}

std::vector<uint8_t> Encryptor::generateIV() {
    std::vector<uint8_t> iv(IV_SIZE);
    if (RAND_bytes(iv.data(), iv.size()) != 1) {
        throw std::runtime_error("Failed to generate IV: " + getOpenSSLError());
    }
    return iv;
}

void Encryptor::setIV(const std::vector<uint8_t>& iv) {
    if (iv.size() != IV_SIZE) {
        throw std::invalid_argument("Invalid IV size");
    }
    iv_ = iv;
    iv_set_ = true;  // 设置标志，表示 IV 已经被设置
}

bool Encryptor::deriveKey(const std::string& password,
                         std::vector<uint8_t>& key,
                         std::vector<uint8_t>& salt) {
    return PKCS5_PBKDF2_HMAC(password.c_str(),
                            password.length(),
                            salt.data(),
                            salt.size(),
                            10000, // 迭代次数
                            EVP_sha256(),
                            key.size(),
                            key.data()) == 1;
}

} // namespace byte_enclave 