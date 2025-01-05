#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace byte_enclave {

// LZ77匹配结构
struct LZ77Match {
    size_t distance;  // 距离
    size_t length;    // 长度
    uint8_t next_char; // 下一个字符

    LZ77Match(size_t d = 0, size_t l = 0, uint8_t c = 0)
        : distance(d), length(l), next_char(c) {}
};

class Compressor {
public:
    // 压缩文件
    bool compress(const std::string& input_path, const std::string& output_path);
    
    // 解压缩文件
    bool decompress(const std::string& input_path, const std::string& output_path);

private:
    // LZ77算法参数
    static constexpr size_t WINDOW_SIZE = 4096;      // 滑动窗口大小
    static constexpr size_t LOOKAHEAD_SIZE = 16;     // 前向缓冲区大小
    static constexpr size_t MIN_MATCH_LENGTH = 3;    // 最小匹配长度

    // LZ77压缩和解压缩
    std::vector<LZ77Match> lz77_compress(const std::vector<uint8_t>& data);
    std::vector<uint8_t> lz77_decompress(const std::vector<LZ77Match>& matches);
};

} // namespace byte_enclave 