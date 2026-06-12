# screen_ds.c - 显示器信息持续监控程序

## 概述
一个基于Xrandr的显示器状态持续监控程序。持续监听显示器的连接/断开事件，当检测到变化时（防抖3秒后）输出所有显示器的当前信息。

## 程序结构

### 数据结构
```c
typedef struct {
    char* output_name;  // 输出接口名(如HDMI-1)
    int width, height;  // 分辨率
    int x, y;           // 位置坐标
    int rotation;       // 旋转方向
} MonitorInfo;

typedef struct {
    MonitorInfo* monitors;  // 显示器数组
    int count;              // 显示器数量
} MonitorList;
```

### 旋转方向编码
| 值 | 字母 | 说明 |
|----|------|------|
| RR_Rotate_0 | N | 正常方向 |
| RR_Rotate_90 | L | 左旋转90度 |
| RR_Rotate_180 | I | 翻转180度 |
| RR_Rotate_270 | R | 右旋转90度 |

## 工作原理

1. **初始化**: 打开X显示连接，注册RandR事件监听
2. **首次输出**: 获取并输出当前所有显示器信息
3. **事件循环**:
   - 监听`RRScreenChangeNotify`和`RRNotify`事件
   - 检测到事件后设置`need_update`标志
   - 防抖3秒后（`DEBOUNCE_TIME`）重新获取并输出显示器信息
4. **无事件时**: 休眠100ms减少CPU占用

## 输出格式
```
HDMI-1|1920x1080|0x0|N
DP-2|1920x1080|1920x0|N
```
每行格式: `接口名|分辨率|位置|旋转方向`

## 关键函数

| 函数 | 功能 |
|------|------|
| `get_monitor_info()` | 获取所有连接的显示器信息 |
| `output_monitor_info()` | 输出显示器信息到文件或stdout |
| `rotation_to_char()` | 旋转值转字母表示 |
| `current_timestamp()` | 获取毫秒级时间戳 |

## 调用方式
```bash
./screen_ds [输出文件路径]
# 例: ./screen_ds /tmp/screen.txt
# 不指定参数则输出到标准输出
```

## 特性
- 3秒防抖：避免频繁热插拔导致的重复输出
- 持续运行，直到Ctrl+C退出
- 支持输出到文件或标准输出

## 依赖
- X11 + Xrandr扩展
