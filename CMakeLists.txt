cmake_minimum_required(VERSION 3.15)
project(ByteEnclave VERSION 0.1.0 LANGUAGES CXX)

# 启用测试
enable_testing()

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 设置编译选项
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -std=c++17 -fPIC)
endif()

# 查找依赖包
find_package(OpenSSL REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# 添加pybind11
add_subdirectory(3rd-party/pybind11)

# 添加子目录
add_subdirectory(src)
add_subdirectory(test)

# 设置核心库目标
add_library(byte_enclave STATIC
    src/filesystem.cpp
    src/backup.cpp
    src/packer.cpp
    src/compressor.cpp
    src/encryptor.cpp
)

# 设置包含目录
target_include_directories(byte_enclave
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 设置编译选项
target_compile_features(byte_enclave PUBLIC cxx_std_17)
target_compile_options(byte_enclave PRIVATE -std=c++17 -fPIC)

# 链接依赖库
target_link_libraries(byte_enclave
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
)

# 创建Python绑定模块
pybind11_add_module(byte_enclave_python
    src/python_bindings.cpp
)

target_link_libraries(byte_enclave_python
    PRIVATE
        byte_enclave
)

# 安装配置
include(GNUInstallDirs)
install(TARGETS byte_enclave byte_enclave_python
    EXPORT byte_enclave-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

# 导出配置
install(EXPORT byte_enclave-targets
    FILE ByteEnclaveTargets.cmake
    NAMESPACE ByteEnclave::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ByteEnclave
)
