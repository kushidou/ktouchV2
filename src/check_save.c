#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

typedef struct {
    GtkWidget *window;
    int monitor_num;
    int x;
    int y;
    int width;
    int height;
    char *name;
} MonitorInfo;

MonitorInfo *monitors = NULL;
int monitor_count = 0;
GtkApplication *app;
int exit_code = 1; // 默认退出代码为1（取消）

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

// 关闭所有窗口
void close_all_windows() {
    for (int i = 0; i < monitor_count; i++) {
        if (monitors[i].window) {
            gtk_widget_destroy(monitors[i].window);
        }
    }
}

// 保存按钮点击回调函数
void on_save_clicked(GtkWidget *widget, gpointer data) {
    printf("0\n"); // 打印0
    exit_code = 0; // 设置退出代码为0
    
    close_all_windows();
    g_application_quit(G_APPLICATION(app));
}

// 取消按钮点击回调函数
void on_cancel_clicked(GtkWidget *widget, gpointer data) {
    printf("1\n"); // 打印1
    exit_code = 1; // 设置退出代码为1
    
    close_all_windows();
    g_application_quit(G_APPLICATION(app));
}

// 窗口关闭事件回调函数
gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
    printf("1\n"); // 打印1
    exit_code = 1; // 设置退出代码为1
    
    close_all_windows();
    g_application_quit(G_APPLICATION(app));
    
    return TRUE; // 阻止默认关闭行为，因为我们自己处理
}

// 创建显示器确认窗口
void create_monitor_window(MonitorInfo *info) {
    // 窗口尺寸
    int window_width = 400;
    int window_height = 200;
    
    // 计算居中位置
    int center_x = info->x + (info->width - window_width) / 2;
    int center_y = info->y + (info->height - window_height) / 2;
    
    // 创建窗口标题，包含显示器名称
    char window_title[256];
    if (info->name && strlen(info->name) > 0) {
        snprintf(window_title, sizeof(window_title), "ktouch保存确认-%s", info->name);
    } else {
        snprintf(window_title, sizeof(window_title), "ktouch保存确认-显示器%d", info->monitor_num + 1);
    }
    
    // 创建窗口
    info->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(info->window), window_title);
    gtk_window_set_default_size(GTK_WINDOW(info->window), window_width, window_height);
    gtk_window_move(GTK_WINDOW(info->window), center_x, center_y);
    gtk_window_set_keep_above(GTK_WINDOW(info->window), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(info->window), TRUE);
    gtk_window_set_position(GTK_WINDOW(info->window), GTK_WIN_POS_CENTER);
    
    // 连接窗口关闭事件
    g_signal_connect(info->window, "delete-event", G_CALLBACK(on_window_delete_event), NULL);
    
    // 创建容器
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 20);
    gtk_container_add(GTK_CONTAINER(info->window), box);
    
    // 提示文字
    GtkWidget *label = gtk_label_new("是否保存刚刚的配置的信息？\n任意窗口都可以操作。");
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    
    // 按钮容器（水平排列）
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_set_homogeneous(GTK_BOX(button_box), TRUE);
    gtk_box_pack_start(GTK_BOX(box), button_box, FALSE, FALSE, 0);
    
    // 保存按钮
    GtkWidget *save_button = gtk_button_new_with_label("保存");
    g_signal_connect(save_button, "clicked", G_CALLBACK(on_save_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), save_button, TRUE, TRUE, 0);
    
    // 取消按钮
    GtkWidget *cancel_button = gtk_button_new_with_label("取消并退出");
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_cancel_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), cancel_button, TRUE, TRUE, 0);
    
    // 显示窗口
    gtk_widget_show_all(info->window);
}

// 应用启动回调函数
static void activate(GApplication *application, gpointer user_data) {
    GdkScreen *screen = gdk_screen_get_default();
    if (!screen) {
        fprintf(stderr, "无法获取屏幕信息\n");
        show_error_dialog("无法获取屏幕信息");
        exit(1);
    }
    
    // 获取显示器数量 - 使用兼容旧版本的方法
    monitor_count = gdk_screen_get_n_monitors(screen);
    if (monitor_count <= 0) {
        fprintf(stderr, "未找到显示器\n");
        show_error_dialog("未找到显示器");
        exit(1);
    }
    
    printf("找到 %d 个显示器\n", monitor_count);
    
    // 分配内存存储显示器信息
    monitors = malloc(monitor_count * sizeof(MonitorInfo));
    memset(monitors, 0, monitor_count * sizeof(MonitorInfo));
    
    // 收集所有显示器信息
    for (int i = 0; i < monitor_count; i++) {
        GdkRectangle geometry;
        gdk_screen_get_monitor_geometry(screen, i, &geometry);
        
        monitors[i].monitor_num = i;
        monitors[i].x = geometry.x;
        monitors[i].y = geometry.y;
        monitors[i].width = geometry.width;
        monitors[i].height = geometry.height;
        
        // 尝试获取显示器名称/接口
        const char *monitor_name = gdk_screen_get_monitor_plug_name(screen, i);
        if (monitor_name && strlen(monitor_name) > 0) {
            monitors[i].name = strdup(monitor_name);
        } else {
            // 如果无法获取显示器名称，使用默认名称
            char default_name[50];
            snprintf(default_name, sizeof(default_name), "显示器%d", i + 1);
            monitors[i].name = strdup(default_name);
        }
        
        printf("显示器 %d: 位置(%d, %d), 分辨率 %dx%d, 名称: %s\n", 
               i+1, geometry.x, geometry.y, geometry.width, geometry.height,
               monitors[i].name);
    }
    
    // 为每个显示器创建窗口
    for (int i = 0; i < monitor_count; i++) {
        create_monitor_window(&monitors[i]);
    }
}

int main(int argc, char *argv[]) {
    // 检查DISPLAY环境变量，确保图形环境可用
    if (getenv("DISPLAY") == NULL) {
        fprintf(stderr, "DISPLAY环境变量未设置，尝试设置默认值\n");
        setenv("DISPLAY", ":0", 1);
    }
    
    // 创建GTK应用
    app = gtk_application_new("com.example.touchscreen_confirm", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);
    
    // 释放内存
    if (monitors) {
        for (int i = 0; i < monitor_count; i++) {
            if (monitors[i].name) {
                free(monitors[i].name);
            }
        }
        free(monitors);
    }
    
    // 返回相应的退出代码
    return exit_code;
}