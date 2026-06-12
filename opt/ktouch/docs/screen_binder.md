# screen_binder.c - 触摸屏与显示器绑定程序

## 概述
核心模块之一，用于将触摸屏设备与显示器进行一一绑定。程序在每个显示器上依次弹出校准窗口，用户点击触摸屏后，程序记录触摸设备与当前显示器的对应关系，最终保存到`touch_dis_table.txt`文件。

## 程序结构

### 数据结构
```c
// 屏幕信息
int *screen_widths, *screen_heights;     // 屏幕宽高
int *screen_x_offsets, *screen_y_offsets; // 屏幕坐标
char **screen_names;                      // 屏幕接口名

// 触摸绑定信息
int *touch_bindings;    // 每个屏幕绑定的触摸设备ID
char **touch_names;     // 触摸设备名称
char **touch_paths;     // 触摸设备路径
```

### 线程模型
| 线程 | 功能 |
|------|------|
| 主线程 | GTK事件循环，窗口管理 |
| `touch_thread` | 触摸事件监听线程 |
| `enter_thread` | 全局回车键监听线程(X11) |

### 原子变量(线程安全)
```c
_Atomic int global_enter_pressed;   // 全局回车键按下标志
_Atomic int program_active;         // 程序活动状态
_Atomic int calibration_active;     // 校准进行中标志
```

## 工作原理

1. **屏幕枚举**: 通过Xrandr获取所有已连接显示器
2. **触摸初始化**: 初始化触摸监听器，启动触摸事件监听线程
3. **全局回车监听**: 启动独立线程监听全局回车键（用于跳过非触摸屏）
4. **信息窗口创建**: 依次为每个非当前校准屏幕创建提示窗口
5. **校准流程**:
   - 在当前屏幕创建全屏校准窗口
   - 等待用户点击触摸屏（60秒超时）
   - 记录触摸设备ID、名称、路径
   - 用户按回车键可跳过当前屏幕
6. **保存结果**: 将绑定关系保存到`touch_dis_table.txt`

## 校准窗口交互
```
┌─────────────────────────────────────┐
│  请在触摸屏上点击此屏幕。            │
│                                     │
│  屏幕接口: HDMI-1                   │
│                                     │
│  如果此屏幕不是触摸屏，              │
│  那么请敲击回车或者等待60秒。        │
└─────────────────────────────────────┘
```

## 关键函数

| 函数 | 功能 |
|------|------|
| `get_screen_info()` | Xrandr获取屏幕信息 |
| `init_touch_listener()` | 初始化触摸监听 |
| `touch_callback()` | 触摸事件回调，记录绑定 |
| `global_enter_listener_thread()` | X11全局回车键监听 |
| `show_next_screen()` | 显示下一个校准屏幕 |
| `save_mapping_table()` | 保存绑定关系到文件 |
| `create_calibration_window()` | 创建校准窗口 |
| `create_info_window()` | 创建信息提示窗口 |

## 输出格式
文件`touch_dis_table.txt`格式：
```
屏幕接口名|触摸设备名|触摸设备ID|设备路径
HDMI-1|USB Touchscreen|12345|/dev/input/event5
```

## 调用方式
```bash
sudo ./screen_binder  # 需要sudo权限访问触摸设备
```

## 依赖
- GTK+ 3.x
- X11 + Xrandr扩展
- touch_listen模块（触摸事件监听）
- 需要root权限访问`/dev/input/`设备
