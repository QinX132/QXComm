# QinXHttpServer
my cpp http server

### 一、构建流程

##### linux：（cmake version 3.22.1）

###### 1. 构建第三方库


###### 2. 部署protobuf
1. 下载protobuf并部署：

  > wget https://github.com/protocolbuffers/protobuf/releases/download/v21.12/protobuf-all-21.12.zip
  > unzip protobuf-all-21.12.zip
  > cd protobuf-all-21.12
  > ./configure
  > make
  > sudo make install
  > sudo ldconfig # refresh shared library cache.

2. cmake如何使用protobuf：
  首先找到依赖：
  find_package(Protobuf REQUIRED)

  include路径添加：

  ${PROTOBUF_INCLUDE_DIRS}

  源文件添加cc文件；

  link添加：

  ${PROTOBUF_LIBRARIES}
