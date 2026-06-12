#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libudev.h>
#include <X11/Xatom.h> // 为XA_STRING, XA_FLOAT等预定义原子
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_LINE_LENGTH 256
#define MAX_DEVICES 50

// 全局日志文件指针
FILE *log_file = NULL;

// 设备信息结构体
typedef struct {
    int device_id;                  // 设备ID
    char name[256];                     // 设备名称
    char device_node[256];              // 设备节点路径
    char vid[10];                // 供应商ID
    char pid[10];                 // 模型ID
    char usb_path[100];            // 物理路径
    int is_touchscreen;             // 是否是触摸屏设备标志
    float matrix[9];
    int match_success;
} InputDeviceInfo;

// 配置结构体
typedef struct {
    char display_name[50];
    char touchscreen_name[50];
    char vid[10];
    char pid[10];
    char usb_path[100];
} TouchConfig;

// 屏幕信息结构体
typedef struct {
    char name[50];
    int width;
    int height;
    int x;
    int y;
} ScreenInfo;



// 函数声明
void log_message(const char* format, ...);
int read_config(const char* filename, TouchConfig configs[], int max_configs);
int read_screen_info(const char* filename, ScreenInfo screens[], int max_screens);
int find_matching_device(TouchConfig config, InputDeviceInfo devices[], int device_count);
int parse_resolution_and_position(const char* str, int* width, int* height, int* x, int* y);
void calculate_virtual_desktop(ScreenInfo screens[], int screen_count, int *total_width, int *total_height);
void calculate_ctm(ScreenInfo screen, int total_width, int total_height, float matrix[9]);
int set_ctm(int device_id, float matrix[9]);


// 日志函数
void log_message(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    // 获取当前时间
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // 输出到控制台
    printf("[%s] touch_set \t", time_str);
    vprintf(format, args);
    
    // 输出到文件
    if (log_file) {
        fprintf(log_file, "[%s] touch_set \t", time_str);
        vfprintf(log_file, format, args);
        fflush(log_file); // 确保立即写入文件
    }
    
    va_end(args);
}

// 将设备节点路径转换为 sysfs 路径
char* devnode_to_syspath(const char* devnode) {
    struct stat st;
    if (stat(devnode, &st) != 0) {
        return NULL;
    }
    
    // 获取主设备号和次设备号
    unsigned int major = major(st.st_rdev);
    unsigned int minor = minor(st.st_rdev);
    
    // 构建 sysfs 路径
    char* syspath = malloc(256);
    snprintf(syspath, 256, "/sys/dev/char/%u:%u", major, minor);
    
    return syspath;
}



// 获取输入设备信息的主要函数
int get_input_devices(InputDeviceInfo result[]) {
    Display *display;
    int ndevices;
    XIDeviceInfo *devices;
    struct udev *udev_ctx;
    int count = 0;
    
    // 打开X11显示连接
    display = XOpenDisplay(NULL);
    if (!display) {
        log_message("无法打开X显示连接\n");
        return 0;
    }
    
    // 初始化udev上下文
    udev_ctx = udev_new();
    if (!udev_ctx) {
        log_message("无法创建udev上下文\n");
        XCloseDisplay(display);
        return 0;
    }
    
    // 获取X输入设备列表
    devices = XIQueryDevice(display, XIAllDevices, &ndevices);
    if (!devices) {
        log_message( "无法查询X输入设备\n");
        udev_unref(udev_ctx);
        XCloseDisplay(display);
        return 0;
    }
    

    
    // 遍历所有设备
    for (int i = 0; i < ndevices; i++) {
        XIDeviceInfo *dev = &devices[i];
        
        // 只关注从设备(slave devices)，排除主设备和虚拟设备
        // if (dev->use != XISlavePointer && dev->use != XISlaveKeyboard) {
        //     continue;
        // }
        if(dev -> use != 3) continue;
        
        char *device_node = NULL;
        
        
        // 获取设备属性
        int num_props = 0;
        Atom *props = XIListProperties(display, dev->deviceid, &num_props);
        // log_message("正在处理设备%d:%s(use=%d)\n", dev->deviceid, dev->name, dev->use);
        if (props) {
            for (int j = 0; j < num_props; j++) {
                Atom prop = props[j];
                char *prop_name = XGetAtomName(display, prop);
                
                if (strcmp(prop_name, "Device Node") == 0) {
                    // 获取设备节点路径
                    Atom actual_type;
                    int actual_format;
                    unsigned long nitems, bytes_after;
                    unsigned char *prop_data = NULL;
                    
                    if (XIGetProperty(display, dev->deviceid, prop, 0, 100, False,
                                      AnyPropertyType, &actual_type, &actual_format,
                                      &nitems, &bytes_after, &prop_data) == Success) {
                        // 使用 XInternAtom 获取字符串原子类型
                        Atom string_atom = XInternAtom(display, "STRING", False);
                        if (actual_type == string_atom && actual_format == 8) {
                            device_node = strdup((char*)prop_data);
                        }
                        // log_message("   |- Device Node = %s\n", device_node);
                        XFree(prop_data);
                    }
                }
                
                XFree(prop_name);
            }
            XFree(props);
        }


        
        // 如果找到设备节点，通过udev查询更多信息
        char* syspath = devnode_to_syspath(device_node);
        // log_message("Device syspath = %s\n", syspath);
        
        struct udev_device *udev_dev =  udev_device_new_from_syspath(udev_ctx, syspath);
        
        
        // const char *name = udev_device_get_property_value(udev_dev, "NAME");
        // 获取供应商ID
        const char *vendor_id = udev_device_get_property_value(udev_dev, "ID_VENDOR_ID");
        // 获取模型ID
        const char *model_id = udev_device_get_property_value(udev_dev, "ID_MODEL_ID");
        // 获取物理路径
        const char *physical_path = udev_device_get_property_value(udev_dev, "ID_PATH");
        // 检查是否是触摸屏设备
        const char *is_touchscreen = udev_device_get_property_value(udev_dev, "ID_INPUT_TOUCHSCREEN");

        // log_message("    |- Device info: vid=%s, pid=%s, path=%s, touch=%s\n", vendor_id, model_id, physical_path, is_touchscreen);
        
        
        
        

        if (is_touchscreen){
            // log_message("识别到触摸设备，添加到列表中\n    name=%s", name);
            // 初始化当前设备信息结构体
            result[count].device_id = dev->deviceid;
            result[count].match_success = 0;
            strncpy(result[count].name, dev->name, sizeof(result[count].name));
            strncpy(result[count].vid, vendor_id, sizeof(result[count].vid));
            strncpy(result[count].pid, model_id, sizeof(result[count].pid));
            strncpy(result[count].usb_path, physical_path, sizeof(result[count].usb_path));
            strncpy(result[count].device_node, device_node, sizeof(result[count].device_node));
            for(int i=0;i<9;i++){
                result[count].matrix[i] = 0;
            }
            // result[count].device_node = NULL;
            // result[count].name = strdup(dev->name);
            // result[count].device_node = strdup(device_node);
            // result[count].vid = strdup(vendor_id);
            // result[count].pid = strdup(model_id);
            // result[count].usb_path = strdup(physical_path);
            result[count].is_touchscreen = 1;
            log_message("发现触摸屏设备 %s(id=%d), %s:%s, path=%s\n",result[count].name, result[count].device_id, 
                                                            result[count].vid, result[count].pid,result[count].usb_path);
            
            count++;
            
        }
        udev_device_unref(udev_dev);

        
    }
    
    // 释放资源
    XIFreeDeviceInfo(devices);
    udev_unref(udev_ctx);
    XCloseDisplay(display);
    
    return count;
}

// 读取配置文件
int read_config(const char* filename, TouchConfig configs[], int max_configs) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        log_message("无法打开配置文件: %s\n", filename);
        return -1;
    }
    
    char line[MAX_LINE_LENGTH];
    int count = 0;
    
    while (fgets(line, sizeof(line), file) && count < max_configs) {
        // 移除换行符
        line[strcspn(line, "\n")] = 0;
        
        // 跳过空行和注释
        if (line[0] == '\0' || line[0] == '#') continue;
        
        // 解析行
        char* tokens[5];
        char* token = strtok(line, "|");
        int i = 0;
        
        while (token && i < 5) {
            tokens[i++] = token;
            token = strtok(NULL, "|");
        }
        
        if (i == 5) {
            strncpy(configs[count].display_name, tokens[0], sizeof(configs[count].display_name));
            strncpy(configs[count].touchscreen_name, tokens[1], sizeof(configs[count].touchscreen_name));
            strncpy(configs[count].vid, tokens[2], sizeof(configs[count].vid));
            strncpy(configs[count].pid, tokens[3], sizeof(configs[count].pid));
            strncpy(configs[count].usb_path, tokens[4], sizeof(configs[count].usb_path));
            count++;
        } else {
            log_message("无效的配置行: %s\n", line);
        }
    }
    
    fclose(file);
    return count;
}

// 读取屏幕信息
int read_screen_info(const char* filename, ScreenInfo screens[], int max_screens) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        log_message("无法打开屏幕信息文件: %s\n", filename);
        return -1;
    }
    
    char line[MAX_LINE_LENGTH];
    int count = 0;
    
    while (fgets(line, sizeof(line), file) && count < max_screens) {
        // 移除换行符
        line[strcspn(line, "\n")] = 0;
        
        // 跳过空行和注释
        if (line[0] == '\0' || line[0] == '#') continue;
        
        // 解析显示器名
        char* name = strtok(line, "|");
        if (!name) continue;
        
        // 解析分辨率
        char* resolution = strtok(NULL, "|");
        if (!resolution) continue;
        
        // 解析坐标
        char* position = strtok(NULL, "|");
        if (!position) continue;
        
        int width, height, x, y;
        // 解析分辨率 (格式: 1920x1080)
        if (sscanf(resolution, "%dx%d", &width, &height) != 2) {
            log_message("无效的分辨率格式: %s\n", resolution);
            continue;
        }
        
        // 解析坐标 (格式: 1920x0 或 1920,0)
        if (sscanf(position, "%d%*[x,]%d", &x, &y) != 2) {
            log_message("无效的坐标格式: %s\n", position);
            continue;
        }
        
        strncpy(screens[count].name, name, sizeof(screens[count].name));
        screens[count].width = width;
        screens[count].height = height;
        screens[count].x = x;
        screens[count].y = y;
        count++;
        
        log_message("屏幕 %s: 分辨率 %dx%d, 位置 (%d,%d)\n", name, width, height, x, y);
    }
    
    fclose(file);
    return count;
}

// 辅助函数：去除字符串中的空格
void remove_spaces(char* str) {
    char* i = str;
    char* j = str;
    while (*j != '\0') {
        *i = *j++;
        if (*i != ' ') {
            i++;
        }
    }
    *i = '\0';
}


// 查找匹配的设备
int find_matching_device(TouchConfig config, InputDeviceInfo devices[], int device_count) {
    int match_index = -1;
    int match_count = 0;
    int match_indices[MAX_DEVICES];
    
    log_message("查找匹配设备: %s (VID:%s PID:%s 路径:%s)\n", 
               config.touchscreen_name, config.vid, config.pid, config.usb_path);
    
    // 第一步：按设备名完全匹配
    for (int i = 0; i < device_count; i++) {
    //     if (strcmp(devices[i].name, config.touchscreen_name) == 0) {
    //         match_indices[match_count++] = i;
    //         log_message("名称完全匹配: %s\n", devices[i].name);
    //     }

        char device_name_no_space[256];
        char config_name_no_space[256];
        
        strcpy(device_name_no_space, devices[i].name);
        strcpy(config_name_no_space, config.touchscreen_name);
        
        remove_spaces(device_name_no_space);
        remove_spaces(config_name_no_space);
        
        if (strcmp(device_name_no_space, config_name_no_space) == 0) {
            match_indices[match_count++] = i;
            log_message("名称完全匹配(去除空格): %s -> %s\n", devices[i].name, device_name_no_space);
        }
    }
    
    // 如果只有一个匹配项，直接返回
    if (match_count == 1) {
        return match_indices[0];
    }
    
    // 如果有多个匹配项，按vid:pid进一步匹配
    if (match_count > 1) {
        log_message("找到多个名称匹配项，按VID:PID进一步筛选\n");
        int vid_pid_match_count = 0;
        int vid_pid_match_indices[MAX_DEVICES];
        
        for (int i = 0; i < match_count; i++) {
            int idx = match_indices[i];
            if (strcasecmp(devices[idx].vid, config.vid) == 0 && 
                strcasecmp(devices[idx].pid, config.pid) == 0) {
                vid_pid_match_indices[vid_pid_match_count++] = idx;
                log_message("VID:PID匹配: %s (VID:%s PID:%s)\n", 
                           devices[idx].name, devices[idx].vid, devices[idx].pid);
            }
        }
        
        if (vid_pid_match_count == 1) {
            return vid_pid_match_indices[0];
        }
        
        // 如果还有多个匹配项，按usb路径匹配
        if (vid_pid_match_count > 1 && config.usb_path[0] != '\0') {
            log_message("找到多个VID:PID匹配项，按USB路径进一步筛选\n");
            for (int i = 0; i < vid_pid_match_count; i++) {
                int idx = vid_pid_match_indices[i];
                if (strcmp(devices[idx].usb_path, config.usb_path) == 0) {
                    log_message("USB路径匹配: %s\n", devices[idx].usb_path);
                    return idx; // 返回第一个匹配的
                }
            }
        }
        
        // 如果还是无法确定，返回第一个匹配项
        if (vid_pid_match_count > 0) {
            log_message("仍有多个匹配项，返回第一个\n");
            return vid_pid_match_indices[0];
        }
    }
    
    // // 如果没有名称匹配项，尝试按vid:pid匹配
    // log_message("未找到名称匹配项，尝试VID:PID匹配\n");
    // for (int i = 0; i < device_count; i++) {
    //     if (strcasecmp(devices[i].vid, config.vid) == 0 && 
    //         strcasecmp(devices[i].pid, config.pid) == 0) {
    //         match_indices[match_count++] = i;
    //         log_message("VID:PID匹配: %s (VID:%s PID:%s)\n", 
    //                    devices[i].name, devices[i].vid, devices[i].pid);
    //     }
    // }
    
    // if (match_count == 1) {
    //     return match_indices[0];
    // }
    
    // // 如果还有多个匹配项，按usb路径匹配
    // if (match_count > 1 && config.usb_path[0] != '\0') {
    //     log_message("找到多个VID:PID匹配项，按USB路径进一步筛选\n");
    //     for (int i = 0; i < match_count; i++) {
    //         int idx = match_indices[i];
    //         if (strcmp(devices[idx].usb_path, config.usb_path) == 0) {
    //             log_message("USB路径匹配: %s\n", devices[idx].usb_path);
    //             return idx;
    //         }
    //     }
    // }
    
    // // 如果还是无法确定，返回第一个匹配项或-1
    // if (match_count > 0) {
    //     log_message("仍有多个匹配项，返回第一个\n");
    //     return match_indices[0];
    // }
    
    log_message("未找到匹配设备\n");
    return -1;
}


/* ==================================

 = 矩阵坐标计算相关代码 =

======================================*/

// 计算虚拟桌面大小
void calculate_virtual_desktop(ScreenInfo screens[], int screen_count, int *total_width, int *total_height) {
    int min_x = 0, min_y = 0;
    int max_x = 0, max_y = 0;
    
    for (int i = 0; i < screen_count; i++) {
        int screen_right = screens[i].x + screens[i].width;
        int screen_bottom = screens[i].y + screens[i].height;
        
        if (screens[i].x < min_x) min_x = screens[i].x;
        if (screens[i].y < min_y) min_y = screens[i].y;
        if (screen_right > max_x) max_x = screen_right;
        if (screen_bottom > max_y) max_y = screen_bottom;
    }
    
    *total_width = max_x - min_x;
    *total_height = max_y - min_y;
    
    log_message("虚拟桌面边界: x(%d 到 %d), y(%d 到 %d)\n", 
               min_x, max_x, min_y, max_y);
}

// 计算坐标转换矩阵的函数（预留算法接口）
void calculate_ctm(ScreenInfo screen, int total_width, int total_height, float matrix[9]) {
    float scale_x = (float)screen.width / total_width;
    float scale_y = (float)screen.height / total_height;
    float offset_x = (float)screen.x / total_width;
    float offset_y = (float)screen.y / total_height;
    
    // 设置变换矩阵
    matrix[0] = scale_x;  matrix[1] = 0.0f;     matrix[2] = offset_x;
    matrix[3] = 0.0f;     matrix[4] = scale_y;  matrix[5] = offset_y;
    matrix[6] = 0.0f;     matrix[7] = 0.0f;     matrix[8] = 1.0f;
}

// 设置设备的坐标转换矩阵
int set_ctm(int device_id, float matrix[9]) {
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "无法打开X显示连接\n");
        return 0;
    }
    
    // 创建ATOM属性
    Atom prop = XInternAtom(display, "Coordinate Transformation Matrix", False);
    if (!prop) {
        fprintf(stderr, "无法创建Coordinate Transformation Matrix属性\n");
        XCloseDisplay(display);
        return 0;
    }
    
    // 检查设备是否支持此属性
    int num_props = 0;
    Atom *props = XIListProperties(display, device_id, &num_props);
    int supports_matrix = 0;
    
    if (props) {
        for (int i = 0; i < num_props; i++) {
            char *prop_name = XGetAtomName(display, props[i]);
            if (strcmp(prop_name, "Coordinate Transformation Matrix") == 0) {
                supports_matrix = 1;
                XFree(prop_name);
                break;
            }
            XFree(prop_name);
        }
        XFree(props);
    }
    
    if (!supports_matrix) {
        fprintf(stderr, "设备 %d 不支持坐标转换矩阵属性\n", device_id);
        XCloseDisplay(display);
        return 0;
    }
    
    // 创建FLOAT类型的原子
    Atom float_atom = XInternAtom(display, "FLOAT", False);
    if (!float_atom) {
        fprintf(stderr, "无法创建FLOAT类型原子\n");
        XCloseDisplay(display);
        return 0;
    }
    
    // 设置属性值
    XIChangeProperty(display, device_id, prop, float_atom, 32, PropModeReplace, 
                     (unsigned char*)matrix, 9);
    
    XFlush(display);
    XCloseDisplay(display);
    return 1;
}

// // 释放设备信息结构体内存
// void free_input_devices(struct InputDeviceInfo *devices, int num_devices) {
//     for (int i = 0; i < num_devices; i++) {
//         free(devices[i].name);
//         free(devices[i].device_node);
//         free(devices[i].vid);
//         free(devices[i].pid);
//         free(devices[i].usb_path);
//     }
//     free(devices);
// }

// 示例使用
int main(int argc, char *argv[])  {
    char *output_file_path = NULL;
    
    // 处理命令行参数
    if (argc > 1) {
        output_file_path = argv[1];
        log_message("输出文件路径: %s\n", output_file_path);
    }

    // 打开日志文件
    log_file = fopen("/opt/ktouch/sub_modules.log", "a");
    if (!log_file) {
        printf("警告: 无法打开日志文件 /opt/ktouch/sub_modules.log，仅输出到控制台\n");
    }
    
    log_message("开始触摸屏配置...\n");
    
    TouchConfig configs[MAX_DEVICES];
    ScreenInfo screens[MAX_DEVICES];
    InputDeviceInfo devices[MAX_DEVICES];
    
    int config_count = read_config("/opt/ktouch/config", configs, MAX_DEVICES);
    if (config_count < 0) {
        log_message("读取配置文件失败\n");
        if (log_file) fclose(log_file);
        return 1;
    }
    log_message("读取了 %d 个配置项\n", config_count);
    
    int screen_count = read_screen_info("/tmp/ktouch/screen.txt", screens, MAX_DEVICES);
    if (screen_count < 0) {
        log_message("读取屏幕信息文件失败\n");
        if (log_file) fclose(log_file);
        return 1;
    }
    log_message("读取了 %d 个屏幕信息\n", screen_count);
    
    // 计算虚拟桌面大小
    int total_width, total_height;
    calculate_virtual_desktop(screens, screen_count, &total_width, &total_height);
    log_message("虚拟桌面大小: %dx%d\n", total_width, total_height);
    
    int device_count = 0;
    device_count = get_input_devices(devices);
    log_message("找到了 %d 个触摸设备\n", device_count);
    
    // 输出所有找到的设备信息
    for (int i = 0; i < device_count; i++) {
        log_message("设备 %d: %s (VID:%s PID:%s 路径:%s XInput ID:%d)\n", 
                   i, devices[i].name, devices[i].vid, devices[i].pid, 
                   devices[i].usb_path, devices[i].device_id);
    }
    
    // 对每个配置项查找匹配的设备
    for (int i = 0; i < config_count; i++) {
        log_message("处理配置: %s|%s|%s|%s|%s\n", 
                   configs[i].display_name, configs[i].touchscreen_name,
                   configs[i].vid, configs[i].pid, configs[i].usb_path);
        
        int device_index = find_matching_device(configs[i], devices, device_count);
        if (device_index == -1) {
            log_message("未找到匹配 %s 的设备\n", configs[i].touchscreen_name);
            continue;
        }
        
        log_message("匹配设备: %s (VID:%s PID:%s 路径:%s XInput ID:%d)\n", 
                   devices[device_index].name, devices[device_index].vid, 
                   devices[device_index].pid, devices[device_index].usb_path,
                   devices[device_index].device_id);
        devices[device_index].match_success = 1;
        
        // 检查XInput设备ID是否有效
        if (devices[device_index].device_id == -1) {
            log_message("错误: 设备 %s 没有有效的XInput ID\n", devices[device_index].name);
            continue;
        }
        
        // 查找对应的屏幕信息
        ScreenInfo* matched_screen = NULL;
        for (int j = 0; j < screen_count; j++) {
            if (strcmp(screens[j].name, configs[i].display_name) == 0) {
                matched_screen = &screens[j];
                break;
            }
        }
        
        if (matched_screen == NULL) {
            log_message("未找到 %s 对应的屏幕, 将使用第一屏\n", configs[i].display_name);
            // 未找到屏幕，则返回第一个屏幕
            matched_screen = &screens[0];
        }
        
        log_message("匹配屏幕: %s %dx%d 位置(%d,%d)\n", 
                   matched_screen->name, matched_screen->width, 
                   matched_screen->height, matched_screen->x, matched_screen->y);
        
        // 计算并设置矩阵
        float matrix[9];
        calculate_ctm(*matched_screen, total_width, total_height, matrix);
        
        log_message("计算矩阵: [%f, %f, %f, %f, %f, %f, %f, %f, %f]\n",
                   matrix[0], matrix[1], matrix[2],
                   matrix[3], matrix[4], matrix[5],
                   matrix[6], matrix[7], matrix[8]);
        
        set_ctm(devices[device_index].device_id, matrix);
        memcpy(devices[device_index].matrix, matrix, sizeof(float) * 9);
        
        log_message("已配置 %s 为 %s 的触摸屏\n", 
                   devices[device_index].name, configs[i].display_name);
    }

    // 保存到文件
    if (output_file_path) {
        int max_retries = 3;
        int retry_delay = 1; // 秒
        FILE *output_file = NULL;
        
        for (int retry = 0; retry < max_retries; retry++) {
            output_file = fopen(output_file_path, "w");
            if (output_file) {
                break;
            }
            log_message("无法打开输出文件 %s (尝试 %d/%d)，%d秒后重试...\n", 
                        output_file_path, retry + 1, max_retries, retry_delay);
            sleep(retry_delay);
        }
        
        if (output_file) {
            int wcount = 0;
            for (int i = 0; i < device_count; i++) {
                wcount++;
                if(devices[i].match_success == 0){
                    continue;
                }
                fprintf(output_file, "%d|%s|%s|%s:%s|%.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f\n",
                        devices[i].device_id,
                        devices[i].name,
                        devices[i].device_node,
                        devices[i].vid, 
                        devices[i].pid,
                        devices[i].matrix[0], devices[i].matrix[1], devices[i].matrix[2],
                        devices[i].matrix[3], devices[i].matrix[4], devices[i].matrix[5],
                        devices[i].matrix[6], devices[i].matrix[7], devices[i].matrix[8]);
            }
            fclose(output_file);
            log_message("已成功写入 %d 条记录到文件 %s\n", wcount, output_file_path);
        } else {
            log_message("经过 %d 次尝试后仍无法打开输出文件 %s，放弃写入\n", 
                        max_retries, output_file_path);
        }
    }
    
    log_message("触摸屏配置完成\n");
    
    if (log_file) {
        fclose(log_file);
    }
    
    return 0;

}