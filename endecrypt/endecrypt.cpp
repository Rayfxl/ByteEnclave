#include <cstring>
#include <iostream>
#include <openssl/aes.h>
#include <openssl/md5.h>

// AES加密
void aes_encrypt(const unsigned char *plaintext, unsigned char *ciphertext, const unsigned char *key)
{
    AES_KEY encryptKey;
    AES_set_encrypt_key(key, 128, &encryptKey);
    AES_encrypt(plaintext, ciphertext, &encryptKey);
}

// AES解密
void aes_decrypt(const unsigned char *ciphertext, unsigned char *plaintext, const unsigned char *key)
{
    AES_KEY decryptKey;
    AES_set_decrypt_key(key, 128, &decryptKey);
    AES_decrypt(ciphertext, plaintext, &decryptKey);
}

// 计算MD5散列值
void compute_md5(const unsigned char *data, size_t length, unsigned char *md5_hash)
{
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    MD5_Update(&md5Context, data, length);
    MD5_Final(md5_hash, &md5Context);
}

int main()
{
    // 示例数据
    const unsigned char *plaintext = (unsigned char *)"Hello, World!";
    unsigned char key[16] = "0123456789abcdef"; // 16字节密钥
    unsigned char ciphertext[16];
    unsigned char decryptedtext[16];
    unsigned char md5_hash[MD5_DIGEST_LENGTH];

    // AES加密
    aes_encrypt(plaintext, ciphertext, key);
    std::cout << "AES Encrypted text: ";
    for (int i = 0; i < 16; ++i)
    {
        printf("%02x", ciphertext[i]);
    }
    std::cout << std::endl;

    // AES解密
    aes_decrypt(ciphertext, decryptedtext, key);
    std::cout << "AES Decrypted text: " << decryptedtext << std::endl;

    // 计算MD5散列值
    compute_md5(plaintext, strlen((const char *)plaintext), md5_hash);
    std::cout << "MD5 Hash: ";
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        printf("%02x", md5_hash[i]);
    }
    std::cout << std::endl;

    return 0;
}