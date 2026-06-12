# screen_ds_once.c - 显示器信息单次查询程序

## 概述
screen_ds.c的简化版本，仅执行一次显示器信息查询并输出结果后退出。适用于需要快速获取当前显示器配置的场景。

## 与screen_ds.c的区别
| 特性 | screen_ds | screen_ds_once |
|------|-----------|----------------|
| 执行模式 | 持续监控 | 单次查询 |
| 事件监听 | 有 | 无 |
| 退出方式 | Ctrl+C | 自动退出 |

## 工作原理

1. 打开X显示连接
2. 查询RandR扩展是否可用
3. 获取所有已连接显示器信息
4. 输出到文件或标准输出
5. 释放资源并退出

## 数据结构
与screen_ds.c相同：
```c
typedef struct {
    char* output_name;
    int width, height;
    int x, y;
    int rotation;
} MonitorInfo;
```

## 输出格式
```
HDMI-1|1920x1080|0x0|N
```

## 关键函数
| 函数 | 功能 |
|------|------|
| `get_monitor_info()` | 获取显示器信息 |
| `output_monitor_info()` | 输出信息 |
| `rotation_to_char()` | 旋转方向编码转换 |

## 调用方式
```bash
./screen_ds_once [输出文件路径]
```

## 依赖
- X11 + Xrandr扩展
