### 环境准备
建议使用Linux系统，windows下使用WSL，配置方法和Linux一致。

1. 安装gcc、g++，请确认版本为 11.3 及以上的稳定版本
``` bash
sudo apt install gcc g++
```

2. 安装CMake，请确认版本为 3.17 及以上的稳定版本
``` bash
sudo apt install cmake
```

2. 安装make
``` bash
sudo apt install make
```

### 构建命令
配置好上述环境后，进入项目目录后可以通过以下命令进行构建。
- `make`/`make build`: 构建整个项目;
- `make test-cpp`: 构建项目后执行测例;
- `make clean`：清理生成文件

构建之后生成的动态库放入了主目录下的lib目录下，方便用pyinstaller打包。

若要继续开发，需先写好C++代码，再将接口通过pybind11绑定，最后在python前端中使用。

本项目最后打包生成的可执行文件适用于linux。

3rd-party：项目所使用的第三方库。
dist：可执行文件。
docs：用户及开发文档。
gui：前端。
include：头文件。
lib：库文件。
scripts：脚本文件。
src：源文件。
test：测试文件。
