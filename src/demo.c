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

// 添加Xrandr头文件和X11显示类型头文件
#ifdef GDK_WINDOWING_X11
#include <X11/extensions/Xrandr.h>
#include <gdk/x11/gdkx11display.h>
#endif

int win_time_delay = 50;

// 全局变量
GtkWidget **info_windows = NULL;
int total_screens = 0;
int *screen_widths = NULL;
int *screen_heights = NULL;
int *screen_x_offsets = NULL;
int *screen_y_offsets = NULL;
char **screen_names = NULL;
FILE *log_file = NULL;
int current_test_round = 0;
int total_test_rounds = 0;
int space_pressed = 0;
guint create_timeout_id = 0;
int current_window_index = 0;

// 函数声明
void get_screen_info();
void log_message(const char *format, ...);
GtkWidget* create_info_window(int screen_index);
gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean create_next_window(gpointer user_data);
void close_all_windows();
void verify_windows();
void start_next_test_round();
void on_info_window_destroy(GtkWidget *widget, gpointer user_data);

// 日志函数
void log_message(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    printf("[%s] screen_demo \t", time_str);
    vprintf(format, args);
    
    if (log_file) {
        fprintf(log_file, "[%s] screen_demo \t", time_str);
        vfprintf(log_file, format, args);
        fflush(log_file);
    }
    
    va_end(args);
}

// 获取屏幕信息 (使用Xrandr方法)
void get_screen_info() {
    GdkDisplay *gdk_display = gdk_display_get_default();
    
    #ifdef GDK_WINDOWING_X11
    if (!GDK_IS_X11_DISPLAY(gdk_display)) {
        log_message("错误: 非X11显示环境\n");
        return;
    }
    
    Display *xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display);
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

// 创建信息窗口
GtkWidget* create_info_window(int screen_index) {
    // 计算窗口位置和大小：偏移(10,10)，尺寸比屏幕小20
    int window_x = screen_x_offsets[screen_index] + 10;
    int window_y = screen_y_offsets[screen_index] + 10;
    int window_width = screen_widths[screen_index] - 20;
    int window_height = screen_heights[screen_index] - 20;
    
    // 确保窗口尺寸不会为负值
    if (window_width < 100) window_width = 100;
    if (window_height < 100) window_height = 100;
    
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "屏幕信息演示");
    gtk_window_move(GTK_WINDOW(window), window_x, window_y);
    gtk_window_set_default_size(GTK_WINDOW(window), window_width, window_height);
    gtk_window_set_decorated(GTK_WINDOW(window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    
    char message[512];
    snprintf(message, sizeof(message), 
             "<span font='24' weight='bold'>屏幕 %d: %s</span>\n\n"
             "<span font='18'>分辨率: %dx%d</span>\n"
             "<span font='18'>位置: (%d, %d)</span>\n\n"
             "<span font='18'>窗口位置: (%d, %d)</span>\n"
             "<span font='18'>窗口大小: %dx%d</span>\n\n"
             "<span font='18'>测试轮次: %d/%d</span>\n\n"
             "<span font='18' foreground='blue'>按空格键继续...</span>", 
             screen_index, screen_names[screen_index],
             screen_widths[screen_index], screen_heights[screen_index],
             screen_x_offsets[screen_index], screen_y_offsets[screen_index],
             window_x, window_y, window_width, window_height,
             current_test_round + 1, total_test_rounds);
    
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), message);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    
    // 添加容器使标签居中
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(window), box);
    
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(on_info_window_destroy), GINT_TO_POINTER(screen_index));
    
    return window;
}

// 键盘事件处理
gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    if (event->keyval == GDK_KEY_space) {
        log_message("用户按下空格键\n");
        space_pressed = 1;
        
        // 关闭所有窗口并开始下一轮测试
        close_all_windows();
        return TRUE;
    }
    return FALSE;
}

// 信息窗口销毁事件处理
void on_info_window_destroy(GtkWidget *widget, gpointer user_data) {
    int screen_index = GPOINTER_TO_INT(user_data);
    log_message("屏幕 %d (%s) 的信息窗口已销毁\n", screen_index, screen_names[screen_index]);
    info_windows[screen_index] = NULL;
}

// 创建下一个窗口（间隔3秒）
gboolean create_next_window(gpointer user_data) {
    if (current_window_index >= total_screens) {
        log_message("所有窗口已创建完成\n");
        verify_windows();
        create_timeout_id = 0;
        return G_SOURCE_REMOVE;
    }
    
    log_message("正在为屏幕 %d (%s) 创建信息窗口...\n", 
                current_window_index, screen_names[current_window_index]);
    
    info_windows[current_window_index] = create_info_window(current_window_index);
    gtk_widget_show_all(info_windows[current_window_index]);
    
    log_message("屏幕 %d 的信息窗口已显示\n", current_window_index);
    
    current_window_index++;
    
    if (current_window_index < total_screens) {
        // 设置3秒后创建下一个窗口
        // create_timeout_id = g_timeout_add_seconds(3, create_next_window, NULL);
        create_timeout_id = g_timeout_add(win_time_delay, create_next_window, NULL);
    } else {
        log_message("所有窗口已创建完成\n");
        verify_windows();
    }
    
    return G_SOURCE_REMOVE;
}

// 关闭所有窗口
void close_all_windows() {
    log_message("正在关闭所有窗口...\n");
    
    for (int i = 0; i < total_screens; i++) {
        if (info_windows[i] != NULL) {
            gtk_widget_destroy(info_windows[i]);
            info_windows[i] = NULL;
        }
    }
    
    // 取消超时计时器
    if (create_timeout_id > 0) {
        g_source_remove(create_timeout_id);
        create_timeout_id = 0;
    }
    
    log_message("所有窗口已关闭\n");
    
    // 开始下一轮测试
    start_next_test_round();
}

// 校验窗口的显示状态、位置和大小
void verify_windows() {
    log_message("开始校验窗口状态...\n");
    
    int all_windows_ok = 1;
    
    for (int i = 0; i < total_screens; i++) {
        if (info_windows[i] == NULL) {
            log_message("错误: 屏幕 %d 的窗口未创建\n", i);
            all_windows_ok = 0;
            continue;
        }
        
        if (!gtk_widget_get_visible(info_windows[i])) {
            log_message("错误: 屏幕 %d 的窗口不可见\n", i);
            all_windows_ok = 0;
        }
        
        // 获取窗口实际位置和大小
        int x, y, width, height;
        gtk_window_get_position(GTK_WINDOW(info_windows[i]), &x, &y);
        gtk_window_get_size(GTK_WINDOW(info_windows[i]), &width, &height);
        
        // 计算期望的位置和大小
        int expected_x = screen_x_offsets[i] + 10;
        int expected_y = screen_y_offsets[i] + 10;
        int expected_width = screen_widths[i] - 20;
        int expected_height = screen_heights[i] - 20;
        
        // 确保期望尺寸不会为负值
        if (expected_width < 100) expected_width = 100;
        if (expected_height < 100) expected_height = 100;
        
        // 校验位置（允许±5像素的误差）
        if (abs(x - expected_x) > 5 || abs(y - expected_y) > 5) {
            log_message("警告: 屏幕 %d 的窗口位置不匹配。预期: (%d, %d), 实际: (%d, %d)\n", 
                       i, expected_x, expected_y, x, y);
        }
        
        // 校验大小（允许±5像素的误差）
        if (abs(width - expected_width) > 5 || abs(height - expected_height) > 5) {
            log_message("警告: 屏幕 %d 的窗口大小不匹配。预期: %dx%d, 实际: %dx%d\n", 
                       i, expected_width, expected_height, width, height);
        }
        
        log_message("屏幕 %d 校验完成: 位置=(%d, %d), 大小=%dx%d\n", i, x, y, width, height);
    }
    
    if (all_windows_ok) {
        log_message("所有窗口校验成功\n");
    } else {
        log_message("部分窗口校验失败\n");
    }
}

// 开始下一轮测试
void start_next_test_round() {
    current_test_round++;
    
    if (current_test_round >= total_test_rounds) {
        log_message("所有测试轮次已完成，程序结束\n");
        
        // 清理资源
        for (int i = 0; i < total_screens; i++) {
            free(screen_names[i]);
        }
        free(screen_widths);
        free(screen_heights);
        free(screen_x_offsets);
        free(screen_y_offsets);
        free(screen_names);
        free(info_windows);
        
        // 关闭日志文件
        if (log_file) {
            fclose(log_file);
            log_file = NULL;
        }
        
        gtk_main_quit();
        return;
    }
    
    log_message("开始第 %d/%d 轮测试\n", current_test_round + 1, total_test_rounds);
    
    // 重置窗口索引
    current_window_index = 0;
    space_pressed = 0;
    
    // 开始创建窗口（第一个窗口立即创建，后续窗口间隔3秒）
    create_timeout_id = g_timeout_add(100, create_next_window, NULL);
}

int main(int argc, char *argv[]) {
    log_file = fopen("screen_demo.log", "a");
    if (!log_file) {
        printf("无法打开日志文件，将只输出到控制台\n");
    }
    
    log_message("=== 多屏幕信息窗口演示程序 ===\n");
    
    gtk_init(&argc, &argv);
    
    // 获取屏幕信息
    get_screen_info();
    
    if (total_screens == 0) {
        log_message("未找到任何屏幕，程序退出\n");
        return 1;
    }
    
    // 设置测试轮次为屏幕数量
    total_test_rounds = total_screens;
    log_message("将执行 %d 轮测试（每轮在所有屏幕上创建窗口）\n", total_test_rounds);
    
    // 开始第一轮测试
    current_test_round = -1;
    start_next_test_round();
    
    gtk_main();
    
    log_message("程序结束\n");
    return 0;
}