---
sidebar_position: 7
sidebar_label: C#
title: C# Connector
---

## 总体介绍

C# connector 目前是通过 TDengine 客户端驱动建立本地连接。但是对于RESTful连接并未进行暂未封装，需要用户参考 [RESTful APIs](https://docs.taosdata.com//reference/restful-api/) 自己封装。

## 支持的平台

C# 连接器支持的系统有：Linux 64/Windows x64/Windows x86

## 版本支持

| TDengine.Connector |             TDengine 版本             | 说明                           |
|--------------------|:-------------------------------------:|--------------------------------|
|        1.0.2       | TDever-2.5.0.x,ver-2.4.0.x,ver-2.3.4+ | 连接管理、同步查询、错误信息   |
|        1.0.3       | TDever-2.5.0.x,ver-2.4.0.x,ver-2.3.4+ | 参数绑定、schemaless, json tag |
|        1.0.4       | TDever-2.5.0.x,ver-2.4.0.x,ver-2.3.4+ | 异步查询，订阅、修复绑定参数     |
|        1.0.5       | TDever-2.5.0.x,ver-2.4.0.x,ver-2.3.4+ | 修复 windows 同步查询中文报错   |
|        1.0.6       | TDever-2.5.0.x,ver-2.4.0.x,ver-2.3.4+ | 修复schemaless |

## 支持的特性

### 本地连接

"本地连接" 指连接器通过本地的客户端驱动程序 taosc 直接与服务端程序 taosd 建立连接。
本地连接支持的特新如下：

1. 连接管理
2. 同步查询
3. 异步查询
4. 参数绑定
5. 获取系统信息（暂不支持）
6. 错误信息
7. 订阅功能
8. Schemaless

### RESTful 连接

暂未封装，需要用户参考 [RESTful APIs](https://docs.taosdata.com//reference/restful-api/) 自己封装。

## 安装步骤

### 安装前的准备

* 安装 [.NET SDK](https://dotnet.microsoft.com/download)

* 安装 [TDengine 客户端](/reference/connector/#安装客户端驱动)

* 安装 （可选） [Nuget 客户端](https://docs.microsoft.com/en-us/nuget/install-nuget-client-tools)

### 使用 dotnet CLI 安装

使用 C# Connector 连接数据库前，需要具备以下条件：

1. Linux 或 Windows 操作系统
2. .Net 5.0 以上运行时环境
3. TDengine-client

**注意**：由于 TDengine 的应用驱动是使用 C 语言开发的，使用 C# 驱动包时需要依赖系统对应的本地函数库。

- TDengine 客户端驱动在 Linux 系统中成功安装 TDengine 后，依赖的本地客户端驱动 libtaos.so 文件会被自动拷贝至 /usr/lib/libtaos.so，该目录包含在 Linux 自动扫描路径上，无需单独指定。
- TDengine 客户端驱动在 Windows 系统中安装完客户端之后，驱动包依赖的 taos.dll 文件会自动拷贝到系统默认搜索路径 C:/Windows/System32 下，同样无需要单独指定。

**注意**：在 Windows 环境开发时需要安装 TDengine 对应的 [windows 客户端](https://www.taosdata.com/cn/all-downloads/#TDengine-Windows-Client)，Linux 服务器安装完 TDengine 之后默认已安装 client，也可以单独安装 [Linux 客户端](/get-started/) 连接远程 TDengine Server。

#### 使用 Nuget 获取 C# 驱动

可以在当前 .NET 项目的路径中，通过 dotnet 命令引用 Nuget 的 C# connector 到当前项目。

``` bash
dotnet add package TDengine.Connector
```

#### 使用源码获取 C# 驱动

可以通过下载 TDengine 的源码，自己引用最新版本的 C# connector

```bash
git clone https://github.com/taosdata/TDengine.git
cd TDengine/src/connector/C#/src/
cp -r C#/src/TDengineDriver/ myProject

cd myProject
dotnet add TDengineDriver/TDengineDriver.csproj
```

## 建立连接

### 建立本地来连接

``` C#
using TDengineDriver;

namespace TDengineExample
{

    internal class EstablishConnection
    {
        static void Main(String[] args)
        {
            string host = "localhost";
            short port = 6030;
            string username = "root";
            string password = "taosdata";
            string dbname = "";

            var conn = TDengine.Connect(host, username, password, dbname, port);
            if (conn == IntPtr.Zero)
            {
                Console.WriteLine("Connect to TDengine failed");
            }
            else
            {
                Console.WriteLine("Connect to TDengine success");
            }
            TDengine.Close(conn);
            TDengine.Cleanup();
        }
    }
}

```

### 建立 RESTful 连接

暂未封装，需要用户参考 [RESTful APIs](https://docs.taosdata.com//reference/restful-api/) 自己封装。

## 使用示例

|示例程序                                                                                                               | 示例程序描述                                       |
|--------------------------------------------------------------------------------------------------------------------|--------------------------------------------|
| [C#checker](https://github.com/taosdata/TDengine/tree/develop/examples/C%23/C%23checker)                           | 使用 C# Driver 可以根据help命令给出的参数列表，测试C# Driver的同步增删改    |
| [TDengineTest](https://github.com/taosdata/TDengine/tree/develop/examples/C%23/TDengineTest)                       | 使用 C# Driver 的简单增删改查示例 |
| [insertCn](https://github.com/taosdata/TDengine/tree/develop/examples/C%23/insertCn)                               | 使用 C# Driver 插入和查询中文的字符示例    |
| [jsonTag](https://github.com/taosdata/TDengine/tree/develop/examples/C%23/jsonTag)                                 | 使用 C# Driver  插入和查询 json tag 的示例 |
| [stmt](https://github.com/taosdata/TDengine/tree/develop/examples/C%23/stmt)                                       | 使用 C# Driver 绑定参数的示例 |
| [schemaless](https://github.com/taosdata/TDengine/tree/develop/examples/C%23/schemaless)                           | 使用 C# Driver 使用schemaless 的示例 |
| [benchmark](https://github.com/taosdata/TDengine/tree/develop/examples/C%23/taosdemo)                               | C# 版简易benchmark |
| [async   query](https://github.com/taosdata/TDengine/blob/develop/src/connector/C%23/examples/QueryAsyncSample.cs) | 使用 C# Driver 使用异步查询的示例 |
| [subscribe](https://github.com/taosdata/TDengine/blob/develop/src/connector/C%23/examples/SubscribeSample.cs)      | 使用 C# Driver 使用订阅示例 |

## 重要更新记录（optional）

| TDengine.Connector | 说明                           |
|--------------------|--------------------------------|
|        1.0.2       | 新增连接管理、同步查询、错误信息。   |
|        1.0.3       | 新增参数绑定、schemaless、 json tag。 |
|        1.0.4       | 新增异步查询，订阅。修复绑定参数。    |
|        1.0.5       | 修复 windows 同步查询中文报错。   |
|        1.0.6       | 修复 schemaless 在1.0.4 和 1.0.5中失效的问题。 |

## 其他说明

### 第三方驱动

Maikebing.Data.Taos 是一个 TDengine 的 ADO.NET 提供器，支持 Linux，Windows。该开发包由热心贡献者`麦壳饼@@maikebing`提供，具体请参考:

* 接口下载:<https://github.com/maikebing/Maikebing.EntityFrameworkCore.Taos>
* 用法说明:<https://www.taosdata.com/blog/2020/11/02/1901.html>

## 常见问题

* "Unable to establish connection"，"Unable to resolve FQDN"， 一般都是应为为配置 FQDN 可以参考[如何彻底搞懂 TDengine 的 FQDN](https://www.taosdata.com/blog/2021/07/29/2741.html)

* java.lang.UnsatisfiedLinkError: no taos in java.library.path 原因：程序没有找到依赖的本地函数库 taos。 解决方法：Windows 下可以将 C:\TDengine\driver\taos.dll 拷贝到 C:\Windows\System32\ 目录下，Linux 下将建立如下软链 ln -s /usr/local/taos/driver/libtaos.so.x.x.x.x /usr/lib/libtaos.so 即可。

* 其它问题请参考 [Issues](https://github.com/taosdata/TDengine/issues)
  
## API 参考