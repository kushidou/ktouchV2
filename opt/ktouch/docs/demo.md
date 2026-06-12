# demo.c - 多屏幕信息窗口演示程序

## 概述
一个基于GTK+和Xrandr的多屏幕信息演示程序。在所有连接的显示器上依次创建信息窗口，显示屏幕分辨率、位置等信息，支持多轮测试和窗口位置校验。

## 程序结构

### 全局变量
| 变量 | 类型 | 说明 |
|------|------|------|
| `info_windows` | `GtkWidget**` | 各屏幕信息窗口指针数组 |
| `total_screens` | `int` | 连接的显示器总数 |
| `screen_widths/heights` | `int*` | 各屏幕宽高 |
| `screen_x_offsets/y_offsets` | `int*` | 各屏幕坐标偏移 |
| `screen_names` | `char**` | 屏幕接口名(如HDMI-1) |
| `current_test_round` | `int` | 当前测试轮次 |
| `total_test_rounds` | `int` | 总测试轮次 |
| `win_time_delay` | `int` | 窗口创建间隔(ms)，默认50 |

## 工作原理

1. **屏幕信息获取**: 使用Xrandr扩展查询所有已连接的输出设备及其CRTC信息
2. **窗口创建**: 每个屏幕创建一个比屏幕尺寸小20像素的窗口，偏移(10,10)
3. **定时创建**: 窗口按`win_time_delay`毫秒间隔依次创建
4. **空格键控制**: 用户按空格键关闭所有窗口，进入下一轮测试
5. **窗口校验**: 每轮结束后校验窗口位置和大小（允许±5像素误差）
6. **日志记录**: 所有操作记录到`screen_demo.log`文件

## 关键函数

| 函数 | 功能 |
|------|------|
| `get_screen_info()` | 通过Xrandr获取所有屏幕信息 |
| `create_info_window()` | 创建单个屏幕的信息窗口 |
| `create_next_window()` | 定时回调，依次创建窗口 |
| `verify_windows()` | 校验窗口位置和大小 |
| `start_next_test_round()` | 开始新一轮测试 |
| `on_key_press()` | 空格键事件处理 |

## 窗口布局
```
窗口位置: (screen_x + 10, screen_y + 10)
窗口大小: (screen_width - 20) x (screen_height - 20)
```

## 调用方式
```bash
./demo
```

## 依赖
- GTK+ 3.x
- X11 + Xrandr扩展
