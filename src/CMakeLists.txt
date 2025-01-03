add_library(byte_enclave_core
    filesystem.cpp
    backup.cpp
    packer.cpp
    compressor.cpp
    encryptor.cpp
)

target_include_directories(byte_enclave_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(byte_enclave_core PUBLIC cxx_std_17)
target_compile_options(byte_enclave_core PRIVATE -std=c++17 -fPIC)

# 查找zlib库
find_package(ZLIB REQUIRED)

target_link_libraries(byte_enclave_core
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
) 