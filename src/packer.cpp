#include "packer.hpp"
#include <fstream>
#include <cstring>
#include <filesystem>
#include <zlib.h>

namespace fs = std::filesystem;

namespace byte_enclave {

namespace {
    constexpr uint32_t MAGIC_NUMBER = 0x42454E43; // "BENC"
    constexpr uint32_t VERSION = 1;
    constexpr size_t BUFFER_SIZE = 8192;
    
    // 写入固定大小的数据
    template<typename T>
    bool writeData(std::ofstream& out, const T& data) {
        return out.write(reinterpret_cast<const char*>(&data), sizeof(T)).good();
    }
    
    // 读取固定大小的数据
    template<typename T>
    bool readData(std::ifstream& in, T& data) {
        return in.read(reinterpret_cast<char*>(&data), sizeof(T)).good();
    }
    
    // 写入字符串
    bool writeString(std::ofstream& out, const std::string& str) {
        uint64_t size = str.size();
        if (!writeData(out, size)) return false;
        return out.write(str.data(), size).good();
    }
    
    // 读取字符串
    bool readString(std::ifstream& in, std::string& str) {
        uint64_t size;
        if (!readData(in, size)) return false;
        str.resize(size);
        return in.read(str.data(), size).good();
    }
}

Packer::Packer() = default;

bool Packer::writeHeader(std::ofstream& out, const PackageHeader& header) {
    return writeData(out, header);
}

bool Packer::readHeader(std::ifstream& in, PackageHeader& header) {
    return readData(in, header);
}

uint32_t Packer::calculateChecksum(const std::vector<uint8_t>& data) {
    uint32_t checksum = crc32(0L, Z_NULL, 0);
    return crc32(checksum, data.data(), data.size());
}

bool writeFileHeader(std::ofstream& out, const FileHeader& fh) {
    return writeData(out, fh.type) &&
           writeString(out, fh.path) &&
           writeData(out, fh.size) &&
           writeData(out, fh.offset) &&
           writeData(out, fh.checksum) &&
           (fh.type != FileHeader::Type::Symlink || writeString(out, fh.link_target));
}

bool readFileHeader(std::ifstream& in, FileHeader& fh) {
    return readData(in, fh.type) &&
           readString(in, fh.path) &&
           readData(in, fh.size) &&
           readData(in, fh.offset) &&
           readData(in, fh.checksum) &&
           (fh.type != FileHeader::Type::Symlink || readString(in, fh.link_target));
}

bool Packer::pack(const std::vector<fs::path>& files,
                 const fs::path& output_path) {
    try {
        std::ofstream out(output_path, std::ios::binary);
        if (!out) return false;
        
        // 写入包头
        PackageHeader header;
        header.magic = MAGIC_NUMBER;
        header.version = VERSION;
        header.file_count = files.size();
        header.total_size = 0;
        header.checksum = 0;
        
        if (!writeHeader(out, header)) return false;
        
        // 计算文件头的位置
        size_t headers_start = sizeof(PackageHeader);
        size_t data_start = headers_start;
        
        // 获取基准路径
        fs::path base_path;
        if (!files.empty()) {
            base_path = files[0].parent_path();
            for (const auto& file : files) {
                auto parent = file.parent_path();
                while (!parent.empty() && parent != base_path) {
                    if (parent.string().length() < base_path.string().length()) {
                        base_path = parent;
                        break;
                    }
                    parent = parent.parent_path();
                }
            }
        }
        
        // 计算文件头大小
        for (const auto& file : files) {
            data_start += sizeof(FileHeader::Type);  // 类型
            data_start += sizeof(uint64_t) + fs::relative(file, base_path).string().size();  // 路径
            data_start += sizeof(uint64_t) * 2 + sizeof(uint32_t);  // size, offset, checksum
        }
        
        // 写入文件头
        std::vector<FileHeader> file_headers;
        uint64_t current_offset = data_start;
        
        for (const auto& file : files) {
            FileHeader fh;
            fh.path = fs::relative(file, base_path).string();
            fh.type = FileHeader::Type::Regular;
            fh.size = fs::file_size(file);
            fh.offset = current_offset;
            fh.checksum = 0;  // 稍后更新
            
            if (!writeFileHeader(out, fh)) return false;
            file_headers.push_back(fh);
            
            current_offset += fh.size;
            header.total_size += fh.size;
        }
        
        // 写入文件数据
        uint32_t total_checksum = crc32(0L, Z_NULL, 0);
        std::vector<uint8_t> buffer(BUFFER_SIZE);
        
        for (size_t i = 0; i < files.size(); ++i) {
            const auto& file = files[i];
            auto& fh = file_headers[i];
            
            // 写入文件内容
            std::ifstream in(file, std::ios::binary);
            if (!in) return false;
            
            uint32_t file_checksum = crc32(0L, Z_NULL, 0);
            
            while (in) {
                in.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
                size_t bytes_read = in.gcount();
                if (bytes_read > 0) {
                    file_checksum = crc32(file_checksum, buffer.data(), bytes_read);
                    total_checksum = crc32(total_checksum, buffer.data(), bytes_read);
                    
                    if (!out.write(reinterpret_cast<const char*>(buffer.data()),
                                 bytes_read)) {
                        return false;
                    }
                }
            }
            
            fh.checksum = file_checksum;
        }
        
        // 更新文件头的校验和
        out.seekp(headers_start);
        for (const auto& fh : file_headers) {
            if (!writeFileHeader(out, fh)) return false;
        }
        
        // 更新包头的校验和
        header.checksum = total_checksum;
        out.seekp(0);
        if (!writeHeader(out, header)) return false;
        
        return true;
    } catch (...) {
        return false;
    }
}

bool Packer::unpack(const fs::path& package_path,
                   const fs::path& output_dir) {
    try {
        std::ifstream in(package_path, std::ios::binary);
        if (!in) return false;
        
        // 读取包头
        PackageHeader header;
        if (!readHeader(in, header)) return false;
        
        // 验证魔数和版本
        if (header.magic != MAGIC_NUMBER || header.version != VERSION) {
            return false;
        }
        
        // 读取文件头列表
        std::vector<FileHeader> file_headers;
        for (uint64_t i = 0; i < header.file_count; ++i) {
            FileHeader fh;
            if (!readFileHeader(in, fh)) return false;
            file_headers.push_back(fh);
        }
        
        // 创建输出目录
        fs::create_directories(output_dir);
        
        // 解包文件
        std::vector<uint8_t> buffer(BUFFER_SIZE);
        uint32_t total_checksum = crc32(0L, Z_NULL, 0);
        
        for (const auto& fh : file_headers) {
            auto output_path = output_dir / fh.path;
            fs::create_directories(output_path.parent_path());
            
            // 定位到文件数据
            in.seekg(fh.offset);
            
            // 创建输出文件
            std::ofstream out(output_path, std::ios::binary);
            if (!out) return false;
            
            // 复制文件数据
            uint32_t file_checksum = crc32(0L, Z_NULL, 0);
            uint64_t remaining = fh.size;
            
            while (remaining > 0) {
                size_t to_read = std::min(remaining, buffer.size());
                in.read(reinterpret_cast<char*>(buffer.data()), to_read);
                size_t bytes_read = in.gcount();
                if (bytes_read == 0) break;
                
                file_checksum = crc32(file_checksum, buffer.data(), bytes_read);
                total_checksum = crc32(total_checksum, buffer.data(), bytes_read);
                
                if (!out.write(reinterpret_cast<const char*>(buffer.data()),
                             bytes_read)) {
                    return false;
                }
                
                remaining -= bytes_read;
            }
            
            // 验证文件校验和
            if (file_checksum != fh.checksum) {
                return false;
            }
        }
        
        // 验证总校验和
        return total_checksum == header.checksum;
    } catch (...) {
        return false;
    }
}

bool Packer::extractFile(const fs::path& package_path,
                        const std::string& file_path,
                        const fs::path& output_path) {
    try {
        std::ifstream in(package_path, std::ios::binary);
        if (!in) return false;
        
        // 读取包头
        PackageHeader header;
        if (!readHeader(in, header)) return false;
        
        // 验证魔数和版本
        if (header.magic != MAGIC_NUMBER || header.version != VERSION) {
            return false;
        }
        
        // 查找目标文件
        FileHeader target_fh;
        bool found = false;
        
        for (uint64_t i = 0; i < header.file_count; ++i) {
            FileHeader fh;
            if (!readFileHeader(in, fh)) return false;
            
            if (fh.path == file_path) {
                target_fh = fh;
                found = true;
                break;
            }
        }
        
        if (!found) return false;
        
        // 创建输出目录
        fs::create_directories(output_path.parent_path());
        
        // 定位到文件数据
        in.seekg(target_fh.offset);
        
        // 创建输出文件
        std::ofstream out(output_path, std::ios::binary);
        if (!out) return false;
        
        // 复制文件数据
        std::vector<uint8_t> buffer(BUFFER_SIZE);
        uint32_t file_checksum = crc32(0L, Z_NULL, 0);
        uint64_t remaining = target_fh.size;
        
        while (remaining > 0) {
            size_t to_read = std::min(remaining, buffer.size());
            in.read(reinterpret_cast<char*>(buffer.data()), to_read);
            size_t bytes_read = in.gcount();
            if (bytes_read == 0) break;
            
            file_checksum = crc32(file_checksum, buffer.data(), bytes_read);
            
            if (!out.write(reinterpret_cast<const char*>(buffer.data()),
                         bytes_read)) {
                return false;
            }
            
            remaining -= bytes_read;
        }
        
        // 验证文件校验和
        return file_checksum == target_fh.checksum;
    } catch (...) {
        return false;
    }
}

bool Packer::verifyChecksum(const fs::path& package_path) {
    try {
        std::ifstream in(package_path, std::ios::binary);
        if (!in) return false;
        
        // 读取包头
        PackageHeader header;
        if (!readHeader(in, header)) return false;
        
        // 验证魔数和版本
        if (header.magic != MAGIC_NUMBER || header.version != VERSION) {
            return false;
        }
        
        // 读取文件头列表
        std::vector<FileHeader> file_headers;
        for (uint64_t i = 0; i < header.file_count; ++i) {
            FileHeader fh;
            if (!readFileHeader(in, fh)) return false;
            file_headers.push_back(fh);
        }
        
        // 验证文件数据
        std::vector<uint8_t> buffer(BUFFER_SIZE);
        uint32_t total_checksum = crc32(0L, Z_NULL, 0);
        
        for (const auto& fh : file_headers) {
            // 定位到文件数据
            in.seekg(fh.offset);
            
            // 计算文件校验和
            uint32_t file_checksum = crc32(0L, Z_NULL, 0);
            uint64_t remaining = fh.size;
            
            while (remaining > 0) {
                size_t to_read = std::min(remaining, buffer.size());
                in.read(reinterpret_cast<char*>(buffer.data()), to_read);
                size_t bytes_read = in.gcount();
                if (bytes_read == 0) break;
                
                file_checksum = crc32(file_checksum, buffer.data(), bytes_read);
                total_checksum = crc32(total_checksum, buffer.data(), bytes_read);
                
                remaining -= bytes_read;
            }
            
            // 验证文件校验和
            if (file_checksum != fh.checksum) {
                return false;
            }
        }
        
        // 验证总校验和
        return total_checksum == header.checksum;
    } catch (...) {
        return false;
    }
}

std::vector<FileHeader> Packer::listContents(const fs::path& package_path) {
    std::vector<FileHeader> headers;
    
    try {
        std::ifstream in(package_path, std::ios::binary);
        if (!in) return headers;
        
        // 读取包头
        PackageHeader header;
        if (!readHeader(in, header)) return headers;
        
        // 验证魔数和版本
        if (header.magic != MAGIC_NUMBER || header.version != VERSION) {
            return headers;
        }
        
        // 读取文件头列表
        for (uint64_t i = 0; i < header.file_count; ++i) {
            FileHeader fh;
            if (!readFileHeader(in, fh)) {
                headers.clear();
                return headers;
            }
            headers.push_back(fh);
        }
    } catch (...) {
        headers.clear();
    }
    
    return headers;
}

} // namespace byte_enclave