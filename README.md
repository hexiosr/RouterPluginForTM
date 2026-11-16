# 路由器全网速率（TrafficMonitor）

通过路由器 UPnP 的 WAN 字节计数器，把全网上下行总速率显示在 TrafficMonitor 任务栏。

## 功能

- 「全网速率 ↑ / ↓」两项：直接显示 `↑: 速率` / `↓: 速率`；TrafficMonitor 会把这两项叠成任务栏的两行
- 速率来自路由器 WAN 计数器的字节差：每秒采样一次，按 1 秒窗口计算
- 自动通过 SSDP 发现路由器，不需要填写地址、账号或密码
- 宽度预留可选「自动」或 1–4 位；固定预留时数值变化不会让整行左右抖动
- 悬停显示：连接状态、更新时间、上行/下行

## 安装

1. 把 `RouterTotalSpeed.dll` 复制到 TrafficMonitor 的 `plugins` 目录
2. 重启 TrafficMonitor
3. 右键任务栏 → 选项 → 插件项目，勾选「全网速率 ↑」「全网速率 ↓」两项

TrafficMonitor 1.86 的任务栏两行排列是「两项一组」：勾选的两项在显示顺序里相邻时，才会上下叠在同一列。
如果它们没有对齐，到「显示设置 → 设置项顺序」里把这两项拖到一起即可。

## 配置

右键任务栏 → 插件 → 路由器全网速率 → 选项：

- 显示单位：数值后是否带 `MB/s` / `Mbps`
- 单位：`MB/s` 或 `Mbps`（1 MB/s = 8 Mbps）
- 小数位：0 / 1 / 2 位
- 宽度预留：自动或预留 1–4 位整数；数值超出预留宽度时自动放宽，不会截断

设置保存在插件目录下 `RouterTotalSpeed.ini` 的 `[Display]` 段。

## 显示含义

| 项目 | 含义 |
| --- | --- |
| `↑` | 路由器 WAN 口的上行总速率 |
| `↓` | 路由器 WAN 口的下行总速率 |
| `查找路由器` | 启动后正在通过 SSDP 寻找路由器 |
| `未发现路由器` | 没找到支持 UPnP 的 IGD，请确认路由器已开启 UPnP |

只统计经过 WAN 口的流量，局域网内设备之间互传不会计入。

## 适配其他路由器

只要路由器支持 UPnP 且实现 `WANCommonInterfaceConfig` 计数器即可，无需额外配置；发现是自动的。
UPnP 关闭的固件会在界面显示「未发现路由器」。

## 构建

需要 MinGW-w64（x64）：

```bat
build_mingw.bat
```

产物：`bin\Release\x64\RouterTotalSpeed.dll`（静态链接，无运行库依赖）。

## 目录

```
src/core   插件主类、显示绘制、显示设置
src/router UPnP 发现与 WAN 计数器采样
src/net    WinHTTP、UTF-8 转换
src/ui     设置对话框、图标资源
tools      开发用冒烟测试（不参与构建）
```
