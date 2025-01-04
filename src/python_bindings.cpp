#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "backup.hpp"
#include "packer.hpp"
#include "compressor.hpp"
#include "encryptor.hpp"

namespace py = pybind11;
namespace fs = std::filesystem;

PYBIND11_MODULE(byte_enclave_python, m) {
    using namespace byte_enclave;
    
    py::class_<FileMetadata>(m, "FileMetadata")
        .def(py::init<>())
        .def_readwrite("owner", &FileMetadata::owner)
        .def_readwrite("group", &FileMetadata::group)
        .def_readwrite("permissions", &FileMetadata::permissions)
        .def_readwrite("access_time", &FileMetadata::access_time)
        .def_readwrite("modify_time", &FileMetadata::modify_time)
        .def_readwrite("create_time", &FileMetadata::create_time)
        .def_readwrite("file_type", &FileMetadata::file_type);
    
    py::class_<BackupOptions>(m, "BackupOptions")
        .def(py::init<>())
        .def_readwrite("include_hidden_files", &BackupOptions::include_hidden_files)
        .def_readwrite("exclude_patterns", &BackupOptions::exclude_patterns)
        .def_readwrite("password", &BackupOptions::password);
    
    py::class_<BackupManager>(m, "BackupManager")
        .def(py::init<>())
        .def("backup", &BackupManager::backup)
        .def("restore", &BackupManager::restore)
        .def("list_backup_contents", &BackupManager::listBackupContents)
        .def("verify_backup", &BackupManager::verifyBackup);
    
    py::class_<FileSystem>(m, "FileSystem")
        .def(py::init<>())
        .def("get_file_metadata", &FileSystem::getFileMetadata)
        .def("set_file_metadata", &FileSystem::setFileMetadata)
        .def("copy_file", &FileSystem::copyFile)
        .def("create_symlink", &FileSystem::createSymlink)
        .def("create_hardlink", &FileSystem::createHardlink)
        .def("create_named_pipe", &FileSystem::createNamedPipe);
    
    py::class_<Packer>(m, "Packer")
        .def(py::init<>())
        .def("pack", &Packer::pack)
        .def("unpack", &Packer::unpack)
        .def("extract_file", &Packer::extractFile)
        .def("verify_checksum", &Packer::verifyChecksum)
        .def("list_contents", &Packer::listContents);
    
    py::class_<Compressor>(m, "Compressor")
        .def(py::init<>())
        .def("compress", &Compressor::compress)
        .def("decompress", &Compressor::decompress);
    
    py::class_<Encryptor>(m, "Encryptor")
        .def(py::init<>())
        .def("initialize", &Encryptor::initialize)
        .def("encrypt", static_cast<bool (Encryptor::*)(const fs::path&, const fs::path&, const std::string&)>(&Encryptor::encrypt))
        .def("decrypt", static_cast<bool (Encryptor::*)(const fs::path&, const fs::path&, const std::string&)>(&Encryptor::decrypt))
        .def("encrypt_data", static_cast<std::vector<uint8_t> (Encryptor::*)(const std::vector<uint8_t>&)>(&Encryptor::encrypt))
        .def("decrypt_data", static_cast<std::vector<uint8_t> (Encryptor::*)(const std::vector<uint8_t>&)>(&Encryptor::decrypt));
} 