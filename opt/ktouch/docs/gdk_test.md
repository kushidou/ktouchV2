# gdk_test.c - GDK类型测试

## 概述
一个极简的编译测试程序，用于验证`GdkMonitor`类型在当前GTK/GDK版本中是否可用。

## 代码
```c
#include <gdk/gdk.h>
#include <gtk/gtk.h>

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    GdkMonitor *monitor;  // 仅测试该类型是否可识别
    return 0;
}
```

## 用途
该文件不包含实际功能逻辑，仅用于：
- 验证编译环境中GDK头文件是否包含`GdkMonitor`类型定义
- 确认GTK版本是否支持该类型（GTK 3.22+引入）

## 编译测试
```bash
gcc gdk_test.c -o gdk_test $(pkg-config --cflags --libs gtk+-3.0)
```
