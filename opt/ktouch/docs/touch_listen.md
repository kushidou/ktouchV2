# touch_listen.c / touch_listen.h - 触摸事件监听库

## 概述
一个触摸屏事件监听库，通过直接读取Linux输入设备（`/dev/input/event*`）获取触摸事件。支持多点触控，提供回调函数接口供上层模块使用。

## 数据结构

### touch_device (设备信息)
```c
struct touch_device {
    int fd;              // 文件描述符
    char name[256];      // 设备名称
    char path[256];      // 设备路径
    int min_x, max_x;    // X轴范围
    int min_y, max_y;    // Y轴范围
};
```

### touch_event (触摸事件)
```c
struct touch_event {
    int device_id;              // 设备标识(路径哈希值)
    int x, y;                   // 坐标
    int pressure;               // 压力值
    int touch_id;               // 触摸点ID(多点触控)
    int event_type;             // 事件类型: 0=按下, 1=移动, 2=释放
    char device_name[256];      // 设备名称
    char device_path[256];      // 设备路径
    struct timespec timestamp;  // 时间戳
};
```

### 回调函数类型
```c
typedef void (*touch_event_callback)(struct touch_event event);
```

## 工作原理

### 设备发现
1. 遍历`/dev/input/`目录下所有`event*`设备
2. 通过ioctl检查设备是否支持`EV_ABS`和`EV_KEY`
3. 进一步检查是否支持`ABS_MT_POSITION_X`(多点)或`ABS_X`(单点)
4. 获取设备名称和坐标范围

### 事件监听
1. 使用`select()`多路复用监听所有设备文件描述符
2. 读取`input_event`结构体
3. 处理事件类型：
   - `EV_ABS`: 处理坐标、压力、触摸点ID
   - `EV_KEY`: 处理`BTN_TOUCH`按键事件
   - `EV_SYN`: 同步事件，触发回调
4. 通过回调函数通知上层

### 事件类型映射
| Linux事件 | 触摸事件类型 | 说明 |
|-----------|-------------|------|
| ABS_MT_TRACKING_ID != -1 | 0 (按下) | 新触摸点 |
| ABS_MT_TRACKING_ID == -1 | 2 (释放) | 触摸释放 |
| BTN_TOUCH = 1 | 0 (按下) | 触摸开始 |
| BTN_TOUCH = 0 | 2 (释放) | 触摸结束 |
| SYN_REPORT | 1 (移动) | 坐标更新 |

## API接口

| 函数 | 功能 |
|------|------|
| `init_touch_listener(callback)` | 初始化监听器，注册回调 |
| `listen_touch_events()` | 开始监听(阻塞) |
| `stop_touch_listener()` | 停止监听 |
| `cleanup_touch_listener()` | 清理资源 |
| `get_active_touch_devices()` | 获取活跃设备列表 |

## 调用示例
```c
#include "touch_listen.h"

void my_callback(struct touch_event event) {
    printf("设备 %s: %s at (%d,%d)\n", 
           event.device_name,
           event.event_type == 0 ? "按下" : 
           event.event_type == 1 ? "移动" : "释放",
           event.x, event.y);
}

int main() {
    init_touch_listener(my_callback);
    listen_touch_events();  // 阻塞
    cleanup_touch_listener();
    return 0;
}
```

## 使用者
- `screen_binder.c` - 触摸屏绑定程序

## 依赖
- Linux输入子系统(`/dev/input/event*`)
- 需要root权限访问输入设备
