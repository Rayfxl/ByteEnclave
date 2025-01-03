#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <memory>

namespace byte_enclave {

struct PackageHeader {
    uint32_t magic;          // 魔数，用于识别文件格式
    uint32_t version;        // 版本号
    uint64_t file_count;     // 文件数量
    uint64_t total_size;     // 总大小
    uint32_t checksum;       // 包头校验和
};

struct FileHeader {
    std::string path;        // 相对路径
    uint64_t size;          // 文件大小
    uint64_t offset;        // 在包中的偏移
    uint32_t checksum;      // 校验和
};

class Packer {
public:
    Packer();
    ~Packer() = default;

    // 打包文件
    bool pack(const std::vector<std::filesystem::path>& files,
             const std::filesystem::path& output_path);

    // 解包文件
    bool unpack(const std::filesystem::path& package_path,
               const std::filesystem::path& output_dir);

    // 列出包内容
    std::vector<FileHeader> listContents(const std::filesystem::path& package_path);

    // 提取单个文件
    bool extractFile(const std::filesystem::path& package_path,
                    const std::string& file_path,
                    const std::filesystem::path& output_path);

    // 验证包的校验和
    bool verifyChecksum(const std::filesystem::path& package_path);

private:
    // 写入包头
    bool writeHeader(std::ofstream& out, const PackageHeader& header);
    
    // 读取包头
    bool readHeader(std::ifstream& in, PackageHeader& header);
    
    // 计算校验和
    uint32_t calculateChecksum(const std::vector<uint8_t>& data);
};

} // namespace byte_enclave 