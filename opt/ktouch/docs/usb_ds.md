# usb_ds.c - USB触摸屏设备热插拔监控程序

## 概述
通过udev监控USB设备的热插拔事件，当检测到新的触摸屏设备插入时，通过控制文件通知外部程序。支持轮询间隔配置和控制文件暂停机制。

## 程序结构

### 配置常量
```c
#define DEFAULT_POLL_INTERVAL 5   // 默认轮询间隔(秒)
#define MAX_RETRY_ATTEMPTS 3      // 最大重试次数
#define RETRY_DELAY 1             // 重试延迟(秒)
```

### 控制变量
```c
volatile sig_atomic_t keep_running = 1;  // 运行标志
```

## 工作原理

### 监控流程
```
创建udev监控对象
    ↓
过滤USB和input子系统事件
    ↓
主循环:
    ├── 检查控制文件状态
    │   └── 若非0，跳过本轮
    ├── select等待事件(超时=轮询间隔)
    ├── 有事件:
    │   ├── 获取设备信息
    │   ├── 检查是否为"add"动作
    │   ├── 检查是否为指针设备
    │   └── 检查是否为触摸屏
    └── 更新控制文件状态
```

### 触摸屏判断
```c
int is_touchscreen(struct udev_device *dev) {
    // 1. 检查ID_INPUT_TOUCHSCREEN属性
    // 2. 检查设备名称是否包含"touch"/"touchscreen"/"tablet"
}
```

### 控制文件机制
- `0`: 正常监控状态
- `1`: 检测到触摸屏，等待外部处理后恢复为`0`

### 文件操作重试
所有文件读写操作最多重试3次，每次间隔1秒。

## 关键函数

| 函数 | 功能 |
|------|------|
| `monitor_usb_devices()` | 主监控循环 |
| `is_touchscreen()` | 判断设备是否为触摸屏 |
| `read_file_content_with_retry()` | 带重试的文件读取 |
| `write_file_content_with_retry()` | 带重试的文件写入 |
| `sigint_handler()` | SIGINT信号处理 |

## 调用方式
```bash
./usb_ds [控制文件路径] [轮询间隔秒数]
# 例: ./usb_ds /tmp/usb_monitor.ctl 5
# 不带参数: 无控制文件，5秒轮询
```

## 使用场景
当USB触摸屏热插拔时，通过控制文件通知主程序重新配置触摸映射。

## 依赖
- libudev
- POSIX信号处理
