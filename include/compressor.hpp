#ifndef BYTE_ENCLAVE_COMPRESSOR_HPP
#define BYTE_ENCLAVE_COMPRESSOR_HPP

#include <vector>
#include <cstdint>
#include <cstddef>

namespace byte_enclave {

class Compressor {
public:
    Compressor();

    /**
     * @brief 压缩数据
     * @param data 要压缩的数据
     * @return 压缩后的数据
     * @throw std::runtime_error 如果压缩失败
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data);

    /**
     * @brief 解压数据
     * @param compressed_data 要解压的数据
     * @return 解压后的数据
     * @throw std::runtime_error 如果解压失败或数据无效
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed_data);

    /**
     * @brief 获取压缩比率
     * @param original_size 原始数据大小
     * @param compressed_size 压缩后数据大小
     * @return 压缩比率（原始大小/压缩后大小）
     */
    double getCompressionRatio(size_t original_size, size_t compressed_size) const;
};

} // namespace byte_enclave

#endif // BYTE_ENCLAVE_COMPRESSOR_HPP 