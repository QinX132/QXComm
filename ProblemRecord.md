# 开发者手册

## 一、部署日志

#### 1. cmake如何使用protobuf

* 首先找到依赖：
  `find_package(Protobuf REQUIRED)`

* include路径添加：

  `${PROTOBUF_INCLUDE_DIRS}`

* 源文件添加cc文件；

* link添加：

  `${PROTOBUF_LIBRARIES}`

#### 2. vue工程部署

安装nodejs、npm：

```sh
curl -sL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt-get install -y nodejs
```

(安装全局组件)

```sh
npm install -g yarn
npm install -g @vue/cli
npm install -g @vue/cli-service
npm install -g @vue/cli-plugin-babel
npm install -g @vue/cli-plugin-eslint
```

安装vue-router:

```sh
sudo npm install vue-router
```

安装element ui：

```sh
npm install element-plus --save
```

创建工程

```sh
vue create qxcomm
```

进入工程并启动工程

```
cd qxcomm
npm install // 下载组件
npm run serve
```

构建应用

```sh
cd qxcomm
npm run build
```



（使用模板：https://lolicode.gitee.io/scui-doc/guide/）



#### 3. 部署时遇见问题

* “密钥存储在过时的 trusted.gpg 密钥环中”：

  ```sh
  cd /etc/apt
  sudo cp trusted.gpg trusted.gpg.d
  ```

  
