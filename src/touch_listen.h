
#ifndef TOUCH_LISTEN
#define TOUCH_LISTEN

// 触摸屏设备信息结构体
struct touch_device {
    int fd;
    char name[256];
    char path[256];
    int min_x, max_x;
    int min_y, max_y;
};

// 触摸事件信息结构体
struct touch_event {
    int device_id;          // 设备标识（可以使用设备路径的哈希值）
    int x;                  // X坐标
    int y;                  // Y坐标
    int pressure;           // 压力值
    int touch_id;           // 触摸点ID（用于多点触控）
    int event_type;         // 事件类型: 按下、移动、释放
    char device_name[256];  // 设备名称
    char device_path[256];  // 设备路径（如/dev/input/event1）
    struct timespec timestamp; // 事件时间戳
};


// 回调函数类型定义
typedef void (*touch_event_callback)(struct touch_event event);

// 监听触摸事件
void listen_touch_events();

// 初始化触摸事件监听
int init_touch_listener(touch_event_callback callback);

void stop_touch_listener();

#endif // !TOUCH_LISTEN