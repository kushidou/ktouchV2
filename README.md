# ktouch V2

适配 UOS 和麒麟 V10 的触摸屏映射校准工具。支持多屏环境下自动检测触摸屏与显示器对应关系，计算校准矩阵并写入系统，解决触摸紊乱问题。

## 功能

- **图形化配置** — 全屏窗口引导用户依次点击各显示器，自动建立触摸屏与显示器的映射
- **后台守护** — systemd 服务持续监控屏幕/触摸屏/USB 变化，自动触发重新校准
- **显示器管理** — 内置显示器设置工具（kdisplay），支持分辨率、旋转方向调整
- **旋转适配** — 校准矩阵自动适配屏幕旋转（横屏/竖屏/倒置）
- **签字笔支持** — 传统方式配置签字笔设备
- **现场诊断** — kscreen-debug 脚本快速排错，kscreen-log 采集日志

## 架构

```
opt/ktouch/
├── kscreen-remap-daemon    # ← 主控进程（Python），systemd 服务入口
├── kscreen-setup            # 配置脚本：拉起 screen_binder → 解析结果 → 写入 config
├── kscreen-remap            # 单次校准：读取 config → 计算矩阵 → 写入 libinput
│
├── screen_binder            # GUI：全屏窗口 + 触摸检测，输出显示器-触摸屏对应表
├── screen_ds                # 显示器监控：检测分辨率/坐标变化，写入 screen.txt
├── screen_ds_once           # 单次更新显示器信息（screen_ds 阉割版）
├── touch_ds                 # 触摸屏监控：检测矩阵变化，写入 touchmap.txt
├── touch_set                # 校准计算：根据配置计算变换矩阵，写入 libinput
├── usb_ds                   # USB 监控：检测触摸屏插拔，写入 usbadd.txt
├── check_save               # 二次确认窗口：允许用户放弃本次校准
│
├── kdisplay                 # 显示器设置 GUI 工具
├── display_monitor.py       # 分辨率变化监控脚本
│
├── kscreen-debug            # 现场排错脚本
├── kscreen-log              # 日志采集脚本
├── kscreen-fix-daemon       # 服务修复脚本
│
├── desktops/                # 桌面快捷方式
├── forest-light/            # 主题文件
└── config                   # 触摸屏配置文件
```

**运行流程：**

1. `kscreen-remap-daemon` 作为 systemd 服务启动，拉起各监控进程
2. 高频轮询 `screen.txt`（显示器变化）、`usbadd.txt`（USB 插拔）
3. 低频轮询 `touch_ds` 检测触摸矩阵变化
4. 任一条件触发 → `kscreen-remap` 执行校准
5. 用户可通过快捷方式手动触发 `kscreen-setup` 重新配置映射

## 显示器设置（UOS 专属）

UOS 环境下可通过 `kdisplay` 调用系统显示服务（`com.deepin.daemon.Display`）调整显示器参数，与触摸校准联动。

```
kdisplay  ───DBus──►  com.deepin.daemon.Display
    │                       │
    │  GUI 操作             ├── 分辨率 / 旋转 / 主屏切换
    │  · 刷新检测           ├── SwitchMode 合并显示模式
    │  · 设置分辨率         └── Save 持久化
    │  · 旋转方向
    │  · 指定主显示器       display_monitor.py
    │  · 自定义分辨率        后台持续检测分辨率变化
    │                       通知 kscreen-remap-daemon 重新校准
    └── 应用并保存 → 重启显示服务
```

**主要功能：**

| 功能 | 说明 |
|------|------|
| 分辨率设置 | 列出当前显示器支持的 60Hz 模式，支持自定义分辨率 |
| 旋转方向 | 支持正常 / 左转90° / 右转90° / 翻转 |
| 主显示器 | 可指定任意已启用的显示器为主显示器 |
| 自定义分辨率 | 非标分辨率写入 `display_config/` 目录，重启后保持 |
| 排列示意图 | 右侧画布实时展示多屏几何排列 |

自定义分辨率配置文件存放在 `opt/ktouch/display_config/`，格式为 `<显示器名>`。

**用法：**
```bash
# GUI 方式
/opt/ktouch/kdisplay

# 命令行快捷方式
kdisplay
```

> 仅在 UOS 环境下可用（依赖 D-Bus 服务 `com.deepin.daemon.Display`）。
> 麒麟 V10 不支持此功能。

## 编译

```bash
# 依赖
sudo apt install libinput-tools python3-evdev python3-pyudev
sudo apt install libevdev-dev libinput-dev libmtdev-dev libudev-dev libwacom-dev

# 编译
cd src
make          # 仅编译到 src/
make install  # 编译并安装到 ../opt/ktouch/
make clean
```

## 打包

```bash
# 先确保二进制已编译安装
cd src && make install

# 打包为 .deb
chmod +x build_deb.sh
./build_deb.sh          # 输出到当前目录
./build_deb.sh ~/dist/  # 输出到指定目录
```

脚本自动从 `DEBIAN/control` 读取包名、版本、架构，生成 `ktouch_<版本>_<架构>.deb`。

## 安装

```bash
sudo dpkg -i ktouch_*.deb
# postinst 会自动配置 systemd 服务、快捷方式、用户组权限
# 安装完成后需重启以使 input 组生效
```

## License

MIT
