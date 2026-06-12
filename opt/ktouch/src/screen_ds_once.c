#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#define DEBOUNCE_TIME 3 // 防抖时间（秒）

typedef struct {
    char* output_name;
    int width;
    int height;
    int x;
    int y;
    int rotation; // 新增：旋转方向
} MonitorInfo;

typedef struct {
    MonitorInfo* monitors;
    int count;
} MonitorList;

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
    printf("[%s] screen_ds_once \t", time_str);
    vprintf(format, args);
    
    // 输出到文件
    if (log_file) {
        fprintf(log_file, "[%s] screen_ds \t", time_str);
        vfprintf(log_file, format, args);
        fflush(log_file); // 确保立即写入文件
    }
    
    va_end(args);
}

// 获取当前时间戳（毫秒）
long long current_timestamp() {
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    return milliseconds;
}

// 将旋转值转换为单个字母表示
char rotation_to_char(int rotation) {
    switch (rotation) {
        case RR_Rotate_0: return 'N';   // 正常
        case RR_Rotate_90: return 'L';  // 左旋转
        case RR_Rotate_180: return 'I'; // 倒转
        case RR_Rotate_270: return 'R'; // 右旋转
        default: return 'N';            // 未知则也是正常方向
    }
}

// 获取显示器信息
MonitorList get_monitor_info(Display* dpy, Window root) {
    MonitorList list = {NULL, 0};
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
    if (!res) {
        log_message( "无法获取屏幕资源\n");
        return list;
    }

    // 第一次遍历计算连接中的显示器数量
    int connected_count = 0;
    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *output_info = XRRGetOutputInfo(dpy, res, res->outputs[i]);
        if (output_info && output_info->connection == RR_Connected) {
            connected_count++;
        }
        XRRFreeOutputInfo(output_info);
    }

    // 分配内存
    list.monitors = malloc(connected_count * sizeof(MonitorInfo));
    list.count = 0;

    // 第二次遍历获取详细信息
    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *output_info = XRRGetOutputInfo(dpy, res, res->outputs[i]);
        if (output_info && output_info->connection == RR_Connected) {
            if (output_info->crtc) {
                XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(dpy, res, output_info->crtc);
                if (crtc_info) {
                    list.monitors[list.count].output_name = strdup(output_info->name);
                    list.monitors[list.count].width = crtc_info->width;
                    list.monitors[list.count].height = crtc_info->height;
                    list.monitors[list.count].x = crtc_info->x;
                    list.monitors[list.count].y = crtc_info->y;
                    list.monitors[list.count].rotation = crtc_info->rotation; // 新增：记录旋转方向
                    list.count++;
                    XRRFreeCrtcInfo(crtc_info);
                }
            }
        }
        XRRFreeOutputInfo(output_info);
    }

    XRRFreeScreenResources(res);
    return list;
}

// 释放显示器列表内存
void free_monitor_list(MonitorList* list) {
    for (int i = 0; i < list->count; i++) {
        free(list->monitors[i].output_name);
    }
    free(list->monitors);
    list->monitors = NULL;
    list->count = 0;
}

// 输出显示器信息到文件或标准输出
void output_monitor_info(MonitorList* list, const char* filename) {
    FILE* output = stdout;
    
    if (filename) {
        output = fopen(filename, "w");
        if (!output) {
            log_message("无法打开目标文件: %s\n", filename);
            output = stdout;
        }
    }
    
    log_message("检测到显示器:\n");
    for (int i = 0; i < list->count; i++) {
        fprintf(output, "%s|%dx%d|%dx%d|%c\n",
                list->monitors[i].output_name,
                list->monitors[i].width,
                list->monitors[i].height,
                list->monitors[i].x,
                list->monitors[i].y,
                rotation_to_char(list->monitors[i].rotation));
        log_message("  %s:分辨率%dx%d, 位置%dx%d, 旋转:%c\n",
                list->monitors[i].output_name,
                list->monitors[i].width,
                list->monitors[i].height,
                list->monitors[i].x,
                list->monitors[i].y,
                rotation_to_char(list->monitors[i].rotation));
    }
    
    if (output != stdout) {
        fclose(output);
        log_message("显示器信息已保存到: %s\n", filename);
    }
}

int main(int argc, char *argv[]) {

    log_file = fopen("/opt/ktouch/sub_modules.log", "a");
    if (!log_file) {
        printf("无法打开日志文件，将只输出到控制台\n");
    }
    
    log_message("=== 显示器监控程序(单次执行) ===\n");



    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        log_message("无法打开X显示\n");
        return 1;
    }

    int rr_event_base, rr_error_base;
    if (!XRRQueryExtension(dpy, &rr_event_base, &rr_error_base)) {
        log_message("RandR扩展未找到\n");
        XCloseDisplay(dpy);
        return 1;
    }   

    Window root = DefaultRootWindow(dpy);
    // 监听屏幕变化和输出变化事件
    //XRRSelectInput(dpy, root, RRScreenChangeNotifyMask | RROutputChangeNotifyMask);
    
    // 获取命令行参数
    const char* output_file = NULL;
    if (argc > 1) {
        output_file = argv[1];
    }

    int need_update = 0;
    long long last_event_time = 0;
    
    //printf("开始监听显示器变化...\n");
    //printf("使用Ctrl+C退出程序\n");
    if (output_file) {
        printf("结果将保存到: %s\n", output_file);
    } else {
        printf("结果将输出到标准输出\n");
    }

    // 保存首次信息
    MonitorList monitors = get_monitor_info(dpy, root);
    output_monitor_info(&monitors, output_file);
    free_monitor_list(&monitors);

    //XEvent ev;
    

    XCloseDisplay(dpy);
    return 0;
}