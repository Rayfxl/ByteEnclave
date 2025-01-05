#include "compressor.hpp"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <iostream>

namespace byte_enclave {

bool Compressor::compress(const std::string& input_path, const std::string& output_path) {
    try {
        std::cout << "[COMPRESS] 开始压缩文件: " << input_path << std::endl;
        
        // 读取输入文件
        std::ifstream input(input_path, std::ios::binary);
        if (!input) {
            std::cout << "[COMPRESS] 无法打开输入文件" << std::endl;
            return false;
        }

        std::vector<uint8_t> data(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>()
        );
        input.close();

        std::cout << "[COMPRESS] 读取输入文件大小: " << data.size() << " bytes" << std::endl;

        // 空文件特殊处理
        if (data.empty()) {
            std::cout << "[COMPRESS] 空文件，直接创建空输出" << std::endl;
            std::ofstream output(output_path, std::ios::binary);
            return output.good();
        }

        // 使用LZ77压缩
        std::cout << "[COMPRESS] 开始LZ77压缩" << std::endl;
        auto matches = lz77_compress(data);
        std::cout << "[COMPRESS] LZ77压缩完成，匹配数量: " << matches.size() << std::endl;

        // 写入压缩文件
        std::ofstream output(output_path, std::ios::binary);
        if (!output) {
            std::cout << "[COMPRESS] 无法创建输出文件" << std::endl;
            return false;
        }

        // 写入原始数据大小
        uint32_t original_size = static_cast<uint32_t>(data.size());
        output.write(reinterpret_cast<const char*>(&original_size), sizeof(original_size));

        // 写入匹配序列
        for (const auto& match : matches) {
            uint16_t distance = static_cast<uint16_t>(match.distance);
            uint16_t length = static_cast<uint16_t>(match.length);
            output.write(reinterpret_cast<const char*>(&distance), sizeof(distance));
            output.write(reinterpret_cast<const char*>(&length), sizeof(length));
            output.write(reinterpret_cast<const char*>(&match.next_char), sizeof(match.next_char));
        }

        std::cout << "[COMPRESS] 压缩完成，输出文件大小: " << output.tellp() << " bytes" << std::endl;
        return output.good();
    } catch (const std::exception& e) {
        std::cout << "[COMPRESS] 异常: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cout << "[COMPRESS] 未知异常" << std::endl;
        return false;
    }
}

bool Compressor::decompress(const std::string& input_path, const std::string& output_path) {
    try {
        std::cout << "[DECOMPRESS] 开始解压文件: " << input_path << std::endl;
        
        // 读取压缩文件
        std::ifstream input(input_path, std::ios::binary);
        if (!input) {
            std::cout << "[DECOMPRESS] 无法打开输入文件" << std::endl;
            return false;
        }

        // 处理空文件
        if (input.peek() == std::ifstream::traits_type::eof()) {
            std::cout << "[DECOMPRESS] 空文件，直接创建空输出" << std::endl;
            std::ofstream output(output_path, std::ios::binary);
            return output.good();
        }

        // 读取原始数据大小
        uint32_t original_size;
        if (!input.read(reinterpret_cast<char*>(&original_size), sizeof(original_size))) {
            std::cout << "[DECOMPRESS] 读取原始大小失败" << std::endl;
            return false;
        }
        std::cout << "[DECOMPRESS] 原始文件大小: " << original_size << " bytes" << std::endl;

        // 读取匹配序列
        std::vector<LZ77Match> matches;
        while (input && !input.eof()) {
            uint16_t distance, length;
            uint8_t next_char;
            
            if (input.read(reinterpret_cast<char*>(&distance), sizeof(distance)) &&
                input.read(reinterpret_cast<char*>(&length), sizeof(length)) &&
                input.read(reinterpret_cast<char*>(&next_char), sizeof(next_char))) {
                matches.emplace_back(distance, length, next_char);
            }
        }
        input.close();
        std::cout << "[DECOMPRESS] 读取到 " << matches.size() << " 个匹配" << std::endl;

        // LZ77解压缩
        std::cout << "[DECOMPRESS] 开始LZ77解压缩" << std::endl;
        auto decompressed = lz77_decompress(matches);
        std::cout << "[DECOMPRESS] 解压后大小: " << decompressed.size() << " bytes" << std::endl;

        // 验证解压后的大小
        if (decompressed.size() != original_size) {
            std::cout << "[DECOMPRESS] 解压后大小不匹配，期望: " << original_size 
                     << ", 实际: " << decompressed.size() << std::endl;
            return false;
        }

        // 写入解压缩文件
        std::ofstream output(output_path, std::ios::binary);
        if (!output) {
            std::cout << "[DECOMPRESS] 无法创建输出文件" << std::endl;
            return false;
        }

        output.write(reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
        std::cout << "[DECOMPRESS] 解压完成" << std::endl;
        return output.good();
    } catch (const std::exception& e) {
        std::cout << "[DECOMPRESS] 异常: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cout << "[DECOMPRESS] 未知异常" << std::endl;
        return false;
    }
}

std::vector<LZ77Match> Compressor::lz77_compress(const std::vector<uint8_t>& data) {
    std::vector<LZ77Match> matches;
    if (data.empty()) {
        return matches;
    }

    size_t pos = 0;
    while (pos < data.size()) {
        size_t best_length = 0;
        size_t best_distance = 0;

        // 在滑动窗口中查找最长匹配
        size_t window_start = (pos > WINDOW_SIZE) ? pos - WINDOW_SIZE : 0;
        for (size_t i = window_start; i < pos; i++) {
            size_t length = 0;
            size_t max_length = std::min(LOOKAHEAD_SIZE, data.size() - pos);
            
            // 计算匹配长度
            while (length < max_length && data[i + length] == data[pos + length]) {
                length++;
            }

            // 更新最佳匹配
            if (length >= MIN_MATCH_LENGTH && length > best_length) {
                best_length = length;
                best_distance = pos - i;
            }
        }

        // 添加匹配结果
        if (best_length >= MIN_MATCH_LENGTH) {
            uint8_t next_char = (pos + best_length < data.size()) ? data[pos + best_length] : 0;
            matches.emplace_back(best_distance, best_length, next_char);
            pos += best_length;
            if (next_char != 0) {
                pos++;
            }
        } else {
            matches.emplace_back(0, 0, data[pos]);
            pos++;
        }
    }

    return matches;
}

std::vector<uint8_t> Compressor::lz77_decompress(const std::vector<LZ77Match>& matches) {
    std::vector<uint8_t> result;

    for (const auto& match : matches) {
        if (match.length == 0) {
            // 直接字符
            result.push_back(match.next_char);
        } else {
            // 复制之前的匹配
            size_t start = result.size() - match.distance;
            for (size_t i = 0; i < match.length; i++) {
                result.push_back(result[start + i]);
            }
            if (match.next_char != 0) {
                result.push_back(match.next_char);
            }
        }
    }

    return result;
}

} // namespace byte_enclave 