#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <stdarg.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <stdatomic.h>

#include "touch_listen.h"

// 添加Xrandr头文件和X11显示类型头文件
#ifdef GDK_WINDOWING_X11
#include <X11/extensions/Xrandr.h>
#include <gdk/x11/gdkx11display.h>
#endif

// 全局变量
GtkWidget **calibration_windows = NULL;
GtkWidget **info_windows = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int current_screen = 0;
int total_screens = 0;
int *screen_widths = NULL;
int *screen_heights = NULL;
int *screen_x_offsets = NULL;
int *screen_y_offsets = NULL;
char **screen_names = NULL;
int *touch_bindings = NULL;
char **touch_names = NULL;
char **touch_paths = NULL;
int touch_received = 0;
int enter_pressed = 0;
time_t screen_start_time = 0;
guint timeout_id = 0;
FILE *log_file = NULL;
int info_window_index = 0; // 用于跟踪信息窗口创建进度
Display *xdisplay = NULL; // X11显示连接

// 使用原子操作确保线程安全的状态标志
_Atomic int global_enter_pressed = 0; // 全局回车键按下标志
_Atomic int program_active = 0; // 程序是否处于活动状态
_Atomic int calibration_active = 0; // 校准是否正在进行

pthread_mutex_t enter_mutex = PTHREAD_MUTEX_INITIALIZER; // 回车键互斥锁
pthread_t enter_thread; // 全局回车键监听线程
pthread_t touch_thread;

// 函数声明
void show_next_screen();
void touch_callback(struct touch_event event);
gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean on_timeout(gpointer user_data);
GtkWidget* create_info_window(int screen_index);
GtkWidget* create_calibration_window(int screen_index);
void on_calibration_window_destroy(GtkWidget *widget, gpointer user_data);
void get_screen_info();
void* touch_listener_thread(void* arg);
void log_message(const char *format, ...);
void save_mapping_table();
gboolean create_info_windows_timeout(gpointer user_data);
void* global_enter_listener_thread(void* arg); // 全局回车监听线程
gboolean check_enter_pressed(gpointer user_data); // 检查回车键按下的超时函数
void cleanup_resources(); // 清理资源函数

// 保存映射关系表到文件
void save_mapping_table() {
    FILE *map_file = fopen("touch_dis_table.txt", "w");
    if (!map_file) {
        log_message("无法打开映射关系表文件 touch_dis_table.txt\n");
        return;
    }
    
    log_message("保存映射关系到文件: touch_dis_table.txt\n");
    
    for (int i = 0; i < total_screens; i++) {
        if (touch_bindings[i] != -1) {
            fprintf(map_file, "%s|%s|%d|%s\n", 
                    screen_names[i], 
                    touch_names[i], 
                    touch_bindings[i], 
                    touch_paths[i]);
        } else {
            fprintf(map_file, "%s| | | \n", screen_names[i]);
        }
    }
    
    fclose(map_file);
    log_message("映射关系表保存完成\n");
}

// 日志函数
void log_message(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    printf("[%s] screen_binder \t", time_str);
    vprintf(format, args);
    
    if (log_file) {
        fprintf(log_file, "[%s] screen_binder \t", time_str);
        vfprintf(log_file, format, args);
        fflush(log_file);
    }
    
    va_end(args);
}

// 显示错误对话框
void show_error_dialog(const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                              GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_ERROR,
                                              GTK_BUTTONS_OK,
                                              "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), "错误");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// 触摸事件回调函数
void touch_callback(struct touch_event event) {
    pthread_mutex_lock(&mutex);
    if(event.event_type == 1){
        pthread_mutex_unlock(&mutex);
        return;
    }
    const char *event_types[] = {"按下", "移动", "释放"};
    
    log_message("触摸事件: 设备ID=%d, 设备名=%s, 设备路径=%s, 类型=%s, 坐标=(%d, %d)\n", 
                event.device_id, event.device_name, event.device_path, event_types[event.event_type], event.x, event.y);
    
    static int touch_down_received = 0;
    if (event.event_type == 0) {
        touch_down_received = 1;
        log_message("按下事件记录\n");
    } 
    else if (event.event_type == 2 && touch_down_received) {
        log_message("有效触摸序列完成 (按下+释放)\n");
        touch_bindings[current_screen] = event.device_id;
        strncpy(touch_names[current_screen], event.device_name, 255);
        touch_names[current_screen][255] = '\0';
        strncpy(touch_paths[current_screen], event.device_path, 255);
        touch_paths[current_screen][255] = '\0';
        touch_received = 1;
        touch_down_received = 0;
        
        g_idle_add((GSourceFunc)gtk_widget_destroy, calibration_windows[current_screen]);
    }
    
    pthread_mutex_unlock(&mutex);
}

// 键盘事件处理
gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        log_message("用户按下回车键，跳过屏幕 %d (%s)\n", current_screen, screen_names[current_screen]);
        gtk_widget_destroy(widget);
        return TRUE;
    }
    return FALSE;
}

// 超时处理函数
gboolean on_timeout(gpointer user_data) {
    log_message("屏幕 %d (%s) 超时60秒，继续下一个屏幕\n", current_screen, screen_names[current_screen]);
    gtk_widget_destroy(calibration_windows[current_screen]);
    timeout_id = 0;
    return G_SOURCE_REMOVE;
}

// 创建信息提示窗口
GtkWidget* create_info_window(int screen_index) {
    GdkRectangle rect = {
        .x = screen_x_offsets[screen_index],
        .y = screen_y_offsets[screen_index],
        .width = screen_widths[screen_index],
        .height = screen_heights[screen_index]
    };
    
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "屏幕校准");
    gtk_window_move(GTK_WINDOW(window), rect.x, rect.y);
    gtk_window_set_default_size(GTK_WINDOW(window), rect.width, rect.height);
    gtk_window_set_decorated(GTK_WINDOW(window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE); // 确保窗口在最前面
    
    char message[512];
    snprintf(message, sizeof(message), 
             "<span font='28' weight='bold'>正在校准其他屏幕，请根据其他屏幕上的信息指导操作。</span>\n\n当前屏幕接口: %s", 
             screen_names[screen_index]);
    
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), message);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    
    // 添加容器使标签居中
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(window), box);
    
    gtk_window_set_deletable(GTK_WINDOW(window), FALSE);
    
    return window;
}

// 创建校准窗口
GtkWidget* create_calibration_window(int screen_index) {
    GdkRectangle rect = {
        .x = screen_x_offsets[screen_index],
        .y = screen_y_offsets[screen_index],
        .width = screen_widths[screen_index],
        .height = screen_heights[screen_index]
    };
    
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "触摸屏校准");
    gtk_window_move(GTK_WINDOW(window), rect.x, rect.y);
    gtk_window_set_default_size(GTK_WINDOW(window), rect.width, rect.height);
    gtk_window_fullscreen(GTK_WINDOW(window));
    gtk_window_set_decorated(GTK_WINDOW(window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE); // 确保窗口在最前面
    gtk_window_set_modal(GTK_WINDOW(window), TRUE); // 设置为模态窗口，防止失去焦点
    
    // 设置窗口类型提示为对话框，增加获得焦点的机会
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
    
    // 强制窗口获得焦点
    gtk_window_present(GTK_WINDOW(window));
    
    char message[512];
    snprintf(message, sizeof(message), 
             "<span font='32' weight='bold' foreground='red'>请在触摸屏上点击此屏幕。</span>\n\n屏幕接口: %s\n\n如果此屏幕不是触摸屏，那么请敲击回车或者等待60秒。\n\n点击后请查看下一个屏幕，请勿重复点击。", 
             screen_names[screen_index]);
    
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), message);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    
    // 添加容器使标签居中
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(window), box);
    
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);
    
    return window;
}

// 窗口关闭事件处理
void on_calibration_window_destroy(GtkWidget *widget, gpointer user_data) {
    log_message("关闭屏幕 %d (%s) 的校准窗口\n", current_screen, screen_names[current_screen]);
    
    // 取消超时计时器
    if (timeout_id > 0) {
        g_source_remove(timeout_id);
        timeout_id = 0;
    }
    
    // 如果所有屏幕都已处理完毕
    if (++current_screen >= total_screens) {
        // 设置程序状态为非活动
        atomic_store(&program_active, 0);
        atomic_store(&calibration_active, 0);
        
        // 首先，销毁所有信息窗口
        for (int i = 0; i < total_screens; i++) {
            if (info_windows[i] != NULL) {
                gtk_widget_destroy(info_windows[i]);
                info_windows[i] = NULL;
            }
        }
        
        // 打印绑定结果
        log_message("\n=== 绑定结果 ===\n");
        for (int i = 0; i < total_screens; i++) {
            log_message("屏幕 %d (%s): ", i, screen_names[i]);
            if (touch_bindings[i] != -1) {
                log_message("触摸设备ID: %d, 设备名: %s, 设备路径: %s\n", touch_bindings[i], touch_names[i], touch_paths[i]);
            } else {
                log_message("未绑定触摸设备\n");
            }
        }
        
        // 保存映射关系表
        save_mapping_table();
        
        // 清理资源
        cleanup_resources();
        
        gtk_main_quit();
    } else {
        // 显示下一个屏幕的窗口
        show_next_screen();
    }
}

// 超时创建信息窗口的回调函数
gboolean create_info_windows_timeout(gpointer user_data) {
    if (info_window_index < total_screens) {
        info_windows[info_window_index] = create_info_window(info_window_index);
        gtk_widget_show_all(info_windows[info_window_index]);
        log_message("为屏幕 %d (%s) 创建信息窗口\n", info_window_index, screen_names[info_window_index]);
        info_window_index++;
        return TRUE; // 继续调用
    } else {
        // 所有信息窗口创建完毕，开始校准流程
        show_next_screen();
        return FALSE; // 停止调用
    }
}

// 显示下一个屏幕的窗口
void show_next_screen() {
    log_message("\n=== 处理屏幕 %d (%s) ===\n", current_screen, screen_names[current_screen]);
    
    // 设置校准状态为活动
    atomic_store(&calibration_active, 1);
    
    // 创建当前屏幕的校准窗口
    calibration_windows[current_screen] = create_calibration_window(current_screen);
    g_signal_connect(calibration_windows[current_screen], "destroy", 
                     G_CALLBACK(on_calibration_window_destroy), NULL);
    gtk_widget_show_all(calibration_windows[current_screen]);
    
    // 强制窗口获得焦点
    gtk_window_present(GTK_WINDOW(calibration_windows[current_screen]));
    
    log_message("创建校准窗口完成\n");
    
    // 重置状态
    pthread_mutex_lock(&mutex);
    touch_received = 0;
    enter_pressed = 0;
    screen_start_time = time(NULL);
    pthread_mutex_unlock(&mutex);
    
    // 设置60秒超时
    timeout_id = g_timeout_add_seconds(60, on_timeout, NULL);
    log_message("显示提示文本完成，开始60秒计时\n");
}

// 获取屏幕信息 (使用Xrandr方法)
void get_screen_info() {
    GdkDisplay *gdk_display = gdk_display_get_default();
    
    #ifdef GDK_WINDOWING_X11
    if (!GDK_IS_X11_DISPLAY(gdk_display)) {
        log_message("错误: 非X11显示环境\n");
        return;
    }
    
    xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display);
    Window xroot = GDK_WINDOW_XID(gdk_get_default_root_window());
    
    int rr_event_base, rr_error_base;
    if (!XRRQueryExtension(xdisplay, &rr_event_base, &rr_error_base)) {
        log_message("错误: XRandR扩展不可用\n");
        return;
    }
    
    XRRScreenResources *screen_res = XRRGetScreenResourcesCurrent(xdisplay, xroot);
    if (!screen_res) {
        log_message("错误: 无法获取屏幕资源\n");
        return;
    }
    
    // 计算连接的显示器数量
    total_screens = 0;
    for (int i = 0; i < screen_res->noutput; i++) {
        XRROutputInfo *output_info = XRRGetOutputInfo(xdisplay, screen_res, screen_res->outputs[i]);
        if (output_info && output_info->connection == RR_Connected) {
            total_screens++;
        }
        if (output_info) XRRFreeOutputInfo(output_info);
    }
    
    log_message("找到 %d 个连接的显示器\n", total_screens);
    
    // 分配内存存储屏幕信息
    screen_widths = malloc(total_screens * sizeof(int));
    screen_heights = malloc(total_screens * sizeof(int));
    screen_x_offsets = malloc(total_screens * sizeof(int));
    screen_y_offsets = malloc(total_screens * sizeof(int));
    screen_names = malloc(total_screens * sizeof(char*));
    touch_bindings = malloc(total_screens * sizeof(int));
    touch_names = malloc(total_screens * sizeof(char*));
    touch_paths = malloc(total_screens * sizeof(char*));
    calibration_windows = malloc(total_screens * sizeof(GtkWidget*));
    info_windows = malloc(total_screens * sizeof(GtkWidget*));
    
    // 获取每个显示器的详细信息
    int screen_index = 0;
    for (int i = 0; i < screen_res->noutput; i++) {
        XRROutputInfo *output_info = XRRGetOutputInfo(xdisplay, screen_res, screen_res->outputs[i]);
        if (output_info && output_info->connection == RR_Connected) {
            if (output_info->crtc) {
                XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(xdisplay, screen_res, output_info->crtc);
                if (crtc_info) {
                    screen_names[screen_index] = strdup(output_info->name);
                    screen_widths[screen_index] = crtc_info->width;
                    screen_heights[screen_index] = crtc_info->height;
                    screen_x_offsets[screen_index] = crtc_info->x;
                    screen_y_offsets[screen_index] = crtc_info->y;
                    
                    touch_bindings[screen_index] = -1;
                    touch_names[screen_index] = malloc(256 * sizeof(char));
                    strcpy(touch_names[screen_index], "未绑定");
                    touch_paths[screen_index] = malloc(256 * sizeof(char));
                    strcpy(touch_paths[screen_index], "/dev/null");
                    calibration_windows[screen_index] = NULL;
                    info_windows[screen_index] = NULL;
                    
                    log_message("显示器 %d: %s, 分辨率: %dx%d, 位置: %dx%d\n", 
                               screen_index, screen_names[screen_index],
                               screen_widths[screen_index], screen_heights[screen_index],
                               screen_x_offsets[screen_index], screen_y_offsets[screen_index]);
                    
                    XRRFreeCrtcInfo(crtc_info);
                    screen_index++;
                }
            }
        }
        if (output_info) XRRFreeOutputInfo(output_info);
    }
    
    XRRFreeScreenResources(screen_res);
    #else
    log_message("错误: 非X11环境，无法使用Xrandr\n");
    #endif
}

// 触摸监听线程
void* touch_listener_thread(void* arg) {
    log_message("开始监听触摸事件...\n");
    listen_touch_events();
    return NULL;
}

// 全局回车键监听线程
void* global_enter_listener_thread(void* arg) {
    log_message("开始全局监听回车键...\n");
    
    // 打开X11显示连接
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        log_message("无法打开X11显示连接以监听全局回车键\n");
        return NULL;
    }
    
    // 获取根窗口
    Window root = DefaultRootWindow(display);
    
    // 抓取回车键
    XGrabKey(display, XKeysymToKeycode(display, XK_Return), AnyModifier, 
             root, False, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_KP_Enter), AnyModifier, 
             root, False, GrabModeAsync, GrabModeAsync);
    
    XEvent event;
    while (atomic_load(&program_active)) {
        // 使用非阻塞方式检查事件
        if (XPending(display) > 0) {
            XNextEvent(display, &event);
            if (event.type == KeyPress) {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                if (keysym == XK_Return || keysym == XK_KP_Enter) {
                    // 只有在校准活动时才响应回车键
                    if (atomic_load(&calibration_active)) {
                        log_message("全局回车键被按下（校准过程中）\n");
                        atomic_store(&global_enter_pressed, 1);
                    } else {
                        log_message("全局回车键被按下（非校准过程中，忽略）\n");
                    }
                }
            }
        } else {
            // 短暂睡眠以减少CPU使用
            usleep(100000); // 100毫秒
        }
    }
    
    // 释放抓取的键
    XUngrabKey(display, XKeysymToKeycode(display, XK_Return), AnyModifier, root);
    XUngrabKey(display, XKeysymToKeycode(display, XK_KP_Enter), AnyModifier, root);
    XCloseDisplay(display);
    log_message("全局回车键监听线程退出\n");
    return NULL;
}

// 检查回车键按下的超时函数
gboolean check_enter_pressed(gpointer user_data) {
    if (atomic_load(&global_enter_pressed)) {
        atomic_store(&global_enter_pressed, 0);
        
        // 只有在校准活动时才处理回车键
        if (atomic_load(&calibration_active) && calibration_windows[current_screen]) {
            log_message("检测到全局回车键按下，跳过屏幕 %d (%s)\n", current_screen, screen_names[current_screen]);
            g_idle_add((GSourceFunc)gtk_widget_destroy, calibration_windows[current_screen]);
        }
    }
    
    return G_SOURCE_CONTINUE; // 继续调用
}

// 清理资源函数
void cleanup_resources() {

    // 停止触摸监听
    stop_touch_listener();
    
    // // 等待触摸线程结束
    // if (touch_thread) {
    //     pthread_join(touch_thread, NULL);
    //     touch_thread = 0;
    // }

    // 清理屏幕信息相关资源
    for (int i = 0; i < total_screens; i++) {
        free(screen_names[i]);
        free(touch_names[i]);
        free(touch_paths[i]);
    }
    free(screen_widths);
    free(screen_heights);
    free(screen_x_offsets);
    free(screen_y_offsets);
    free(screen_names);
    free(touch_bindings);
    free(touch_names);
    free(touch_paths);
    free(calibration_windows);
    free(info_windows);
    
    // 关闭X11显示连接
    // if (xdisplay) {
    //     XCloseDisplay(xdisplay);
    //     xdisplay = NULL;
    // }
    
    // 关闭日志文件
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

int main(int argc, char *argv[]) {
    log_file = fopen("/opt/ktouch/sub_modules.log", "a");
    if (!log_file) {
        printf("无法打开日志文件，将只输出到控制台\n");
    }
    
    log_message("=== 触摸屏与显示器绑定程序 (使用Xrandr获取屏幕信息) ===\n");
    
    // 设置程序状态为活动
    atomic_store(&program_active, 1);
    atomic_store(&calibration_active, 0);
    
    gtk_init(&argc, &argv);
    
    get_screen_info();

    log_message("初始化触摸监听器...\n");
    int rst = init_touch_listener(touch_callback);
    if(rst){
        log_message("未找到触摸设备或无法打开触摸设备，请使用sudo运行。\n");
        show_error_dialog("未找到触摸设备或无法打开触摸设备，请使用sudo运行。\n");
        log_message("程序结束\n");
        cleanup_resources();
        return 0;
    }
    
    // pthread_t touch_thread;
    touch_thread = 0;
    if (pthread_create(&touch_thread, NULL, touch_listener_thread, NULL) != 0) {
        log_message("无法创建触摸监听线程\n");
        cleanup_resources();
        return 1;
    }
    
    // pthread_detach(touch_thread);
    
    // 创建全局回车键监听线程
    if (pthread_create(&enter_thread, NULL, global_enter_listener_thread, NULL) != 0) {
        log_message("无法创建全局回车键监听线程\n");
    }
    
    // 添加定时器检查回车键按下
    g_timeout_add(100, check_enter_pressed, NULL); // 每100毫秒检查一次
    
    // 使用超时函数依次创建信息窗口，间隔100毫秒
    info_window_index = 0;
    g_timeout_add(10, create_info_windows_timeout, NULL);
    
    gtk_main();
    
    // 设置程序状态为非活动，等待线程退出
    atomic_store(&program_active, 0);
    
    // 等待全局回车键监听线程退出
    pthread_join(enter_thread, NULL);
    
    log_message("程序结束\n");
    return 0;
}