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

// 添加辅助函数
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
        
        // 设置输出流的缓冲区
        std::vector<char> out_buffer(BUFFER_SIZE);
        out.rdbuf()->pubsetbuf(out_buffer.data(), out_buffer.size());
        
        // 写入包头
        PackageHeader header;
        header.magic = MAGIC_NUMBER;
        header.version = VERSION;
        header.file_count = 0;  // 稍后更新
        header.total_size = 0;
        header.checksum = 0;
        
        if (!writeHeader(out, header)) return false;
        
        // 获取基准路径
        fs::path base_path;
        std::vector<fs::path> files_to_pack;
        
        if (!files.empty()) {
            if (fs::is_directory(files[0])) {
                base_path = files[0];
                // 收集目录下所有文件
                for (const auto& entry : fs::recursive_directory_iterator(files[0])) {
                    if (fs::is_regular_file(entry) || fs::is_symlink(entry)) {
                        files_to_pack.push_back(entry);
                    }
                }
            } else {
                base_path = files[0].parent_path();
                files_to_pack = files;
            }
        }
        
        // 更新文件数量
        header.file_count = files_to_pack.size();
        
        // 计算文件头的位置
        size_t headers_start = sizeof(PackageHeader);
        size_t data_start = headers_start;
        
        // 计算文件头大小
        for (const auto& file : files_to_pack) {
            data_start += sizeof(FileHeader::Type);  // 类型
            data_start += sizeof(uint64_t) + fs::relative(file, base_path).string().size();  // 路径
            data_start += sizeof(uint64_t) * 2 + sizeof(uint32_t);  // size, offset, checksum
            
            if (fs::is_symlink(file)) {
                data_start += sizeof(uint64_t) + fs::read_symlink(file).string().size();  // link_target
            }
        }
        
        // 写入文件头
        std::vector<FileHeader> file_headers;
        uint64_t current_offset = data_start;
        
        for (const auto& file : files_to_pack) {
            FileHeader fh;
            fh.path = fs::relative(file, base_path).string();
            
            if (fs::is_symlink(file)) {
                fh.type = FileHeader::Type::Symlink;
                fh.link_target = fs::read_symlink(file).string();
                fh.size = fh.link_target.length();
            } else {
                fh.type = FileHeader::Type::Regular;
                fh.size = fs::file_size(file);
            }
            
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
        
        for (size_t i = 0; i < files_to_pack.size(); ++i) {
            const auto& file = files_to_pack[i];
            auto& fh = file_headers[i];
            
            if (fs::is_symlink(file)) {
                // 写入符号链接目标
                if (!out.write(fh.link_target.c_str(), fh.link_target.length())) {
                    return false;
                }
                total_checksum = crc32(total_checksum, 
                    (const Bytef*)fh.link_target.c_str(), 
                    fh.link_target.length());
            } else {
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
            if (!readFileHeader(in, fh)) return false;
            file_headers.push_back(fh);
        }
        
        // 创建输出目录
        fs::create_directories(output_dir);
        
        // 计算总校验和
        uint32_t total_checksum = crc32(0L, Z_NULL, 0);
        std::vector<uint8_t> buffer(BUFFER_SIZE);
        
        // 解包每个文件
        for (const auto& fh : file_headers) {
            auto output_path = output_dir / fh.path;
            fs::create_directories(output_path.parent_path());
            
            // 定位到文件数据
            in.seekg(fh.offset);
            
            if (fh.type == FileHeader::Type::Symlink) {
                // 读取并创建符号链接
                std::string link_target;
                link_target.resize(fh.size);
                if (!in.read(link_target.data(), fh.size)) return false;
                
                total_checksum = crc32(total_checksum, 
                    (const Bytef*)link_target.c_str(), 
                    link_target.length());
                
                try {
                    if (fs::exists(output_path)) {
                        fs::remove(output_path);
                    }
                    fs::create_symlink(link_target, output_path);
                } catch (...) {
                    return false;
                }
            } else {
                // 读取并写入文件内容
                std::ofstream out(output_path, std::ios::binary);
                if (!out) return false;
                
                uint32_t file_checksum = crc32(0L, Z_NULL, 0);
                uint64_t remaining = fh.size;
                
                while (remaining > 0) {
                    size_t to_read = std::min(remaining, buffer.size());
                    if (!in.read(reinterpret_cast<char*>(buffer.data()), to_read)) {
                        return false;
                    }
                    
                    file_checksum = crc32(file_checksum, buffer.data(), to_read);
                    total_checksum = crc32(total_checksum, buffer.data(), to_read);
                    
                    if (!out.write(reinterpret_cast<const char*>(buffer.data()),
                                 to_read)) {
                        return false;
                    }
                    
                    remaining -= to_read;
                }
                
                // 验证文件校验和
                if (file_checksum != fh.checksum) {
                    return false;
                }
            }
        }
        
        // 验证总校验和
        if (total_checksum != header.checksum) {
            return false;
        }
        
        return true;
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