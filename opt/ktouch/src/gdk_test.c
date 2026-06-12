#include <gdk/gdk.h>
#include <gtk/gtk.h>

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    GdkMonitor *monitor;  // 仅测试该类型是否可识别
    return 0;
}
