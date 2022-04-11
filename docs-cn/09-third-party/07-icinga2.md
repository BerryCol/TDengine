---
sidebar_label: icinga2
title: icinga2 写入
---

安装 icinga2
请参考[官方文档](https://icinga.com/docs/icinga-2/latest/doc/02-installation/)

TDengine 新版本（2.3.0.0+）包含一个 taosAdapter 独立程序，负责接收包括 icinga2 的多种应用的数据写入。

icinga2 可以收集监控和性能数据并写入 OpenTSDB，taosAdapter 可以支持接收 icinga2 的数据并写入到 TDengine 中。

- 参考链接 `https://icinga.com/docs/icinga-2/latest/doc/14-features/#opentsdb-writer` 使能 opentsdb-writer
- 使能 taosAdapter 配置项 opentsdb_telnet.enable
- 修改配置文件 /etc/icinga2/features-enabled/opentsdb.conf

```
object OpenTsdbWriter "opentsdb" {
  host = "host to taosAdapter"
  port = 6048
}
```

taosAdapter 相关配置参数请参考 `taosadapter --help` 命令输出以及相关文档。
