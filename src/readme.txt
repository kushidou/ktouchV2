# 多个子功能源代码

## 源代码对应程序的功能

### screen_binder + touch_listen.c
创建全屏窗口给与提示，并检测触摸点击事件，生成显示器和触摸屏对应列表到当前目录下。
生成的路径是'touch_dis_table.txt'，文件格式为   显示器名|触摸屏名|触摸屏ID|触摸屏设备路径


### screen_ds 
检测显示器分辨率、数量、坐标是否发生了变化，需要通过参数指定记录文件路径，一般在/tmp下。
文件格式   显示器名|分辨率XxY|坐标x,y
-----
定义路径： /tmp/ktouch/screen.txt


### touch_ds
检测触摸屏的矩阵是否变化，需要通过参数指定记录文件
记格式为    12|ELAN Touchscreen|/dev/input/event5|04f3:2c2c|1.000000,0.000000,0.000000,0.000000,1.000000,0.000000,0.000000,0.000000,1.000000
touch_ds传入两个参数，第一个是对比文件的路径，第二个是通知上位程序更新的文件。
---
定义路径:  /tmp/ktouch/touchmap.txt，/tmp/ktouch/touch_need_update.txt

### usb_ds
检测usb是否有新的接入行为，判断接入是是否是触摸屏，需要通过参数指定记录文件。
接入触摸屏后，将会将指定文件内容改成1，提醒主程序，主程序处理完成后应该将其改回0。
----
定义路径:   /tmp/ktouch/usbadd.txt

## 打包后目录中文件说明

check_save              设置后的二次确认窗口，以确定是否要替换触摸屏配置文件
desktops/               快捷方式存放的目录
display_monitor.py      监控屏幕分辨率是否被修改的脚本，后台保持运行
forest-light/           显示器设置GUI的主题文件
forest-light.tcl        显示器设置GUI的主题文件
kdisplay                显示器设置程序
kscreen-debug           触摸屏debug程序，用于现场快速排错
kscreen-fix-daemon      修复service的专用脚本
kscreen-log             采集程序日志和系统日志并打包
kscreen-remap           单次映射/校准触摸屏
kscreen-remap-daemon    主要功能实现的程序，在后台监控触摸屏状态及其他状态，适时操作校准
kscreen-setup           设置触摸屏脚本
screen_binder           设置触摸屏的GUI主体程序
screen_ds               显示屏变化监控
screen_ds_once          更新一次显示器信息，显示屏监控的简化版
src                     所有二进制程序的源文件，保留在程序内防止丢失
touch_ds                对每个触摸屏的矩阵监控
touch_set               计算校准矩阵并写入libinput的主体程序
usb_ds                  usb设备插拔监控

## 功能设计

### 执行设置的功能

首先关闭所有的监控程序可功能。
执行设置时，先通过脚本准备环境，包括桌面环境的检查、文件路径的创建。
然后执行 screen_binder 程序进行识别。
完成后解析上个步骤生成的临时文件，将其补充信息后转换成我们可用的文件。
最后拉起监控程序。


### 检测功能
由服务拉起脚本，脚本配置好环境后在用户环境运行主程序，主程序使用python编写。
1. 准备环境、文件夹
2. 首先启动屏幕监控，会实时更新screen.txt。
3. 启动usb监控，会实时更新usbadd.txt。
4. 发起一次校准操作。
5. 程序死循环运行，除非收到关闭信号。
    5.1 高频轮询screen.txt和usbadd.txt的更新
    5.2 低频启动touch_ds，检测touchmap.txt和计算值的差异。
    5.3 以上任一一个条件触发重新校准操作。


### 校准功能
1. 读取配置文件并解析
2. 读取screen.txt，并对齐
3. 计算校准矩阵
4. 将校准写入系统


编译环境
sudo apt install  libinput-tools python3-evdev python3-pyudev -y
apt download libevdev-dev libinput-dev libmtdev-dev libudev-dev libwacom-dev  libevdev2 libinput-bin libinput10 libudev1 libwacom-common libwacom2 udev


fix3更新
程序完全兼容麒麟和uos，解决了大量麒麟上的显示bug。垃圾麒麟！！


fix4更新
新增支持旋转方向适配，计算矩阵时将会考虑到屏幕的旋转。
新增支持签字笔的配置，采用传统方式。

fix5更新
新增UOS显示器设置工具
新增旋转适配
新增debug脚本用于现场快速检查

