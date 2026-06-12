# check_save.c - 保存确认对话框

## 概述
这是一个GTK+图形界面程序，用于在多显示器环境下显示保存确认对话框。用户可以在任意显示器上点击"保存"或"取消"按钮，程序会返回相应的退出代码供调用方判断。

## 程序结构

### 数据结构
```c
typedef struct {
    GtkWidget *window;  // GTK窗口指针
    int monitor_num;    // 显示器编号
    int x, y;           // 显示器位置
    int width, height;  // 显示器分辨率
    char *name;         // 显示器名称/接口名
} MonitorInfo;
```

### 全局变量
- `monitors`: 显示器信息数组
- `monitor_count`: 显示器数量
- `app`: GTK应用程序实例
- `exit_code`: 退出代码（默认1=取消）

## 工作原理

1. **初始化阶段**: 检查DISPLAY环境变量，创建GTK应用
2. **显示器枚举**: 使用`gdk_screen_get_n_monitors()`获取所有显示器信息
3. **窗口创建**: 为每个显示器创建一个400x200的居中确认窗口
4. **用户交互**: 
   - 点击"保存" → 打印"0"，退出代码=0
   - 点击"取消并退出" → 打印"1"，退出代码=1
   - 关闭窗口 → 打印"1"，退出代码=1

## 关键函数

| 函数 | 功能 |
|------|------|
| `activate()` | 应用启动回调，枚举显示器并创建窗口 |
| `create_monitor_window()` | 为指定显示器创建确认窗口 |
| `on_save_clicked()` | 保存按钮回调 |
| `on_cancel_clicked()` | 取消按钮回调 |
| `show_error_dialog()` | 显示错误对话框 |

## 调用方式
```bash
./check_save
# 返回值: 0=保存, 1=取消
```

## 依赖
- GTK+ 3.x
- GDK (含X11后端)
