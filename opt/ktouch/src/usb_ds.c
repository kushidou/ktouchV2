#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <ctype.h>

#define DEFAULT_POLL_INTERVAL 5 // 默认轮询间隔为5秒
#define MAX_RETRY_ATTEMPTS 3    // 最大重试次数
#define RETRY_DELAY 1           // 重试延迟(秒)
#define COOLDOWN_SECONDS 30     // 触摸屏检测冷却时间(秒)

FILE *log_file = NULL;                   // 日志文件指针
// 函数提前声明
int write_file_content_with_retry(const char *file_path, int value);
void sigint_handler(int sig);
int is_touchscreen(struct udev_device *dev);
int read_file_content_with_retry(const char *file_path);
void monitor_usb_devices(const char *control_file_path, int poll_interval);
void print_usage(const char *program_name);



volatile sig_atomic_t keep_running = 1;

void sigint_handler(int sig) {
    keep_running = 0;
}

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
    printf("[%s] usb_ds \t", time_str);
    vprintf(format, args);
    
    // 输出到文件
    if (log_file) {
        fprintf(log_file, "[%s] usb_ds \t", time_str);
        vfprintf(log_file, format, args);
        fflush(log_file); // 确保立即写入文件
    }
    
    va_end(args);
}



// 检查设备是否为触摸屏（简化版）
int is_touchscreen(struct udev_device *dev) {
    const char *name = udev_device_get_property_value(dev, "NAME");
    const char *id_input_touchscreen = udev_device_get_property_value(dev, "ID_INPUT_TOUCHSCREEN");
    
    // 检查ID_INPUT_TOUCHSCREEN属性
    if (id_input_touchscreen != NULL && strcmp(id_input_touchscreen, "1") == 0) {
        return 1;
    }
    
    // 检查设备名称中是否包含"touch"相关关键词（不区分大小写）
    if (name != NULL) {
        // 转换为小写以进行不区分大小写的比较
        char lower_name[256];
        strncpy(lower_name, name, sizeof(lower_name) - 1);
        lower_name[sizeof(lower_name) - 1] = '\0';
        
        for (int i = 0; lower_name[i]; i++) {
            lower_name[i] = tolower(lower_name[i]);
        }
        
        if (strstr(lower_name, "touchscreen") != NULL ||
            strstr(lower_name, "touch") != NULL ||
            strstr(lower_name, "ilitek") != NULL ||
            strstr(lower_name, "tablet") != NULL) {
            return 1;
        }
    }
    
    return 0;
}

// 读取文件内容（带重试机制）
int read_file_content_with_retry(const char *file_path) {
    int attempts = 0;
    int content = -1;
    
    while (attempts < MAX_RETRY_ATTEMPTS && keep_running) {
        FILE *file = fopen(file_path, "r");
        if (file == NULL) {
            if (errno == ENOENT) {
                // 文件不存在，创建并初始化为0
                printf("控制文件不存在，创建文件并初始化为0\n");
                if (write_file_content_with_retry(file_path, 0) == 0) {
                    content = 0;
                    break;
                }
            } else {
                perror("打开文件失败");
            }
        } else {
            if (fscanf(file, "%d", &content) == 1) {
                fclose(file);
                break;
            } else {
                fclose(file);
                fprintf(stderr, "读取文件内容失败\n");
            }
        }
        
        attempts++;
        if (attempts < MAX_RETRY_ATTEMPTS) {
            printf("重试读取文件(%d/%d)...\n", attempts, MAX_RETRY_ATTEMPTS);
            sleep(RETRY_DELAY);
        }
    }
    
    if (attempts >= MAX_RETRY_ATTEMPTS) {
        fprintf(stderr, "达到最大重试次数，放弃读取文件\n");
    }
    
    return content;
}

// 写入文件内容（带重试机制）
int write_file_content_with_retry(const char *file_path, int value) {
    int attempts = 0;
    int success = -1;
    
    while (attempts < MAX_RETRY_ATTEMPTS && keep_running) {
        FILE *file = fopen(file_path, "w");
        if (file == NULL) {
            perror("打开文件失败");
        } else {
            if (fprintf(file, "%d", value) > 0) {
                success = 0;
                fclose(file);
                break;
            } else {
                fclose(file);
                fprintf(stderr, "写入文件内容失败\n");
            }
        }
        
        attempts++;
        if (attempts < MAX_RETRY_ATTEMPTS) {
            printf("重试写入文件(%d/%d)...\n", attempts, MAX_RETRY_ATTEMPTS);
            sleep(RETRY_DELAY);
        }
    }
    
    if (attempts >= MAX_RETRY_ATTEMPTS) {
        fprintf(stderr, "达到最大重试次数，放弃写入文件\n");
    }
    
    return success;
}

// 监控USB设备事件
void monitor_usb_devices(const char *control_file_path, int poll_interval) {
    struct udev *udev;
    struct udev_monitor *mon;
    struct udev_device *dev;
    int fd;
    int use_control_file = 0;
    
    // 检查是否使用控制文件
    if (control_file_path != NULL) {
        use_control_file = 1;
        log_message("使用控制文件: %s\n", control_file_path);
        
        // 初始化控制文件（如果不存在）
        if (access(control_file_path, F_OK) != 0) {
            printf("控制文件不存在，创建并初始化为0\n");
            if (write_file_content_with_retry(control_file_path, 0) != 0) {
                fprintf(stderr, "无法创建控制文件，禁用控制文件功能\n");
                use_control_file = 0;
            }
        }
    }
    
    // 创建udev对象
    udev = udev_new();
    if (!udev) {
        // fprintf(stderr, "无法创建udev对象\n");
        exit(EXIT_FAILURE);
    }
    
    // 创建监控对象，监控USB设备
    mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", NULL);
    udev_monitor_filter_add_match_subsystem_devtype(mon, "input", NULL);
    udev_monitor_enable_receiving(mon);
    
    // 获取监控文件描述符
    fd = udev_monitor_get_fd(mon);
    
    printf("开始监控USB设备...\n");
    printf("轮询间隔: %d秒\n", poll_interval);
    printf("按Ctrl+C退出\n\n");
    
    // 主循环，监控设备事件
    time_t last_trigger_time = 0; // 上次触发控制文件的时间
    while (keep_running) {
        fd_set fds;
        struct timeval tv;
        int ret;
        int file_status = 0;
        int touchscreen_detected = 0;
        
        // 如果使用控制文件，在每轮查询开始前检查文件状态
        if (use_control_file) {
            file_status = read_file_content_with_retry(control_file_path);
            if (file_status == -1) {
                // 读取文件失败，继续监控但不处理事件
                log_message("读取控制文件失败，继续监控但不处理事件\n");
                sleep(poll_interval);
                continue;
            }
            
            if (file_status != 0) {
                // 文件状态不为0，不处理事件
                printf("控制文件状态为%d，不处理事件\n", file_status);
                sleep(poll_interval);
                continue;
            }
        }
        
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = poll_interval;
        tv.tv_usec = 0;
        
        ret = select(fd+1, &fds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(fd, &fds)) {
            // 获取设备
            dev = udev_monitor_receive_device(mon);
            if (dev) {
                const char *action = udev_device_get_action(dev);
                const char *devpath = udev_device_get_devpath(dev);
                const char *product = udev_device_get_property_value(dev, "ID_MODEL");
                const char *vendor = udev_device_get_property_value(dev, "ID_VENDOR");
                
                // 只处理添加设备的事件
                if (action && strcmp(action, "add") == 0) {
                    log_message("检测到新设备:\n");
                    log_message("  设备路径: %s, %s:%s\n", devpath, vendor ? vendor : "未知",product ? product : "未知");
                    
                    // 检查是否为指针设备
                    const char *id_input_mouse = udev_device_get_property_value(dev, "ID_INPUT_MOUSE");
                    const char *id_input_touchpad = udev_device_get_property_value(dev, "ID_INPUT_TOUCHPAD");
                    const char *id_input_joystick = udev_device_get_property_value(dev, "ID_INPUT_JOYSTICK");
                    const char *id_input_touchscreen = udev_device_get_property_value(dev, "ID_INPUT_TOUCHSCREEN");
                    
                    if (id_input_mouse != NULL || id_input_touchpad != NULL || id_input_joystick != NULL  || id_input_touchscreen != NULL) {
                        printf("  类型: 指针设备\n");
                        
                        // 检查是否为触摸屏
                        if (is_touchscreen(dev)) {
                            printf("  子类型: 触摸屏\n");
                            touchscreen_detected = 1;
                        }
                    }
                    
                    // printf("\n");
                }
                
                udev_device_unref(dev);
            }
        } else if (ret == 0) {
            // 超时，没有事件发生
            if (use_control_file) {
                // 检查控制文件状态
                int current_status = read_file_content_with_retry(control_file_path);
                if (current_status == -1) {
                    printf("读取控制文件失败\n");
                } else if (current_status != file_status) {
                    printf("控制文件状态已从%d变为%d\n", file_status, current_status);
                }
            }
        }
        
        // 一轮检测完成后，如果需要更新控制文件状态（带冷却时间防重复触发）
        if (use_control_file && touchscreen_detected) {
            time_t now = time(NULL);
            if (difftime(now, last_trigger_time) < COOLDOWN_SECONDS) {
                log_message("冷却中，跳过本次触发（距上次 %.0f 秒）\n", difftime(now, last_trigger_time));
            } else {
                if (write_file_content_with_retry(control_file_path, 1) == 0) {
                    last_trigger_time = time(NULL);
                    log_message("已更新控制文件状态为1\n");
                    
                    // 等待文件状态恢复为0
                    printf("等待控制文件状态恢复为0...\n");
                    while (keep_running) {
                        int current_status = read_file_content_with_retry(control_file_path);
                        if (current_status == 0) {
                            printf("控制文件状态已恢复为0，继续监控\n");
                            break;
                        } else if (current_status == -1) {
                            printf("读取控制文件失败，等待%d秒后重试\n", poll_interval);
                            sleep(poll_interval);
                        } else {
                            printf("当前控制文件状态: %d，等待%d秒后检查\n", current_status, poll_interval);
                            sleep(poll_interval);
                        }
                    }
                } else {
                    log_message("更新控制文件状态失败\n");
                }
            }
        }
    }
    
    // 清理资源
    udev_monitor_unref(mon);
    udev_unref(udev);
}

void print_usage(const char *program_name) {
    printf("用法: %s [控制文件路径] [轮询间隔(秒)]\n", program_name);
    printf("选项:\n");
    printf("  控制文件路径: 用于控制监控的文件路径(如/tmp/usb_monitor.ctl)\n");
    printf("  轮询间隔: 检查间隔时间(秒)，默认%d秒\n", DEFAULT_POLL_INTERVAL);
    printf("示例:\n");
    printf("  %s /tmp/usb_monitor.ctl 5\n", program_name);
    printf("  %s\n", program_name);
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
    
    const char *control_file_path = NULL;
    int poll_interval = DEFAULT_POLL_INTERVAL;
    
    // 解析命令行参数
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        
        control_file_path = argv[1];
    }
    
    if (argc > 2) {
        poll_interval = atoi(argv[2]);
        if (poll_interval <= 0) {
            fprintf(stderr, "轮询间隔必须大于0\n");
            return EXIT_FAILURE;
        }
    }

    log_file = fopen("/opt/ktouch/sub_modules.log", "a");
    if (!log_file) {
        printf("无法打开日志文件，将只输出到控制台\n");
    }
    
    
    log_message("USB触摸屏设备检测程序\n");
    printf("=====================\n");
    
    // 设置信号处理
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    
    // 启动监控
    monitor_usb_devices(control_file_path, poll_interval);
    
    log_message("程序已退出\n");
    return EXIT_SUCCESS;
}