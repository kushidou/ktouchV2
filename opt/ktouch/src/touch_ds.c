#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <libudev.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XInput.h>
#include <sys/stat.h>
#include <ctype.h>

// 监控等待时间
int watch_time_delay = 30;



FILE *log_file = NULL;                   // 日志文件指针
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
    printf("[%s] touch_ds \t", time_str);
    vprintf(format, args);
    
    // 输出到文件
    if (log_file) {
        fprintf(log_file, "[%s] touch_ds \t", time_str);
        vfprintf(log_file, format, args);
        fflush(log_file); // 确保立即写入文件
    }
    
    va_end(args);
}

// 清理字符串中可能存在的空格
void clean_string(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) {
        if (output && output_size > 0) {
            output[0] = '\0';
        }
        return;
    }
    
    // 第一步：去除所有空格
    char temp[256] = {0};
    size_t temp_idx = 0;
    
    for (size_t i = 0; input[i] != '\0' && temp_idx < sizeof(temp) - 1; i++) {
        if (!isspace((unsigned char)input[i])) {
            temp[temp_idx++] = input[i];
        }
    }
    temp[temp_idx] = '\0';
    
    // 第二步：去除首尾引号
    const char* start = temp;
    const char* end = temp + strlen(temp) - 1;
    
    if (*start == '"' || *start == '\'') {
        start++;
    }
    if (end >= start && (*end == '"' || *end == '\'')) {
        end--;
    }
    
    // 第三步：复制到输出缓冲区
    size_t len = end - start + 1;
    if (len >= output_size) {
        len = output_size - 1;
    }
    
    strncpy(output, start, len);
    output[len] = '\0';
}


// 获取FLOAT原子类型
Atom get_float_atom(Display* display) {
    return XInternAtom(display, "FLOAT", False);
}

// 获取STRING原子类型
Atom get_string_atom(Display* display) {
    return XInternAtom(display, "STRING", False);
}

// 获取INTEGER原子类型
Atom get_integer_atom(Display* display) {
    return XInternAtom(display, "INTEGER", False);
}

// 获取设备属性值
char* get_device_property(Display* display, XID deviceid, const char* prop_name, Atom expected_type) {
    Atom prop = XInternAtom(display, prop_name, False);
    Atom type;
    int format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char* data = NULL;
    
    // 获取设备属性
    int result = XIGetProperty(display, deviceid, prop, 0, 1024, False, 
                              AnyPropertyType, &type, &format, 
                              &nitems, &bytes_after, &data);
    
    if (result == Success && type == expected_type && data != NULL) {
        if (format == 8) { // 字符串类型
            return strdup((char*)data);
        }
    }
    
    if (data) XFree(data);
    return NULL;
}

// 获取设备矩阵属性
float* get_device_matrix_property(Display* display, XID deviceid, const char* prop_name, int* success) {
    Atom prop = XInternAtom(display, prop_name, False);
    Atom float_atom = get_float_atom(display);
    Atom type;
    int format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char* data = NULL;
    
    // 获取设备属性
    int result = XIGetProperty(display, deviceid, prop, 0, 9, False, 
                              AnyPropertyType, &type, &format, 
                              &nitems, &bytes_after, &data);
    
    if (result == Success && type == float_atom && format == 32 && nitems == 9) {
        *success = 1;
        return (float*)data;
    }
    
    if (data) XFree(data);
    *success = 0;
    return NULL;
}

// 判断设备是否存在
// 检查设备是否存在
int device_exists(Display* display, XID deviceid) {
    int ndevices;
    XIDeviceInfo* devices = XIQueryDevice(display, XIAllDevices, &ndevices);
    
    if (!devices) {
        return 0; // 无法获取设备列表
    }
    
    int exists = 0;
    for (int i = 0; i < ndevices; i++) {
        if (devices[i].deviceid == deviceid) {
            exists = 1;
            break;
        }
    }
    
    XIFreeDeviceInfo(devices);
    return exists;
}


// 获取设备矩阵属性字符串表示
int get_matrix_string(Display* display, XID deviceid, char* result) {
    // 第一步检查设备
     // 首先检查设备是否存在
    if (!device_exists(display, deviceid)) {
        return 0;
    }
    int success;
    float* matrix = NULL;
    // char* result = malloc(256); // 分配足够空间存储矩阵字符串
    // log_message("尝试获取设备%d的校准矩阵\n", deviceid);
    
    // 先尝试获取 Coordinate Transformation Matrix
    matrix = get_device_matrix_property(display, deviceid, 
                                       "Coordinate Transformation Matrix", &success);
    
    // 如果没有，尝试获取 libinput Calibration Matrix
    if (!success) {
        matrix = get_device_matrix_property(display, deviceid, 
                                           "libinput Calibration Matrix", &success);
    }
    
    if (success && matrix) {
        // 格式化矩阵字符串，保留6位小数，去掉括号
        sprintf(result, "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f",
                matrix[0], matrix[1], matrix[2],
                matrix[3], matrix[4], matrix[5],
                matrix[6], matrix[7], matrix[8]);
        XFree(matrix);
    } else {
        strcpy(result, "None");
    }
    return 1;
}

// 获取设备节点信息
char* get_device_node(Display* display, XID deviceid, const char* device_name) {
    // 尝试不同的属性名称来获取设备节点
    char* device_node = NULL;
    Atom string_atom = get_string_atom(display);
    
    // 尝试不同的属性名称
    const char* prop_names[] = {
        "Device Node",
        "device-node",
        "DEVICE_NODE",
        NULL
    };
    
    for (int i = 0; prop_names[i] != NULL; i++) {
        device_node = get_device_property(display, deviceid, prop_names[i], string_atom);
        if (device_node != NULL) {
            // printf("  找到设备节点: %s (属性: %s)\n", device_node, prop_names[i]);
            break;
        }
    }
    
    if (device_node == NULL) {
        device_node = strdup("Unknown");
        // printf("  未能找到设备节点，使用 'Unknown'\n");
    }
    
    return device_node;
}

// 检查文件是否存在
int file_exists(const char *path) {
    struct stat buf;
    return (stat(path, &buf) == 0);
}

// 使用udev检查设备是否为触摸屏并获取VID:PID
int is_touchscreen_device(const char* device_node, char* vid, char* pid, int buffer_size) {
    if (strcmp(device_node, "Unknown") == 0) {
        // printf("  设备节点未知，跳过触摸屏检查\n");
        return 0;
    }
    
    // 首先检查设备节点是否存在
    if (!file_exists(device_node)) {
        // printf("  设备节点不存在: %s\n", device_node);
        return 0;
    }
    
    struct udev *udev;
    struct udev_device *dev;
    int is_touchscreen = 0;
    
    // 初始化VID和PID
    strncpy(vid, "Unknown", buffer_size);
    strncpy(pid, "Unknown", buffer_size);
    
    // 创建udev上下文
    udev = udev_new();
    if (!udev) {
        log_message("无法创建udev上下文\n");
        return 0;
    }
    
    // printf("  正在检查设备: %s\n", device_node);
    
    // 获取设备信息
    dev = udev_device_new_from_syspath(udev, device_node);
    if (!dev) {
        log_message("  无法从syspath创建udev设备: %s\n", device_node);
        
        // 尝试通过设备节点创建udev设备
        struct stat st;
        if (stat(device_node, &st) == 0) {
            dev = udev_device_new_from_devnum(udev, 'c', st.st_rdev);
            // if (dev) {
            //     printf("  通过设备节点创建udev设备成功\n");
            // } else {
            //     printf("  通过设备节点创建udev设备失败\n");
            // }
        } else {
            log_message("  无法获取设备状态: %s\n", device_node);
        }
    }
    
    if (dev) {
        // 打印设备属性
        // printf("  设备属性:\n");
        struct udev_list_entry *properties = udev_device_get_properties_list_entry(dev);
        struct udev_list_entry *entry;
        
        udev_list_entry_foreach(entry, properties) {
            const char *name = udev_list_entry_get_name(entry);
            const char *value = udev_list_entry_get_value(entry);
            if (strstr(name, "INPUT") || strstr(name, "ID_")) {
                // printf("    %s=%s\n", name, value);
                
                // 获取VID和PID
                if (strcmp(name, "ID_VENDOR_ID") == 0) {
                    strncpy(vid, value, buffer_size);
                    // printf("  找到VID: %s\n", vid);
                } else if (strcmp(name, "ID_MODEL_ID") == 0) {
                    strncpy(pid, value, buffer_size);
                    // printf("  找到PID: %s\n", pid);
                }
            }
        }
        
        // 向上遍历设备树，查找输入设备属性
        struct udev_device *parent = dev;
        int level = 0;
        while (parent && level < 10) { // 限制遍历深度
            // 检查ID_INPUT_TOUCHSCREEN属性
            const char *touchscreen = udev_device_get_property_value(parent, "ID_INPUT_TOUCHSCREEN");
            if (touchscreen) {
                // printf("  在层级 %d 找到 ID_INPUT_TOUCHSCREEN=%s\n", level, touchscreen);
                if (strcmp(touchscreen, "1") == 0) {
                    is_touchscreen = 1;
                    
                    // 获取VID和PID
                    const char *vendor_id = udev_device_get_property_value(parent, "ID_VENDOR_ID");
                    const char *model_id = udev_device_get_property_value(parent, "ID_MODEL_ID");
                    
                    if (vendor_id) {
                        strncpy(vid, vendor_id, buffer_size);
                        // printf("  找到VID: %s\n", vid);
                    }
                    
                    if (model_id) {
                        strncpy(pid, model_id, buffer_size);
                        // printf("  找到PID: %s\n", pid);
                    }
                    
                    break;
                }
            }
            
            // 检查其他相关属性
            const char *capabilities = udev_device_get_property_value(parent, "ID_INPUT");
            if (capabilities) {
                // printf("  在层级 %d 找到 ID_INPUT=%s\n", level, capabilities);
                if (strstr(capabilities, "touchscreen")) {
                    is_touchscreen = 1;
                    
                    // 获取VID和PID
                    const char *vendor_id = udev_device_get_property_value(parent, "ID_VENDOR_ID");
                    const char *model_id = udev_device_get_property_value(parent, "ID_MODEL_ID");
                    
                    if (vendor_id) {
                        strncpy(vid, vendor_id, buffer_size);
                        // printf("  找到VID: %s\n", vid);
                    }
                    
                    if (model_id) {
                        strncpy(pid, model_id, buffer_size);
                        // printf("  找到PID: %s\n", pid);
                    }
                    
                    break;
                }
            }
            
            // 检查父设备
            parent = udev_device_get_parent(parent);
            level++;
        }
        
        if (!is_touchscreen) {
            // printf("  未找到 ID_INPUT_TOUCHSCREEN=1 属性\n");
        }
        
        udev_device_unref(dev);
    } else {
        // printf("  无法创建udev设备对象\n");
    }
    
    udev_unref(udev);
    return is_touchscreen;
}

// 从文件中读取触摸屏信息
typedef struct {
    XID deviceid;
    char name[100];
    char node[100];
    char vid_pid[20];
    char matrix[120];
} TouchscreenInfo;

int read_touchscreens_from_file(const char* filename, TouchscreenInfo *touchscreens) {
    int count = 0;
    int max_count = 25;
    FILE* file = fopen(filename, "r");
    if (!file) {
        log_message("无法打开文件: %s\n", filename);
        return 0;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), file) && count < max_count) {
        // 移除行尾的换行符
        line[strcspn(line, "\n")] = '\0';
        
        // 跳过空行
        if (strlen(line) == 0) {
            continue;
        }
        
        // 使用strtok分割字符串
        char* token = strtok(line, "|");
        if (!token) continue;
        
        // 解析设备ID
        touchscreens[count].deviceid = strtoul(token, NULL, 10);
        
        // 解析名称
        token = strtok(NULL, "|");
        if (!token) continue;
        strncpy(touchscreens[count].name, token, sizeof(touchscreens[count].name) - 1);
        touchscreens[count].name[sizeof(touchscreens[count].name) - 1] = '\0'; // 确保字符串终止
        
        // 解析设备节点
        token = strtok(NULL, "|");
        if (!token) continue;
        strncpy(touchscreens[count].node, token, sizeof(touchscreens[count].node) - 1);
        touchscreens[count].node[sizeof(touchscreens[count].node) - 1] = '\0';
        
        // 解析VID:PID
        token = strtok(NULL, "|");
        if (!token) continue;
        strncpy(touchscreens[count].vid_pid, token, sizeof(touchscreens[count].vid_pid) - 1);
        touchscreens[count].vid_pid[sizeof(touchscreens[count].vid_pid) - 1] = '\0';
        
        // 解析矩阵
        token = strtok(NULL, "|");
        if (!token) continue;
        char matrix_with_space[120];
        strncpy(matrix_with_space, token, sizeof(touchscreens[count].matrix) - 1);
        clean_string(matrix_with_space, touchscreens[count].matrix, sizeof(matrix_with_space));
        // touchscreens[count].matrix[sizeof(touchscreens[count].matrix) - 1] = '\0';
        
        count++;
    }
    
    fclose(file);
    return count;
}


// 检查flag文件是否为1
int is_flag_set(const char* flagfile) {
    if (!flagfile || !file_exists(flagfile)) {
        return 0;
    }
    
    FILE* file = fopen(flagfile, "r");
    if (!file) {
        return 0;
    }
    
    char value[2];
    fgets(value, sizeof(value), file);
    fclose(file);
    
    return (value[0] == '1');
}

// 设置flag文件值
void set_flag(const char* flagfile, int value) {
    if (!flagfile) return;
    
    FILE* file = fopen(flagfile, "w");
    if (!file) {
        log_message("无法打开flag文件: %s\n", flagfile);
        return;
    }
    
    fprintf(file, "%d", value);
    fclose(file);
    log_message("设置flag文件 %s 为 %d\n", flagfile, value);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <触摸屏信息文件> [flag文件]\n", argv[0]);
        return 1;
    }
    
    log_file = fopen("/opt/ktouch/sub_modules.log", "a");
    if (!log_file) {
        printf("无法打开日志文件，将只输出到控制台\n");
    }
    
    log_message("=== 触摸屏矩阵监控 ===\n");


    const char* touchscreen_file = argv[1];
    const char* flag_file = argc > 2 ? argv[2] : NULL;
    
    // 读取触摸屏信息文件
    int touchscreen_count = 0;
    TouchscreenInfo touchscreens[20];
    while(1){
        touchscreen_count = read_touchscreens_from_file(touchscreen_file, touchscreens);
        if (!touchscreens || touchscreen_count == 0) {
            fprintf(stderr, "没有找到触摸屏信息或无法读取文件，等待10秒后重新开始\n");
            sleep(10);
            continue;
        }else break;
    }
    
    
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        log_message("无法打开X显示\n");
        // free_touchscreens(touchscreens, touchscreen_count);
        return 1;
    }

    // 检查XInput扩展
    int opcode, event, error;
    if (!XQueryExtension(display, "XInputExtension", &opcode, &event, &error)) {
        log_message("X Input扩展不可用\n");
        XCloseDisplay(display);
        // free_touchscreens(touchscreens, touchscreen_count);
        return 1;
    }

    // 主循环
    while (1) {
        // 检查flag文件
        if (flag_file && is_flag_set(flag_file)) {
            printf("检测暂停，flag文件值为1\n");
            sleep(10);
            continue;
        }
        
        int changed = 0;

        touchscreen_count = read_touchscreens_from_file(touchscreen_file, touchscreens);
        if (!touchscreens || touchscreen_count == 0) {
            fprintf(stderr, "没有找到触摸屏信息或无法读取文件，等待10秒后重新开始\n");
            sleep(10);
            continue;
        }
        
        
        // 检查每个触摸屏设备的矩阵
        for (int i = 0; i < touchscreen_count; i++) {
            TouchscreenInfo* info = &touchscreens[i];
            printf("检查设备 ID: %lu, 名称: %s\n", info->deviceid, info->name);
            
            // 获取当前矩阵,产生的数据已经不带空格了
            char current_matrix[100] = {0};
            int isDeviceExist = 0;
            // char* current_matrix = get_matrix_string(display, info->deviceid);
            isDeviceExist = get_matrix_string(display, info->deviceid, current_matrix);
            if(!isDeviceExist) {
                printf("设备%lu不存在\n", info->deviceid);
                continue;
            }
            

            
            // 比较矩阵
            if (strcmp(current_matrix, info->matrix) != 0) {
                log_message("设备 %lu 的矩阵发生变化:原矩阵: %s, 新矩阵: %s\n", info->deviceid,info->matrix,current_matrix);
                changed = 1;
            }
            // free(current_matrix);
            
            
            
            // 如果检测到变化，设置flag并跳出循环
            if (changed) {
                if (flag_file) {
                    set_flag(flag_file, 1);
                }
                break;
            }
        }
        
        if (!changed) {
            // printf("所有触摸屏设备矩阵未发生变化\n");
        }
        
        // 等待10秒
        sleep(watch_time_delay);
    }
    
    // 清理资源（实际上不会执行到这里，因为上面是无限循环）
    // free_touchscreens(touchscreens, touchscreen_count);
    XCloseDisplay(display);
    
    return 0;
}