#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>

#include "touch_listen.h"


// 全局变量
struct touch_device *devices = NULL;
int device_count = 0;
touch_event_callback event_callback = NULL;
static volatile int listener_running = 0;




// 计算字符串的简单哈希值，用于设备标识
int hash_string(const char *str) {
    int hash = 0;
    while (*str) {
        hash = hash * 31 + *str++;
    }
    return hash & 0x7FFFFFFF; // 确保是正数
}

// 检查设备是否是触摸屏
bool is_touch_device(const char *device_path) {
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        return false;
    }
    
    unsigned long evbit = 0;
    ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit);
    
    // 检查设备是否支持绝对坐标和触摸事件
    if (!(evbit & (1 << EV_ABS)) || 
        !(evbit & (1 << EV_KEY))) {
        close(fd);
        return false;
    }
    
    // 检查是否支持ABS_MT_POSITION_X事件（多点触控）
    unsigned long absbit = 0;
    ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), &absbit);
    
    if (absbit & (1 << ABS_MT_POSITION_X)) {
        close(fd);
        return true;
    }
    
    // 检查是否支持ABS_X事件（单点触控）
    if (absbit & (1 << ABS_X)) {
        close(fd);
        return true;
    }
    
    close(fd);
    return false;
}

// 获取设备信息
void get_device_info(struct touch_device *device) {
    int fd = open(device->path, O_RDONLY);
    if (fd < 0) {
        return;
    }
    
    // 获取设备名称
    ioctl(fd, EVIOCGNAME(sizeof(device->name)), device->name);
    
    // 获取X轴范围
    struct input_absinfo absinfo;
    if (ioctl(fd, EVIOCGABS(ABS_X), &absinfo) >= 0) {
        device->min_x = absinfo.minimum;
        device->max_x = absinfo.maximum;
    }
    
    // 获取Y轴范围
    if (ioctl(fd, EVIOCGABS(ABS_Y), &absinfo) >= 0) {
        device->min_y = absinfo.minimum;
        device->max_y = absinfo.maximum;
    }
    
    close(fd);
}

// 发现所有触摸屏设备
int discover_touch_devices() {
    DIR *dir;
    struct dirent *entry;
    char path[256];
    
    // 释放之前分配的内存
    if (devices != NULL) {
        for (int i = 0; i < device_count; i++) {
            if (devices[i].fd >= 0) {
                close(devices[i].fd);
            }
        }
        free(devices);
        devices = NULL;
        device_count = 0;
    }
    
    // 打开输入设备目录
    dir = opendir("/dev/input");
    if (!dir) {
        perror("无法打开 /dev/input");
        return -1;
    }
    
    // 第一次遍历，计算设备数量
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
            if (is_touch_device(path)) {
                device_count++;
            }
        }
    }
    
    rewinddir(dir);
    
    // 分配设备数组内存
    devices = malloc(device_count * sizeof(struct touch_device));
    if (!devices) {
        closedir(dir);
        return -1;
    }
    
    // 第二次遍历，填充设备信息
    int index = 0;
    while ((entry = readdir(dir)) != NULL && index < device_count) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
            if (is_touch_device(path)) {
                strncpy(devices[index].path, path, sizeof(devices[index].path));
                get_device_info(&devices[index]);
                devices[index].fd = -1; // 初始化为未打开状态
                index++;
            }
        }
    }
    
    closedir(dir);
    return device_count;
}

// 打开所有触摸屏设备
int open_touch_devices() {
    for (int i = 0; i < device_count; i++) {
        devices[i].fd = open(devices[i].path, O_RDONLY | O_NONBLOCK);
        if (devices[i].fd < 0) {
            perror("无法打开设备");
            return -1;
        }
    }
    return 0;
}

// 关闭所有触摸屏设备
void close_touch_devices() {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].fd >= 0) {
            close(devices[i].fd);
            devices[i].fd = -1;
        }
    }
}

// 处理输入事件
void process_input_event(struct input_event *ev, struct touch_device *device, 
                         struct touch_event *touch_ev) {
    static int x = 0, y = 0, pressure = 0, touch_id = 0;
    
    touch_ev->device_id = hash_string(device->path);
    strncpy(touch_ev->device_name, device->name, sizeof(touch_ev->device_name));
    strncpy(touch_ev->device_path, device->path, sizeof(touch_ev->device_path));
    
    // 获取当前时间戳
    clock_gettime(CLOCK_MONOTONIC, &touch_ev->timestamp);
    
    switch (ev->type) {
        case EV_ABS:
            switch (ev->code) {
                case ABS_X:
                case ABS_MT_POSITION_X:
                    x = ev->value;
                    // 转换为实际坐标（如果需要）
                    if (device->max_x > device->min_x) {
                        touch_ev->x = x;
                    }
                    break;
                case ABS_Y:
                case ABS_MT_POSITION_Y:
                    y = ev->value;
                    // 转换为实际坐标（如果需要）
                    if (device->max_y > device->min_y) {
                        touch_ev->y = y;
                    }
                    break;
                case ABS_PRESSURE:
                case ABS_MT_PRESSURE:
                    pressure = ev->value;
                    touch_ev->pressure = pressure;
                    break;
                case ABS_MT_TRACKING_ID:
                    touch_id = ev->value;
                    touch_ev->touch_id = touch_id;
                    if (touch_id == -1) {
                        // 触摸释放
                        touch_ev->event_type = 2; // 释放
                        if (event_callback) {
                            event_callback(*touch_ev);
                        }
                    } else {
                        // 新触摸点
                        touch_ev->event_type = 0; // 按下
                        if (event_callback) {
                            event_callback(*touch_ev);
                        }
                    }
                    break;
            }
            break;
        case EV_KEY:
            if (ev->code == BTN_TOUCH) {
                if (ev->value) {
                    touch_ev->event_type = 0; // 按下
                } else {
                    touch_ev->event_type = 2; // 释放
                }
                if (event_callback) {
                    event_callback(*touch_ev);
                }
            }
            break;
        case EV_SYN:
            if (ev->code == SYN_REPORT) {
                // 报告同步事件，表示一组事件完成
                touch_ev->event_type = 1; // 移动
                touch_ev->x = x;
                touch_ev->y = y;
                touch_ev->pressure = pressure;
                touch_ev->touch_id = touch_id;
                if (event_callback && (x != 0 || y != 0)) {
                    event_callback(*touch_ev);
                }
            }
            break;
    }
}

// 监听触摸事件
void listen_touch_events() {
    fd_set fds;
    int max_fd = 0;
    struct input_event ev;
    struct touch_event touch_ev;

    listener_running = 1; // 设置运行标志
    
    while (listener_running) {
        FD_ZERO(&fds);
        max_fd = 0;
        
        // 设置文件描述符集合
        for (int i = 0; i < device_count; i++) {
            if (devices[i].fd >= 0) {
                FD_SET(devices[i].fd, &fds);
                if (devices[i].fd > max_fd) {
                    max_fd = devices[i].fd;
                }
            }
        }

        if(listener_running == 0) break;

        if (max_fd == 0) {
            usleep(100000); // 如果没有设备，短暂睡眠
            continue;
        }

        if(listener_running == 0) break;
        
        // 使用select等待事件
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ret = select(max_fd + 1, &fds, NULL, NULL, &timeout);
        if (ret < 0) {
            perror("select错误");
            break;
        }

        if(listener_running == 0) break;
        
        // 检查每个设备是否有事件
        for (int i = 0; i < device_count; i++) {
            if (devices[i].fd >= 0 && FD_ISSET(devices[i].fd, &fds)) {
                // 读取所有可用事件
                while (read(devices[i].fd, &ev, sizeof(ev)) == sizeof(ev)) {
                    process_input_event(&ev, &devices[i], &touch_ev);
                }
            }
        }
    }
    printf("listen_touch_events 成功退出\n");
}


// 添加停止监听函数
void stop_touch_listener() {
    listener_running = 0; // 设置停止标志
}

// 格式化时间戳为可读字符串
void format_timestamp(struct timespec ts, char *buffer, size_t buffer_size) {
    time_t sec = ts.tv_sec;
    struct tm *tm_info = localtime(&sec);
    strftime(buffer, buffer_size, "%H:%M:%S", tm_info);
    
    // 添加毫秒部分
    char ms_buffer[10];
    snprintf(ms_buffer, sizeof(ms_buffer), ".%03ld", ts.tv_nsec / 1000000);
    strncat(buffer, ms_buffer, buffer_size - strlen(buffer) - 1);
}

// 示例回调函数
void print_touch_event(struct touch_event event) {
    const char *event_types[] = {"按下", "移动", "释放"};
    char timestamp[32];
    
    format_timestamp(event.timestamp, timestamp, sizeof(timestamp));
    
    printf("时间: %s\n", timestamp);
    printf("设备: %s\n", event.device_name);
    printf("路径: %s\n", event.device_path);
    printf("设备ID: %d\n", event.device_id);
    printf("事件: %s\n", event_types[event.event_type]);
    printf("坐标: (%d, %d)\n", event.x, event.y);
    printf("压力: %d\n", event.pressure);
    if (event.touch_id >= 0) {
        printf("触摸点ID: %d\n", event.touch_id);
    }
    printf("---\n");
}

// 初始化触摸事件监听
int init_touch_listener(touch_event_callback callback) {
    event_callback = callback;
    
    // 发现触摸设备
    if (discover_touch_devices() <= 0) {
        fprintf(stderr, "未找到触摸屏设备\n");
        return -1;
    }
    
    printf("找到 %d 个触摸屏设备:\n", device_count);
    for (int i = 0; i < device_count; i++) {
        printf("%d: %s (%s)\n", i, devices[i].name, devices[i].path);
    }
    
    // 打开设备
    if (open_touch_devices() < 0) {
        fprintf(stderr, "无法打开触摸设备\n");
        return -1;
    }
    
    return 0;
}

// 清理资源
void cleanup_touch_listener() {
    close_touch_devices();
    if (devices != NULL) {
        free(devices);
        devices = NULL;
    }
    device_count = 0;
}

// 获取当前活动的触摸设备列表
int get_active_touch_devices(struct touch_device **active_devices) {
    if (device_count <= 0) {
        return 0;
    }
    
    *active_devices = malloc(device_count * sizeof(struct touch_device));
    if (!*active_devices) {
        return -1;
    }
    
    memcpy(*active_devices, devices, device_count * sizeof(struct touch_device));
    return device_count;
}

// // 主函数示例
// int main() {
//     // 初始化触摸监听器
//     if (init_touch_listener(print_touch_event) < 0) {
//         return 1;
//     }
    
//     printf("开始监听触摸事件...\n");
//     printf("按Ctrl+C退出\n\n");
    
//     // 开始监听事件
//     listen_touch_events();
    
//     // 清理资源
//     cleanup_touch_listener();
    
//     return 0;
// }