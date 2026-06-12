# touch_set.c - 触摸屏坐标转换矩阵设置程序

## 概述
核心模块之一，负责根据配置文件和屏幕信息，自动计算并设置每个触摸屏设备的坐标转换矩阵（Coordinate Transformation Matrix），实现触摸屏与显示器的正确映射。

## 程序结构

### 数据结构
```c
// 设备信息
typedef struct {
    int device_id;          // XInput设备ID
    char name[256];         // 设备名称
    char device_node[256];  // 设备节点路径
    char vid[10];           // 供应商ID
    char pid[10];           // 产品ID
    char usb_path[100];     // 物理路径
    int is_touchscreen;     // 是否为触摸屏
    float matrix[9];        // 3x3坐标转换矩阵
    int matched;            // 是否已匹配
} InputDeviceInfo;

// 配置项
typedef struct {
    char display_name[50];      // 显示器名称
    char touchscreen_name[50];  // 触摸屏名称
    char vid[10];               // 供应商ID
    char pid[10];               // 产品ID
    char usb_path[100];         // USB路径
    int matched;                // 是否已匹配
} TouchConfig;

// 屏幕信息
typedef struct {
    char name[50];      // 屏幕名称
    int width, height;  // 分辨率
    int x, y;           // 位置
    char rotation;      // 旋转: N/L/R/I
} ScreenInfo;
```

## 工作原理

### 整体流程
```
读取配置文件(/opt/ktouch/config)
         ↓
读取屏幕信息(/tmp/ktouch/screen.txt)
         ↓
计算虚拟桌面大小
         ↓
获取所有触摸设备(XInput + udev)
         ↓
多轮匹配(严格→宽松)
         ↓
计算坐标转换矩阵
         ↓
通过XInput设置矩阵
         ↓
保存结果到输出文件
```

### 矩阵计算原理

坐标转换矩阵将触摸屏的原始坐标映射到虚拟桌面坐标系：

**正常方向(N)**:
```
| sx  0   dx |
| 0   sy  dy |
| 0   0   1  |
```

**左旋转(L)**:
```
| 0  -sx  sx+dx |
| sy  0   dy    |
| 0   0   1     |
```

**右旋转(R)**:
```
| 0   sx  dx    |
| -sy 0   sy+dy |
| 0   0   1     |
```

**翻转(I)**:
```
| -sx 0   sx+dx |
| 0  -sy  sy+dy |
| 0   0   1     |
```

其中：
- `sx = 屏幕宽度 / 虚拟桌面宽度`
- `sy = 屏幕高度 / 虚拟桌面高度`
- `dx = 屏幕X偏移 / 虚拟桌面宽度`
- `dy = 屏幕Y偏移 / 虚拟桌面高度`

### 设备匹配策略

**第一轮(严格匹配)**:
1. 设备名完全匹配（去除空格后比较）
2. 若多个匹配，按VID:PID筛选
3. 若仍多个，按USB路径筛选

**第二轮及以后(宽松匹配)**:
1. 仅按VID:PID匹配
2. 若多个匹配，返回失败(-1)

## 关键函数

| 函数 | 功能 |
|------|------|
| `get_input_devices()` | 通过XInput+udev获取所有触摸设备 |
| `read_config()` | 读取触摸屏配置文件 |
| `read_screen_info()` | 读取屏幕信息文件 |
| `find_matching_device()` | 多策略设备匹配 |
| `calculate_virtual_desktop()` | 计算虚拟桌面总尺寸 |
| `calculate_ctm()` | 计算坐标转换矩阵 |
| `set_ctm()` | 通过XInput设置矩阵属性 |
| `devnode_to_syspath()` | 设备节点转sysfs路径 |

## 输入文件格式

### 配置文件(/opt/ktouch/config)
```
显示器名|触摸屏名|VID|PID|USB路径
HDMI-1|USB Touchscreen|046d|c52b|usb-0000:00:14.0-1
```

### 屏幕信息文件(/tmp/ktouch/screen.txt)
```
HDMI-1|1920x1080|0x0|N
DP-2|1920x1080|1920x0|N
```

### 输出文件格式
```
设备ID|设备名|设备节点|VID:PID|矩阵值
12345|USB Touchscreen|/dev/input/event5|046d:c52b|1.000000, 0.000000, ...
```

## 调用方式
```bash
./touch_set [输出文件路径]
```

## 依赖
- X11 + XInput2扩展
- libudev
