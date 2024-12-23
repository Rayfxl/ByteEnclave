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
    out.write(reinterpret_cast<const char*>(&header), sizeof(PackageHeader));
    return out.good();
}

bool Packer::readHeader(std::ifstream& in, PackageHeader& header) {
    in.read(reinterpret_cast<char*>(&header), sizeof(PackageHeader));
    return in.good();
}

uint32_t Packer::calculateChecksum(const std::vector<uint8_t>& data) {
    uint32_t checksum = crc32(0L, Z_NULL, 0);
    return crc32(checksum, data.data(), data.size());
}

bool Packer::pack(const std::vector<fs::path>& files,
                 const fs::path& output_path) {
    try {
        std::ofstream out(output_path, std::ios::binary);
        if (!out) return false;
        
        // 设置输出流的缓冲区
        std::vector<char> out_buffer(BUFFER_SIZE);
        out.rdbuf()->pubsetbuf(out_buffer.data(), out_buffer.size());
        
        // 写入包头
        PackageHeader header;
        header.magic = MAGIC_NUMBER;
        header.version = VERSION;
        header.file_count = files.size();
        header.total_size = 0;
        header.checksum = 0;
        
        if (!writeHeader(out, header)) return false;
        
        // 计算文件头列表的大小
        size_t headers_size = 0;
        for (const auto& file : files) {
            if (!fs::exists(file)) {
                return false;
            }
            headers_size += sizeof(uint64_t) + file.filename().string().size();
            headers_size += sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint32_t);
        }
        
        // 写入文件头列表
        std::vector<FileHeader> file_headers;
        uint64_t current_offset = sizeof(PackageHeader) + headers_size;
        
        for (const auto& file : files) {
            FileHeader fh;
            fh.path = file.filename().string();
            fh.size = fs::file_size(file);
            fh.offset = current_offset;
            fh.checksum = 0;
            
            file_headers.push_back(fh);
            current_offset += fh.size;
            header.total_size += fh.size;
        }
        
        for (const auto& fh : file_headers) {
            if (!writeString(out, fh.path)) return false;
            if (!writeData(out, fh.size)) return false;
            if (!writeData(out, fh.offset)) return false;
            if (!writeData(out, fh.checksum)) return false;
        }
        
        // 写入文件数据并计算校验和
        uint32_t total_checksum = crc32(0L, Z_NULL, 0);
        std::vector<uint8_t> buffer(BUFFER_SIZE);
        
        for (size_t i = 0; i < files.size(); ++i) {
            std::ifstream in(files[i], std::ios::binary);
            if (!in) return false;
            
            // 设置输入流的缓冲区
            std::vector<char> in_buffer(BUFFER_SIZE);
            in.rdbuf()->pubsetbuf(in_buffer.data(), in_buffer.size());
            
            // 计算文件校验和
            uint32_t file_checksum = crc32(0L, Z_NULL, 0);
            auto file_start_pos = out.tellp();
            
            while (in) {
                in.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
                size_t bytes_read = in.gcount();
                if (bytes_read > 0) {
                    // 更新文件校验和
                    file_checksum = crc32(file_checksum, buffer.data(), bytes_read);
                    
                    // 写入文件数据
                    if (!out.write(reinterpret_cast<const char*>(buffer.data()),
                                 bytes_read)) {
                        return false;
                    }
                    
                    // 更新总校验和
                    total_checksum = crc32(total_checksum, buffer.data(), bytes_read);
                }
            }
            
            // 更新文件头的校验和
            file_headers[i].checksum = file_checksum;
        }
        
        // 更新文件头的校验和
        out.seekp(sizeof(PackageHeader));
        for (const auto& fh : file_headers) {
            if (!writeString(out, fh.path)) return false;
            if (!writeData(out, fh.size)) return false;
            if (!writeData(out, fh.offset)) return false;
            if (!writeData(out, fh.checksum)) return false;
        }
        
        // 更新包头的校验和
        header.checksum = total_checksum;
        out.seekp(0);
        if (!writeHeader(out, header)) return false;
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool Packer::unpack(const fs::path& package_path,
                   const fs::path& output_dir) {
    try {
        std::ifstream in(package_path, std::ios::binary);
        if (!in) return false;
        
        // 设置输入流的缓冲区
        std::vector<char> in_buffer(BUFFER_SIZE);
        in.rdbuf()->pubsetbuf(in_buffer.data(), in_buffer.size());
        
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
            if (!readString(in, fh.path)) return false;
            if (!readData(in, fh.size)) return false;
            if (!readData(in, fh.offset)) return false;
            if (!readData(in, fh.checksum)) return false;
            file_headers.push_back(fh);
        }
        
        // 计算总校验和
        uint32_t total_checksum = crc32(0L, Z_NULL, 0);
        std::vector<uint8_t> buffer(BUFFER_SIZE);
        
        // 创建输出目录
        fs::create_directories(output_dir);
        
        // 解包每个文件
        for (const auto& fh : file_headers) {
            auto output_path = output_dir / fh.path;
            fs::create_directories(output_path.parent_path());
            
            // 打开输出文件
            std::ofstream out(output_path, std::ios::binary);
            if (!out) return false;
            
            // 设置输出流的缓冲区
            std::vector<char> out_buffer(BUFFER_SIZE);
            out.rdbuf()->pubsetbuf(out_buffer.data(), out_buffer.size());
            
            // 定位到文件数据
            in.seekg(fh.offset);
            
            // 计算文件校验和
            uint32_t file_checksum = crc32(0L, Z_NULL, 0);
            uint64_t remaining = fh.size;
            
            while (remaining > 0) {
                size_t to_read = std::min(remaining, buffer.size());
                if (!in.read(reinterpret_cast<char*>(buffer.data()), to_read)) {
                    return false;
                }
                
                // 更新文件校验和
                file_checksum = crc32(file_checksum, buffer.data(), to_read);
                
                // 写入文件数据
                if (!out.write(reinterpret_cast<const char*>(buffer.data()),
                             to_read)) {
                    return false;
                }
                
                // 更新总校验和
                total_checksum = crc32(total_checksum, buffer.data(), to_read);
                remaining -= to_read;
            }
            
            // 验证文件校验和
            if (file_checksum != fh.checksum) {
                return false;
            }
        }
        
        // 验证包校验和
        if (total_checksum != header.checksum) {
            return false;
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool Packer::extractFile(const fs::path& package_path,
                        const std::string& file_path,
                        const fs::path& output_path) {
    try {
        std::ifstream in(package_path, std::ios::binary);
        if (!in) return false;
        
        // 设置输入流的缓冲区
        std::vector<char> in_buffer(BUFFER_SIZE);
        in.rdbuf()->pubsetbuf(in_buffer.data(), in_buffer.size());
        
        PackageHeader header;
        if (!readHeader(in, header)) return false;
        
        if (header.magic != MAGIC_NUMBER || header.version != VERSION) {
            return false;
        }
        
        // 读取文件头列表
        std::vector<FileHeader> file_headers;
        for (uint64_t i = 0; i < header.file_count; ++i) {
            FileHeader fh;
            if (!readString(in, fh.path)) return false;
            if (!readData(in, fh.size)) return false;
            if (!readData(in, fh.offset)) return false;
            if (!readData(in, fh.checksum)) return false;
            file_headers.push_back(fh);
        }
        
        // 查找目标文件
        const FileHeader* target_file = nullptr;
        for (const auto& fh : file_headers) {
            if (fh.path == file_path) {
                target_file = &fh;
                break;
            }
        }
        
        if (!target_file) return false;
        
        // 创建输出目录
        fs::create_directories(output_path.parent_path());
        
        // 打开输出文件
        std::ofstream out(output_path, std::ios::binary);
        if (!out) return false;
        
        // 设置输出流的缓冲区
        std::vector<char> out_buffer(BUFFER_SIZE);
        out.rdbuf()->pubsetbuf(out_buffer.data(), out_buffer.size());
        
        // 定位到文件数据
        in.seekg(target_file->offset);
        
        // 读取并写入文件数据
        std::vector<uint8_t> buffer(BUFFER_SIZE);
        uint32_t file_checksum = crc32(0L, Z_NULL, 0);
        uint64_t remaining = target_file->size;
        
        while (remaining > 0) {
            size_t to_read = std::min(remaining, buffer.size());
            if (!in.read(reinterpret_cast<char*>(buffer.data()), to_read)) {
                return false;
            }
            
            // 更新文件校验和
            file_checksum = crc32(file_checksum, buffer.data(), to_read);
            
            // 写入文件数据
            if (!out.write(reinterpret_cast<const char*>(buffer.data()),
                         to_read)) {
                return false;
            }
            
            remaining -= to_read;
        }
        
        // 验证文件校验和
        if (file_checksum != target_file->checksum) {
            return false;
        }
        
        return true;
    } catch (const std::exception&) {
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
        
        // 保存原始校验和
        uint32_t stored_checksum = header.checksum;
        
        // 读取文件头列表
        std::vector<FileHeader> file_headers;
        for (uint64_t i = 0; i < header.file_count; ++i) {
            FileHeader fh;
            if (!readString(in, fh.path)) return false;
            if (!readData(in, fh.size)) return false;
            if (!readData(in, fh.offset)) return false;
            if (!readData(in, fh.checksum)) return false;
            file_headers.push_back(fh);
        }
        
        // 计算总校验和
        uint32_t total_checksum = crc32(0L, Z_NULL, 0);
        for (const auto& fh : file_headers) {
            // 保存当前位置
            auto current_pos = in.tellg();
            
            // 读取文件数据
            std::vector<uint8_t> file_data(fh.size);
            in.seekg(fh.offset);
            
            if (!in.read(reinterpret_cast<char*>(file_data.data()),
                        file_data.size())) {
                return false;
            }
            
            // 验证文件校验和
            if (calculateChecksum(file_data) != fh.checksum) {
                return false;
            }
            
            // 更新总校验和
            total_checksum = crc32(total_checksum, file_data.data(), file_data.size());
            
            // 恢复文件位置
            in.seekg(current_pos);
        }
        
        // 验证包校验和
        if (total_checksum != stored_checksum) {
            return false;
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<FileHeader> Packer::listContents(const fs::path& package_path) {
    std::vector<FileHeader> contents;
    
    try {
        std::ifstream in(package_path, std::ios::binary);
        if (!in) return contents;
        
        PackageHeader header;
        if (!readHeader(in, header)) return contents;
        
        if (header.magic != MAGIC_NUMBER || header.version != VERSION) {
            return contents;
        }
        
        for (uint64_t i = 0; i < header.file_count; ++i) {
            FileHeader fh;
            if (!readString(in, fh.path)) return contents;
            if (!readData(in, fh.size)) return contents;
            if (!readData(in, fh.offset)) return contents;
            if (!readData(in, fh.checksum)) return contents;
            contents.push_back(fh);
        }
    } catch (const std::exception&) {
        contents.clear();
    }
    
    return contents;
}

} // namespace byte_enclave