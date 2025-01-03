#include "compressor.hpp"
#include <fstream>
#include <cstring>
#include <filesystem>
#include <zlib.h>

namespace fs = std::filesystem;

namespace byte_enclave {

Compressor::Compressor() = default;

std::vector<uint8_t> Compressor::compress(const std::vector<uint8_t>& data) {
    if (data.empty()) return {};

    // 预分配压缩缓冲区
    uLong compressed_size = compressBound(data.size());
    std::vector<uint8_t> compressed(compressed_size);

    // 压缩数据
    int result = ::compress2(
        compressed.data(),
        &compressed_size,
        data.data(),
        data.size(),
        Z_BEST_COMPRESSION
    );

    if (result != Z_OK) {
        throw std::runtime_error("Compression failed");
    }

    // 调整缓冲区大小为实际压缩后的大小
    compressed.resize(compressed_size);
    return compressed;
}

std::vector<uint8_t> Compressor::decompress(const std::vector<uint8_t>& compressed_data) {
    if (compressed_data.empty()) return {};

    // 预分配解压缓冲区（假设压缩比为2）
    uLong decompressed_size = compressed_data.size() * 2;
    std::vector<uint8_t> decompressed(decompressed_size);

    while (true) {
        uLong current_size = decompressed_size;
        int result = ::uncompress(
            decompressed.data(),
            &current_size,
            compressed_data.data(),
            compressed_data.size()
        );

        if (result == Z_OK) {
            // 调整缓冲区大小为实际解压后的大小
            decompressed.resize(current_size);
            return decompressed;
        } else if (result == Z_BUF_ERROR) {
            // 缓冲区太小，增加大小重试
            decompressed_size *= 2;
            decompressed.resize(decompressed_size);
        } else {
            throw std::runtime_error("Decompression failed");
        }
    }
}

double Compressor::getCompressionRatio(size_t original_size, size_t compressed_size) const {
    if (compressed_size == 0) return 0.0;
    return static_cast<double>(original_size) / compressed_size;
}

} // namespace byte_enclave 