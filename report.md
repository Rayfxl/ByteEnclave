# 项目介绍
在当今信息化迅速发展的时代，数据的安全性与完整性显得尤为重要。随着用户对文件存储和管理需求的增加，可靠的文件备份软件成为了保护关键信息的必要工具。本项目致力于开发一款功能强大、操作简便的文件备份软件，旨在满足个人用户及小型企业对数据备份与恢复的需求。

本软件不仅提供了基础的文件数据备份与还原功能，还集成了压缩解压、加密解密及友好的图形化用户界面等附加功能。这使得用户能够以更高效的方式管理文件，保障数据的安全性和可恢复性。

在本报告中，我们将详细阐述项目的设计理念、技术实现以及主要功能。希望通过本项目的实施，能够为用户带来更便捷、高效的数据管理体验。

## 设计理念

在设计我们的文件备份软件时，我们秉持以用户体验为中心的设计理念。软件的界面应简洁直观，确保用户可以快速上手，轻松完成备份和恢复操作。同时，为了满足不同用户的需求，本软件在设计时考虑了灵活性和扩展性，使其能够适应未来可能的功能扩展。数据安全是本项目的核心要素，我们通过采用强大的加密技术来保障备份数据的安全，减少数据丢失或意外删除的风险。此外，现代化的图形用户界面（GUI）设计将提升用户的操作效率，使得备份和恢复数据的过程更加顺畅。

## 技术实现

我们的文件备份软件主要采用 C++ 进行开发，主要技术实现如下：

1. **编程语言与框架**：主要使用 C++ 进行核心功能的开发，结合 Qt 框架来实现图形用户界面。这使得软件在跨平台方面具有良好的兼容性和用户体验。
2. **文件操作**：利用 C++ 的标准库及其文件输入/输出功能，处理文件系统操作，实现文件的备份与恢复。
3. **数据压缩与解压**：集成 zlib 库，以支持文件的压缩和解压功能，从而减少备份文件的存储空间。
4. **数据加密与解密**：使用 Crypto++ 库实现文件的加密和解密功能，为用户提供安全的数据备份解决方案。

通过以上技术的精妙结合，我们形成了一个高效、稳定的 C++ 文件备份软件，能够全面满足用户的文件备份和恢复需求。

## 主要功能

本软件的主要功能包括：

1. **数据备份**：允许用户选择特定文件夹及其子目录进行数据备份，并将备份文件保存至用户指定的位置，支持多种文件格式的备份。
2. **数据恢复**：用户可以快速从备份文件中恢复数据，确保在数据丢失或损坏时能够及时恢复文件。
3. **压缩与解压**：集成了对文件的压缩和解压缩功能，用户可以选择在备份时将文件进行压缩以节省存储空间。
4. **加密与解密**：提供对备份文件的加密和解密功能，用户可以选择使用密码对敏感数据进行保护，确保数据安全。
5. **图形化用户界面**：采用 Qt 框架开发的用户友好界面，满足用户的直观操作需求，提升整体用户体验。

通过实施以上功能，我们的文件备份软件能够为用户提供安全、便捷、高效的文件管理解决方案。

# 项目代码详解

## 文件功能概述表

| 文件路径                     | 功能描述                                                     |
| ---------------------------- | ------------------------------------------------------------ |
| `src\backup.cpp`             | 实现了一个文件备份和恢复系统，包含多种基础功能               |
| `src\compressor.cpp`         | 实现了一个数据压缩和解压缩的类 `Compressor`                  |
| `src\encryptor.cpp`          | 定义了一个加密器类 `Encryptor`                               |
| `src\filesystem.cpp`         | 实现了一个文件系统管理类`FileSystem`，提供了对文件元数据的获取、设置以及文件复制、链接创建等操作 |
| `src\packer.cpp`             | 实现一个文件打包和解包模块                                   |
| `src\python_bindings.cpp`    | 实现了一个用于Python的C++扩展模块，利用`pybind11`库将几个与文件处理相关的C++类绑定到Python中 |
| `test\backup_test.cpp`       | 针对 `BackupManager` 类的备份和恢复功能进行测试              |
| ``test\compressor_test.cpp`` | 验证 `Compressor` 类的功能和性能                             |
| ``test\encryptor_test.cpp`   | 测试加密和解密的基本功能、错误处理和安全性                   |
| ``test\filesystem_test.cpp`  | 测试文件系统相关功能的单元测试代码                           |
| `test\packer_test.cpp`       | 验证 `Packer` 类的打包（pack）和解包（unpack）功能           |
| `gui\main.py`                | 初始化并启动 PySide6 应用程序，显示主窗口                    |
| `gui\mainwindow.py`          | 创建主窗口，设置用户界面，处理备份和还原操作的交互           |

## 功能实现

### backup.cpp

文件 `backup.cpp` 中的代码实现了一个文件备份和恢复系统，属于 `byte_enclave` 命名空间。该系统通过 `BackupManager` 类管理备份操作，主要包含以下功能：

#### 主要功能

1. **构造函数**: 
   - `BackupManager()` 构造了一个文件系统接口的实例，使用智能指针管理生命周期。

2. **备份 (backup)**: 
   - 备份指定的文件或目录。
   - 过程包括权限和路径检查、文件收集和过滤、文件复制与验证、保存元数据（如 CRC32 校验和）。
   - 确保备份文件完整性，通过 CRC32 校验和验证。

3. **还原 (restore)**:
   - 从备份路径将文件或目录恢复到目标路径。
   - 包含权限和路径检查、文件还原等步骤，确保还原后的文件与源文件保持一致。

4. **列举备份内容 (listBackupContents)**:
   - 返回指定备份路径下的所有文件和符号链接的相对路径。

5. **验证备份 (verifyBackup)**:
   - 验证备份的完整性，可以针对目录和单文件备份进行检查。
   - 对文件的 CRC32 进行检查，确保文件未被损坏。

6. **验证单个文件 (verifyFile)**:
   - 针对普通文件和符号链接进行完整性验证，计算当前文件的 CRC32，并与存储的 CRC32 比较。

7. **计算预期校验和 (calculateExpectedChecksum)**:
   - 计算指定文件的 CRC32 值。

8. **收集文件 (collectFiles)**:
   - 递归遍历指定目录，按照自定义选项（如包含隐藏文件、符号链接和排除模式）收集符合条件的文件。

#### 技术细节
- 使用标准库的文件系统库(`std::filesystem`)来处理文件和目录操作。
- 使用 CRC32 算法进行文件完整性校验，依赖 `zlib.h` 库。
- 错误处理通过异常捕获，确保返回值为布尔类型。

#### 总结
该 `BackupManager` 类提供了全面的功能来执行文件和目录的备份与还原，包括数据验证和完整性检查，适用于需要保护和恢复文件数据的应用场景。

### compressor.cpp

该文件实现了一个数据压缩和解压缩的类 `Compressor`，使用了 zlib 库来进行处理。

#### 主要组件

1. **头文件引入**:
   - `compressor.hpp`: 该头文件应包含 `Compressor` 类的声明。
   - 标准库的头文件（如 `<fstream>`, `<cstring>`, `<filesystem>`）和 zlib 库的头文件 `<zlib.h>`。

2. **命名空间**:
   - 使用 `namespace byte_enclave`，将类及相关功能封装在该命名空间中，以避免与其他代码的冲突。

3. **类构造函数**:
   - `Compressor()`: 默认构造函数。

4. **压缩功能**:
   - `compress(const std::vector<uint8_t>& data)`: 
     - 使用 `LZ77` 风格的算法进行压缩。此方法查找最长的匹配字符串，并以 `(offset, length)` 对进行存储。如果没有找到匹配，则直接存储原始字节。
     - 压缩后的结果是一个字节流，其中包含 `(offset, length)` 对以及原始字节。
   
5. **解压缩功能**:
   - `decompress(const std::vector<uint8_t>& compressed_data)`: 
     - 对压缩的数据进行解码。如果 `(offset, length)` 对存在，则从解压数据中提取对应的字节段进行解压。如果没有匹配，则直接还原原始字节。

6. **计算压缩比**:
   - `getCompressionRatio(size_t original_size, size_t compressed_size)`: 
     - 输入: 原始数据大小和压缩数据大小
     - 返回: 计算得到的压缩比（原始大小/压缩大小），如果压缩大小为零则返回 0.0。

#### 总结:
该文件定义了一个有效的压缩和解压缩数据的工具，`LZ77` 风格的算法进行压缩。类提供了处理、异常管理和性能分析的功能，使其在各类应用程序中能够有效地使用。这种压缩方式相对简单，不如 `zlib` 高效，且对于某些类型的数据（如非常小的数据或随机数据）可能没有明显的压缩效果。

该实现仅适用于较为简单的情况。对于复杂的压缩任务，可能需要更高效的算法和数据结构。

### encryptor.cpp

文件 `encryptor.cpp` 定义了一个加密器类 `Encryptor`，用于实现基于 AES-256-GCM 算法的数据加密和解密功能。以下是该文件的主要组成部分及其功能概述：

1. **包含头文件**：
   - 引入了 `encryptor.hpp` 头文件、标准库、文件系统操作和 OpenSSL 库，提供必要的工具和数据结构。

2. **命名空间**：
   - 使用命名空间 `byte_enclave`，并在局部命名空间中定义一个帮助函数用于获取 OpenSSL 的错误信息。

3. **构造与析构函数**：
   - 构造函数初始化 OpenSSL 的 cipher context 并捕获错误。
   - 析构函数释放 cipher context。

4. **初始化功能**：
   - `initialize` 方法用来设置密码和盐值，并从中派生加密密钥。
   - 如果盐值未提供，则生成随机盐值。
   - 生成初始向量（IV）并检查初始化状态。

5. **加密功能**：
   - `encrypt` 方法接收待加密数据，使用 AES-256-GCM 加密，并返回包含 IV、密文和认证标签的字节数组。

6. **解密功能**：
   - `decrypt` 方法处理加密数据，提取 IV 和认证标签，执行 AES 解密并返回原始数据。

7. **IV 生成与设置**：
   - `generateIV` 方法生成一个随机的初始化向量。
   - `setIV` 方法允许用户手动设置 IV，并进行有效性检查。

8. **密钥派生**：
   - `deriveKey` 方法使用 PBKDF2 算法从密码和盐值生成加密密钥，支持迭代处理以增强安全性。

9. **错误处理**：
   - 多处使用异常处理来捕获和报告加密或解密过程中出现的错误。

此文件提供了一个安全、模块化的加密实现，适合用作数据保护机制。整体设计注重安全性和灵活性。

### filesystem.cpp

此文件是`byte_enclave`命名空间中的一部分，主要实现了一个文件系统管理类`FileSystem`，提供了对文件元数据的获取、设置以及文件复制、链接创建等操作。使用了C++17的`std::filesystem`库和相关Linux系统调用来处理文件和目录的操作。

#### 主要功能

1. **获取文件元数据**
   - `FileMetadata getFileMetadata(const fs::path& path)`: 获取指定文件的元数据，包括所有者、组、权限、最后修改时间等。

2. **设置文件元数据**
   - `bool setFileMetadata(const fs::path& path, const FileMetadata& metadata)`: 更新文件的权限、所有者、组和修改时间。

3. **文件复制**
   - `bool copyFile(const fs::path& src, const fs::path& dst)`: 复制文件并保持其元数据，支持复制普通文件和符号链接。

4. **创建符号链接**
   - `bool createSymlink(const fs::path& target, const fs::path& link)`: 创建指向目标的符号链接。

5. **创建硬链接**
   - `bool createHardlink(const fs::path& target, const fs::path& link)`: 创建目标文件的硬链接。

6. **创建命名管道**
   - `bool createNamedPipe(const fs::path& path)`: 创建命名管道。

7. **获取文件类型**
   - `fs::file_type getFileType(const fs::path& path)`: 获取指定路径的文件类型。

8. **复制文件的元数据**
   - `bool copyFileMetadata(const fs::path& src, const fs::path& dst)`: 复制源文件的元数据到目标文件。

9. **文件可读性检查**
   - `bool isReadable(const fs::path& path)`: 检查文件是否可读。

10. **文件可写性检查**
    - `bool isWritable(const fs::path& path)`: 检查文件是否可写或其父目录是否可写。

#### 总结

`filesystem.cpp`文件为文件管理提供了一套完整的接口，通过对文件的元数据操作和各种文件类型的支持，方便用户进行文件系统的操作与管理，文件中广泛使用异常处理，以确保在出现文件操作错误时能优雅地返回错误状态，而不是导致程序崩溃。。

### packer.cpp

文件`packer.cpp`是实现一个文件打包和解包模块，属于 `byte_enclave` 命名空间。以下是文件的概述：

#### 主要功能
1. **打包 (pack)**:
   - 将多个文件打包成一个单一文件，包含文件头信息（文件名、大小、偏移量和校验和）。
   - 计算并保存打包内容的校验和，以确保数据完整性。
   
2. **解包 (unpack)**:
   - 从打包文件中提取文件，并将它们恢复到指定的输出目录。
   - 在解包过程中验证提取的文件的校验和，确保文件未被篡改。

3. **提取单个文件 (extractFile)**:
   - 从打包文件中提取指定的单个文件，并进行校验和验证。

4. **验证包的校验和 (verifyChecksum)**:
   - 验证整个打包文件的校验和，确保文件的完整性。

5. **列出包内容 (listContents)**:
   - 返回打包文件中包含的文件列表及其相关信息。

#### 关键实现
- 使用 `zlib` 库计算文件数据的 CRC32 校验和。
- 利用 C++ 的 `std::filesystem` 处理文件路径和检查文件存在性。
- 实现了用于读取和写入固定大小数据和字符串的模板函数。
- 文件数据库的整体格式由包头（`PackageHeader`）和文件头（`FileHeader`）来定义，其中包含魔数、版本号和文件数量等信息。

#### 错误处理
- 代码中使用了异常捕获来处理文件打开和读取中的潜在错误，操作失败时返回 `false`。

#### 缓冲管理
- 在处理文件读写时，使用了自定义缓冲区以提高性能，特别是在读取和写入大文件时。

通过这些功能和实现，`packer.cpp` 文件有效地封装了文件的打包和解包过程，同时保证了数据的安全性和完整性。

### python_bindings.cpp

该文件实现了一个用于Python的C++扩展模块，名为`byte_enclave_python`，利用`pybind11`库将几个与文件处理相关的C++类绑定到Python中。以下是该模块的主要组成部分：

#### 包含的头文件

- `pybind11/pybind11.h`：用于Python和C++间的绑定。
- `pybind11/stl.h`：支持STL容器与Python的交互。
- 本地头文件包括`backup.hpp`、`packer.hpp`、`compressor.hpp`、`encryptor.hpp`，它们应该定义了该模块需要的主要功能。

#### 绑定的类
1. **FileMetadata**：管理文件元数据，包括拥有者、组、权限、访问时间、修改时间、创建时间和文件类型。
   
2. **BackupOptions**：包括备份配置选项，例如是否包含符号链接和隐藏文件，及排除模式列表。
   
3. **BackupManager**：提供了备份和恢复的功能，包括备份、恢复、列出备份内容和验证备份的功能。
   
4. **FileSystem**：实现文件系统操作，如获取和设置文件元数据、复制文件、创建符号链接、硬链接和命名管道，查询文件类型。
   
5. **Packer**：处理打包和解包的功能，包括打包、解包、提取文件和校验和验证。
   
6. **Compressor**：提供压缩和解压缩文件的功能。
   
7. **Encryptor**：实现初始化、加密和解密的功能。

#### 总结
该模块为涉及文件处理的操作提供了一个接口，可以在Python应用程序中方便地使用这些C++功能，增加了Python程序在文件备份、压缩和加密方面的能力。使用此模块，开发者能够通过Python调用上述所有提供的功能，以实现更高级的文件管理任务。

### backup_test.cpp

该文件 `backup_test.cpp` 是一个使用 Google Test 框架编写的单元测试程序，主要针对 `BackupManager` 类的备份和恢复功能进行测试。以下是文件的概述：

#### 概述

1. **包括头文件**：
   - 引入了 Google Test 和 Google Mock 相关的头文件。
   - 引入了 `backup.hpp`，该文件应包含 `BackupManager` 的定义。

2. **命名空间**：
   - 使用了 `std::filesystem` (命名为 `fs`) 进行文件和目录操作。

3. **测试类 `BackupTest`**：
   - 继承自 `::testing::Test`，用于组织测试用例。
   - **成员变量**：
     - `test_dir_`: 存储测试目录的路径。
     - `backup_manager_`: 唯一指针，指向 `BackupManager` 实例。

4. **测试准备和清理**：
   - `SetUp` 方法：创建临时测试目录，如果存在，尝试删除。然后设置权限并初始化 `BackupManager`。
   - `TearDown` 方法：清理测试目录，确保测试结束后环境干净。

5. **创建测试文件的方法**：
   - `createTestFile`：接受文件名称和内容，在测试目录中创建文件。

6. **测试用例**：
   通过 `TEST_F` 宏定义多组测试用例，每个主要测试的功能如下：
   - `BasicBackup`: 验证基本的文件备份和恢复功能。
   - `MetadataPreservation`: 验证备份和恢复过程中的元数据保持。
   - `SymlinkHandling`: 测试符号链接的处理能力。
   - `FileFiltering`: 测试备份时的文件过滤功能。
   - `BackupVerification`: 验证备份的完整性，检查损坏的备份文件。
   - `ErrorHandling`: 测试错误处理，如处理不存在的文件和无权限的目录。

#### 总结
该测试文件全面检查了备份和还原功能，确保文件的内容、权限和链接都能正确处理，增强了软件的鲁棒性和可靠性。

## 测试部分

### compressor_test.cpp

`compressor_test.cpp` 是一个使用 Google Test 框架编写的单元测试文件，用于验证 `Compressor` 类的功能和性能。测试文件位于 `ByteEnclave-basicmodule/test/` 目录中，主要涵盖了对数据压缩和解压缩过程的各类测试。

#### 主要内容

1. **测试框架**：
   - 使用 Google Test（`gtest`）和 Google Mock（`gmock`）库。

2. **测试类**：
   - `CompressorTest` 继承自 `::testing::Test`，提供测试用例的基础设施。
   - `SetUp` 方法用于在每个测试用例运行前创建 `Compressor` 实例。

3. **辅助函数**：
   - `generateRepeatingData`: 生成具有重复模式的测试数据。
   - `generateRandomData`: 生成随机数据。

4. **测试用例**：
   - `CompressEmptyData`: 测试对空数据的压缩和解压。
   - `CompressSmallData`: 测试对小数据的压缩和解压。
   - `CompressRepeatingData`: 测试对重复数据的压缩，检查压缩率。
   - `CompressRandomData`: 测试对随机数据的压缩，检查压缩率。
   - `CompressLargeData`: 测试对大数据的压缩和解压。
   - `CompressMixedData`: 测试对混合数据（重复和随机）的压缩。
   - `MultipleCompressionCycles`: 测试数据的多次压缩和解压。
   - `CompressionRatio`: 测试压缩率计算。
   - `ErrorHandling`: 测试对损坏数据的错误处理。
   - `EdgeCases`: 测试边界情况（如单字节数据和全零数据）。

#### 关键功能
- 每个测试用例都通过断言（如 `EXPECT_EQ` 和 `EXPECT_THROW`）验证压缩和解压的正确性，以及处理特定条件的能力。
- 压缩率的计算和验证确保压缩器在处理不同类型的数据时保持预期性能。
- 错误处理测试确保代码在异常情况下表现合理。

#### 结论
此文件通过全面的测试覆盖了 `Compressor` 类的各个方面，确保其稳定性和可靠性，是软件开发周期中保证代码质量的重要一环。

### encryptor_test.cpp

该文件包含了用于加密器（`Encryptor`）类的单元测试，使用的是Google测试框架 (gtest/gmock)。测试主要覆盖了加密和解密的基本功能、错误处理和安全性等方面。

#### 主要功能

1. **测试初始化**：
   - 验证加密器是否能够成功初始化，特别是以有效和无效密码。

2. **基本加密解密**：
   - 测试简单数据的加密和解密过程，确保数据可以成功恢复。

3. **空数据处理**：
   - 测试对空数据的加密和解密处理，确保返回结果为空。

4. **大数据处理**：
   - 对较大的数据（1MB）进行加密和解密测试。

5. **初始化向量（IV）处理**：
   - 测试IV的生成和设置，确保加密和解密使用正确的IV。

6. **不同密码解密**：
   - 确保使用不同密码尝试解密同一数据时抛出异常。

7. **多次加密测试**：
   - 测试多次对同一数据加密结果的不一致性，确保即便是相同输入得到的加密结果不同，并且都能成功解密。

8. **错误处理**：
   - 测试对损坏数据和无效IV的处理，确保正确抛出异常。

9. **密钥派生**：
   - 测试在相同密码和IV的情况下，使用盐值生成的密钥是否可以重复加密出相同结果。

#### 测试框架
使用了Google测试框架 (`gtest/hgmock`) 管理和运行测试，并验证加密器的不同功能和错误处理能力。

#### 总结
本文件为`Encryptor`类实现了全面的测试用例，确保其加密解密的正确性和安全性，处理了多种真实场景下的边界情况和错误情况。这些测试有助于提高代码的可靠性和安全性。

### filesystem_test.cpp

该文件 `filesystem_test.cpp` 是一个用于测试文件系统相关功能的单元测试代码，使用了 Google Test (gtest) 和 Google Mock (gmock) 库。

#### 主要内容概述：

1. **依赖库**：
   - 引入了 Google Test 和 Google Mock 以进行单元测试。
   - 引入了 `filesystem.hpp`，用于文件系统操作。

2. **命名空间**：
   - 采用了 `std::filesystem` 命名空间简化代码。

3. **测试类**：
   - `FileSystemTest` 继承自 `::testing::Test`，用来组织一系列与文件系统相关的测试用例。
   - 包含的成员：
     - `SetUp()`：测试开始前的初始化，包括创建一个临时测试目录，确保环境干净。
     - `TearDown()`：测试结束后的清理，删除测试目录。
     - `createTestFile()`：用于创建测试文件的辅助方法。

4. **测试用例**：
   - **获取文件元数据 (`GetFileMetadata`)**：测试从文件获取其元数据的功能。
   - **设置文件元数据 (`SetFileMetadata`)**：测试更新文件元数据的功能。
   - **复制文件 (`CopyFile`)**：测试文件复制功能及其内容和元数据的相同性。
   - **创建符号链接 (`CreateSymlink`)**：测试符号链接的创建，包括常规和悬空链接的处理。
   - **创建硬链接 (`CreateHardlink`)**：测试硬链接的创建和硬链接计数。
   - **创建命名管道 (`CreateNamedPipe`)**：测试命名管道的创建。
   - **获取文件类型 (`GetFileType`)**：测试获取不同文件类型（普通文件、目录、符号链接）的功能。
   - **错误处理 (`ErrorHandling`)**：测试对不存在的文件和权限错误的处理。

总结来说，该文件通过多个具体的测试用例，验证了文件系统操作类 `FileSystem` 的功能和错误处理能力，确保其在各种场景下的正确性和健壮性。

### packer_test.cpp

`packer_test.cpp` 是一个测试文件，使用 Google Test 和 Google Mock 库来验证 `Packer` 类的功能，该类主要负责文件的打包（pack）和解包（unpack）。

#### 主要内容：

1. **包含的库和命名空间**:
   - 引入 Google Test 和 Google Mock 库用于单元测试。
   - 使用 `std::filesystem` 进行文件和目录操作。
   - 引入 `packer.hpp`，假设其中定义了 `Packer` 类。

2. **PackerTest 类**:
   - 继承自 `::testing::Test`，用于设置和清理测试环境。
   - `SetUp()` 方法创建临时测试目录并初始化 `Packer` 实例。
   - `TearDown()` 方法删除测试生成的临时目录。

3. **辅助方法**:
   - `createTestFile()`: 创建带有指定内容的测试文件。
   - `compareFiles()`: 比较两个文件是否内容相同。

4. **主要测试用例**:
   - **打包单个文件**: 测试 `pack()` 方法是否能正确打包一个文件。
   - **打包多个文件**: 测试能否同时打包多个文件。
   - **解包文件**: 测试 `unpack()` 方法能否正确解包文件并验证解包内容。
   - **提取单个文件**: 测试从打包文件中提取特定文件的功能。
   - **校验和验证**: 测试打包后的文件是否通过校验和验证，以及验证损坏文件的情况。
   - **错误处理**: 测试对不存在文件和无效操作的处理。

该测试文件有效地验证了 `Packer` 类的各个功能，确保打包、解包、校验和错误处理逻辑的正确性。

### backup.hpp

`backup.hpp` 是一个头文件，定义了用于备份和恢复文件的功能。它是一个 C++ 项目的一部分，主要用于作为 `byte_enclave` 命名空间中的备份管理模块。这个模块提供了一些基本的备份和恢复操作，同时支持配置选项。

#### 主要结构和类

1. **BackupOptions**:
   - 结构体，包含备份选项。
   - 成员变量：
     - `include_symlinks`: 指定是否包括符号链接（默认值为 `true`）。
     - `include_hidden_files`: 指定是否包括隐藏文件（默认值为 `false`）。
     - `exclude_patterns`: 用于指定需要排除的文件模式的字符串向量。

2. **BackupManager**:
   - 类，负责执行备份和恢复操作。
   - 成员方法：
     - `BackupManager()`: 构造函数，初始化备份管理器。
     - `backup(...)`: 执行文件的备份操作，接受源路径、备份路径和备份选项作为参数。
     - `restore(...)`: 执行文件的恢复操作，从备份路径恢复到目标路径。
     - `listBackupContents(...)`: 列出指定备份路径的内容。
     - `verifyBackup(...)`: 验证备份文件的完整性。
   - 保护方法：
     - `verifyFile(...)`: 验证单个文件的完整性。
     - `calculateChecksum(...)`: 计算文件的校验和。
     - `collectFiles(...)`: 收集指定目录中的文件，根据备份选项过滤文件。
     - `calculateExpectedChecksum(...)`: 计算预期的文件校验和。
   - 私有成员：
     - `fs_`: 一个指向 `FileSystem` 类型的智能指针，用于文件系统操作。

#### 依赖和头文件
- 引入了 `filesystem.hpp` 及 C++ 标准库中的 `filesystem`, `string`, `vector`, `map`, `memory`, `fstream`, `stdexcept`, 和 `cstdint` 等头文件，以支持文件系统操作、数据结构和异常处理。

#### 用途
该头文件定义的类和结构提供了灵活的备份解决方案，适用于需要备份和恢复文件的应用程序。用户可以通过设置不同的 `BackupOptions` 来控制备份过程中的行为。

### compressor.hpp

该文件定义了一个名为 `Compressor` 的类，它用于数据的压缩和解压。

#### 主要组成部分

1. **头文件保护**: 
   - 使用 `#ifndef`, `#define`, 和 `#endif` 指令防止头文件重复包含。

2. **包含的库**: 
   - 引入了 `<vector>`, `<cstdint>`, 和 `<cstddef>`，提供了标准库中向量和数据类型的支持。

3. **命名空间**: 
   - 使用 `namespace byte_enclave` 来 encapsulate （封装）相关功能。

4. **Compressor 类**:
   - **构造函数**: `Compressor()` - 默认构造函数，用于初始化对象。
   
   - **压缩函数**:
     - `std::vector<uint8_t> compress(const std::vector<uint8_t>& data)`:
       - 输入: 待压缩的字节数据。
       - 输出: 压缩后的字节数据。
       - 异常: 当压缩失败时抛出 `std::runtime_error`。

   - **解压函数**:
     - `std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed_data)`:
       - 输入: 待解压的压缩字节数据。
       - 输出: 解压后的字节数据。
       - 异常: 可能会抛出 `std::runtime_error` 如果解压失败或数据无效。

   - **压缩比率计算函数**:
     - `double getCompressionRatio(size_t original_size, size_t compressed_size) const`:
       - 输入: 原始数据大小和压缩后数据大小。
       - 输出: 计算的压缩比率（原始大小 / 压缩后大小）。

#### 总结
此头文件为 `Compressor` 类提供了接口，用于实现数据的压缩和解压，并确保了异常处理以应对可能的错误情况。

### encryptor.hpp

这个文件定义了一个名为 `Encryptor` 的类，位于 `byte_enclave` 命名空间中，主要用于提供 AES-256 加密和解密功能。它的主要特点和功能包括：

#### 常量

- **KEY_SIZE**: 定义密钥的大小为 32 字节（256 位）。
- **IV_SIZE**: 定义初始化向量 (IV) 的大小为 16 字节（AES 块大小）。
- **SALT_SIZE**: 定义盐的大小为 32 字节。

#### 构造函数和析构函数
- `Encryptor()`: 默认构造函数。
- `~Encryptor()`: 析构函数，负责清理。

#### 初始化
- `bool initialize(const std::string& password, const std::vector<uint8_t>& salt)`: 使用给定的密码和可选的盐值初始化加密器。

#### 加密与解密
- `std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data)`: 加密输入数据并返回加密结果。
- `std::vector<uint8_t> decrypt(const std::vector<uint8_t>& encrypted_data)`: 解密给定的加密数据并返回原始数据。

#### 随机数生成
- `std::vector<uint8_t> generateIV()`: 生成一个随机的初始化向量 (IV)。

#### 设置与获取
- `void setIV(const std::vector<uint8_t>& iv)`: 设置初始化向量。
- `const std::vector<uint8_t>& getSalt() const`: 获取当前的盐值。

#### 私有成员函数
- `bool deriveKey(const std::string& password, std::vector<uint8_t>& key, std::vector<uint8_t>& salt)`: 从密码派生出加密用的密钥。

#### 成员变量
- `EVP_CIPHER_CTX* ctx_`: OpenSSL 的加密上下文，用于执行加密和解密操作。
- `std::vector<uint8_t> key_`: 存储派生的密钥。
- `std::vector<uint8_t> iv_`: 存储初始化向量。
- `std::vector<uint8_t> salt_`: 存储盐值，用于密钥派生。
- `bool initialized_`: 标记是否初始化过。
- `bool iv_set_`: 标记初始化向量是否已设置。

#### 总结
`encryptor.hpp` 文件主要提供了加密和解密功能，能够通过 AES-256 算法处理数据，支持密码和盐的使用，确保数据的安全性。使用此类时，用户可以初始化、加密、解密以及管理加密上下文的状态。

### filesystem.hpp

该文件 `filesystem.hpp` 定义了一个简单的文件系统操作模块，属于 `byte_enclave` 命名空间。以下是文件的主要内容和结构概述：

1. **文件包含**：
   - 使用了 C++ 标准库中的 `<filesystem>`，以及其他必要的头文件，如 `<string>`、`<vector>`、`<map>` 和 `<memory>`。

2. **命名空间**：
   - 定义了别名 `fs` 表示 `std::filesystem`，方便后续使用。

3. **结构体 `FileMetadata`**：
   - 用于存储文件的元数据，包括：
     - `owner`：文件所有者的用户名。
     - `group`：文件所属组名。
     - `permissions`：文件权限。
     - 时间戳 (`access_time`, `modify_time`, `create_time`)：记录文件的访问、修改和创建时间。
     - `file_type`：文件类型（如普通文件、目录、符号链接等）。

4. **类 `FileSystem`**：
   - 提供了一系列方法来执行文件系统操作：
     - `getFileMetadata`：获取指定文件的元数据信息。
     - `setFileMetadata`：设置指定文件的元数据信息。
     - `copyFile`：复制文件及其元数据。
     - `createSymlink`：创建符号链接。
     - `createHardlink`：创建硬链接。
     - `createNamedPipe`：创建命名管道。
     - `getFileType`：获取文件类型。
     - `isReadable` 和 `isWritable`：检查文件的可读性和可写性。
   - 还包含一个私有辅助函数 `copyFileMetadata`，用于复制文件的元数据。

5. **异常处理**：
   - 一些方法在特定情况下会抛出 `std::runtime_error`，如文件不存在或无法访问。

整体而言，这个文件为文件系统操作提供了一个简化和封装的接口，方便用户获取和修改文件的基本属性和进行其他相关操作。

### packer.hpp

`packer.hpp` 是一个用于文件打包和解包操作的头文件，属于 `byte_enclave` 命名空间。它定义了用于管理文件包的结构和功能，包括包头和文件头的定义，以及实现打包、解包和校验的类 `Packer`。

#### 主要结构体

1. **PackageHeader**:
   - `magic`: 用于识别文件格式的魔数。
   - `version`: 版本号，指明包的版本。
   - `file_count`: 包含的文件数量。
   - `total_size`: 包内文件的总大小。
   - `checksum`: 包头的校验和，用于确保数据完整性。

2. **FileHeader**:
   - `path`: 文件在包内的相对路径。
   - `size`: 下属文件的大小。
   - `offset`: 文件在包中的存储偏移。
   - `checksum`: 文件的校验和，用于验证文件的完整性。

#### 类 `Packer`
- 提供文件打包和解包的功能。
- 主要方法：
  - `pack(...)`: 将文件打包到指定路径。
  - `unpack(...)`: 从包中解压文件到指定目录。
  - `listContents(...)`: 列出包内所有文件的详情。
  - `extractFile(...)`: 从包中提取单个文件。
  - `verifyChecksum(...)`: 验证包的校验和以确保数据的正确性。

#### 私有方法
- `writeHeader(...)`: 向包中写入包头信息。
- `readHeader(...)`: 从包中读取包头信息。
- `calculateChecksum(...)`: 计算给定数据的校验和。

#### 依赖库
- `#include <filesystem>`: 用于文件系统操作。
- `#include <vector>`, `#include <string>`, `#include <memory>`: 标准库组件，用于数据容器和内存管理。

该文件提供的工具对于管理文件打包和解包操作非常有效，适合需要文件集合管理的应用程序。

## 图形界面

### main.py

该文件是一个使用 PySide6 构建的简单 GUI 应用程序的入口点。其主要功能是初始化一个 Qt 应用程序并显示主窗口。

#### 代码分析

1. **导入库**
   - `sys`：用于访问与 Python 解释器交互的参数和功能。
   - `QApplication`：从 PySide6 导入的 Qt 应用程序类，用于管理应用生命周期。
   - `MainWindow`：从 `mainwindow` 模块导入的自定义主窗口类。

2. **`main` 函数**
   - 创建 `QApplication` 实例，传入命令行参数（`sys.argv`），以支持从命令行运行。
   - 实例化 `MainWindow` 类，这是用户界面中主要的窗口。
   - 调用 `show()` 方法以显示主窗口。
   - 通过 `sys.exit(app.exec())` 来启动应用的事件循环，确保程序能够正确退出。

#### 总结

该文件是典型的 Qt 应用程序启动结构，负责创建和显示主窗口并处理应用程序的主事件循环。

### mainwindow.py

#### 文件描述

`mainwindow.py` 是一个使用 PySide6 库构建的图形用户界面 (GUI) 应用程序，主要用于实现文件的备份和还原功能。该文件定义了一个主窗口类 `MainWindow`，包含了设置用户界面的逻辑和交互功能。

#### 主要组成部分

1. **导入模块**：
   - 导入了必要的 QtWidgets 和 QtCore 组件，以构建和管理 GUI。

2. **MainWindow 类**：
   - 继承自 `QMainWindow`，定义了主窗口的基本结构和行为。
   - 在初始化时设置窗口标题并调用 `setup_ui()` 方法构建界面。

3. **setup_ui 方法**：
   - 创建中心部件和垂直布局。
   - 添加控件：
     - 源目录和目标目录标签及其选择按钮。
     - 备份和还原操作按钮。
     - 显示当前进度的进度条。
   - 设置最低窗口尺寸。

4. **槽函数**：
   - `select_source_directory()`: 打开文件对话框选择源目录，更新标签显示选中的目录。
   - `select_target_directory()`: 打开文件对话框选择目标目录，更新标签显示选中的目录。
   - `start_backup()`: 备份按钮的点击事件，当前仅显示提示信息，实际备份逻辑待实现。
   - `start_restore()`: 还原按钮的点击事件，当前仅显示提示信息，实际还原逻辑待实现。

#### 功能

- 用户可以选择源和目标目录进行备份和还原操作。
- 通过进度条可以显示当前操作的进度（实际功能待实现）。

这个程序的设计目的是提供一个简单的用户界面，便于用户管理文件备份与还原操作。
