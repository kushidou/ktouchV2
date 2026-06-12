# touch_ds.c - 触摸屏坐标矩阵监控程序

## 概述
持续监控所有触摸屏设备的坐标转换矩阵（Coordinate Transformation Matrix）。当检测到某个触摸设备的矩阵发生变化时，设置flag文件通知外部程序。

## 程序结构

### 数据结构
```c
typedef struct {
    XID deviceid;           // XInput设备ID
    char name[100];         // 设备名称
    char node[100];         // 设备节点路径
    char vid_pid[20];       // VID:PID
    char matrix[120];       // 坐标转换矩阵字符串
} TouchscreenInfo;
```

### 配置常量
```c
int watch_time_delay = 30;  // 监控间隔(秒)
```

## 工作原理

1. **读取配置**: 从触摸屏信息文件读取设备列表（含期望的矩阵值）
2. **X11连接**: 打开X显示连接，检查XInput扩展
3. **主循环**:
   - 检查flag文件是否为1（暂停检测）
   - 重新读取触摸屏信息文件
   - 对每个设备获取当前矩阵值
   - 比较矩阵是否与配置值不同
   - 若变化，设置flag文件为1
   - 等待30秒后继续

### 矩阵获取流程
```
设备 → XInput属性查询 → "Coordinate Transformation Matrix"
                       → 或 "libinput Calibration Matrix"
                       → 返回3x3浮点矩阵
```

## 关键函数

| 函数 | 功能 |
|------|------|
| `get_device_matrix_property()` | 获取设备的3x3矩阵属性 |
| `get_matrix_string()` | 获取矩阵的字符串表示 |
| `device_exists()` | 检查设备是否仍然存在 |
| `is_touchscreen_device()` | 通过udev判断是否为触摸屏 |
| `read_touchscreens_from_file()` | 读取触摸屏配置文件 |
| `is_flag_set()` / `set_flag()` | flag文件读写 |

## 输入文件格式
触摸屏信息文件每行格式：
```
设备ID|设备名|设备节点|VID:PID|矩阵值
12345|USB Touchscreen|/dev/input/event5|046d:c52b|1.000000,0.000000,...
```

## flag文件机制
- flag文件值为`0`：正常监控
- flag文件值为`1`：暂停监控，等待外部恢复为`0`

## 调用方式
```bash
./touch_ds <触摸屏信息文件> [flag文件路径]
# 例: ./touch_ds /tmp/touch_info.txt /tmp/touch_matrix.flag
```

## 依赖
- X11 + XInput2扩展
- libudev
