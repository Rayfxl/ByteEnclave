cmake_minimum_required(VERSION 3.14)

# 添加Google Test
add_subdirectory(${CMAKE_SOURCE_DIR}/3rd-party/googletest googletest_build)

# 启用测试
enable_testing()

# 添加测试源文件
add_executable(filesystem_test filesystem_test.cpp)
target_link_libraries(filesystem_test PRIVATE
    gtest
    gtest_main
    gmock
    gmock_main
    byte_enclave_core
)

add_executable(packer_test packer_test.cpp)
target_link_libraries(packer_test PRIVATE
    gtest
    gtest_main
    gmock
    gmock_main
    byte_enclave_core
)

add_executable(compressor_test compressor_test.cpp)
target_link_libraries(compressor_test PRIVATE
    gtest
    gtest_main
    gmock
    gmock_main
    byte_enclave_core
)

add_executable(encryptor_test encryptor_test.cpp)
target_link_libraries(encryptor_test PRIVATE
    gtest
    gtest_main
    gmock
    gmock_main
    byte_enclave_core
    OpenSSL::Crypto
)

add_executable(backup_test backup_test.cpp)
target_link_libraries(backup_test PRIVATE
    gtest
    gtest_main
    gmock
    gmock_main
    byte_enclave_core
)

add_executable(integration_test integration_test.cpp)
target_link_libraries(integration_test PRIVATE
    gtest
    gtest_main
    gmock
    gmock_main
    byte_enclave_core
    OpenSSL::Crypto
)

# 添加包含目录
target_include_directories(filesystem_test PRIVATE ${CMAKE_SOURCE_DIR}/3rd-party/googletest/googlemock/include)
target_include_directories(packer_test PRIVATE ${CMAKE_SOURCE_DIR}/3rd-party/googletest/googlemock/include)
target_include_directories(compressor_test PRIVATE ${CMAKE_SOURCE_DIR}/3rd-party/googletest/googlemock/include)
target_include_directories(encryptor_test PRIVATE ${CMAKE_SOURCE_DIR}/3rd-party/googletest/googlemock/include)
target_include_directories(backup_test PRIVATE ${CMAKE_SOURCE_DIR}/3rd-party/googletest/googlemock/include)
target_include_directories(integration_test PRIVATE ${CMAKE_SOURCE_DIR}/3rd-party/googletest/googlemock/include)

# 添加测试
add_test(NAME filesystem_test COMMAND $<TARGET_FILE:filesystem_test>)
add_test(NAME packer_test COMMAND $<TARGET_FILE:packer_test>)
add_test(NAME compressor_test COMMAND $<TARGET_FILE:compressor_test>)
add_test(NAME encryptor_test COMMAND $<TARGET_FILE:encryptor_test>)
add_test(NAME backup_test COMMAND $<TARGET_FILE:backup_test>)
add_test(NAME integration_test COMMAND $<TARGET_FILE:integration_test>) 