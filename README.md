# QinXHttpServer
my cpp http server

### 一、构建流程

本仓库使用cmake构建，适配类unix系统，若有某些系统不适配请联系作者。

##### linux：（cmake version 3.22.1 gcc version 11.4.0 (Ubuntu 11.4.0-1ubuntu1~22.04)）

###### 1. 克隆代码仓

git clone --recursive  https://github.com/QinX132/QinXHttpServer.git (--recursive 克隆所有子模块的代码仓)

###### 2. 构建第三方库

在工程目录执行./build -t ，将会自动进入third_party，执行third_party_build_all.sh脚本，构建gmssl()、libevent库（优先构建静态库）

三方库依赖以及其版本：

> [submodule "third_party/GmSSL"]
>     path = third_party/GmSSL
>     url = https://github.com/guanzhi/GmSSL.git (commit:6de0e022)
> [submodule "third_party/json"]
>     path = third_party/json
>     url = https://github.com/nlohmann/json.git (commit:199dea11)
> [submodule "third_party/libevent"]
>     path = third_party/libevent
>     url = https://github.com/libevent/libevent.git (commit:4fd07f0e)


###### 3. 部署protobuf
1. 下载protobuf并部署：

   ```sh
   wget https://github.com/protocolbuffers/protobuf/releases/download/v21.12/protobuf-all-21.12.zip
   unzip protobuf-all-21.12.zip
   cd protobuf-all-21.12
   ./configure
   make
   sudo make install
   sudo ldconfig                                      # refresh shared library cache.
   ```

> （开发者注：cmake如何使用protobuf：）
>
> * 首先找到依赖：
>     `find_package(Protobuf REQUIRED)`
>
> *  include路径添加：
>
>     `${PROTOBUF_INCLUDE_DIRS}`
>
> * 源文件添加cc文件；
>
> * link添加：
>
>     `${PROTOBUF_LIBRARIES}`

###### 4. 构建utils代码仓：

在工程目录执行./build -u ，将会自动进入utils，执行 utils_build.sh脚本，编译所有.c文件，并且将此代码仓的文件编译为.a文件。此代码仓包含了一些c语言编写的模块功能，包括安全管理模块、健康检查模块、日志模块、内存管理模块、命令行模块、线程池模块、定时器模块、网络消息模块、作者自己编写的双向循环链表，以及一些常用的api。（相关的模块说明待开发者补充）

该目录内置unittest，如果要执行单元测试，执行如下操作：

```sh
cd utils

rm build -rf && mkdir build && pushd build

cmake -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=ON ..

make && make test
```

###### 5. 构建server 、 client 代码共享仓（主要是protobuf文件）

在工程目录执行./build -h ，将会自动进入SCShare，执行 scshare_build.sh脚本，编译.proto文件，并且将此代码仓的文件编译为.a文件

###### 6. 构建 server、client

在工程目录执行./build -sc ，将会自动进入server、client，执行脚本，编译。生成可执行文件server/src/backEnd/build/QXServer、 client/build/QXClient。

### 二、utils 说明

### 三、server 说明

### 四、client 说明