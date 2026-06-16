# 定义编译器
CC = gcc

# 定义pkg-config参数
GTK_FLAGS = $(shell pkg-config --cflags --libs gtk+-3.0)

# 定义目标文件
TARGETS = screen_binder screen_ds touch_ds usb_ds touch_set screen_ds_once check_save

# 安装目标目录
DESTDIR = ../opt/ktouch

# 默认目标：只编译，不安装
all: $(TARGETS)

# 编译screen_binder
screen_binder: screen_binder.c touch_listen.c
	$(CC) -o $@ $^ $(GTK_FLAGS) -lpthread -lX11 -lXrandr

# 编译screen_ds
screen_ds: screen_ds.c
	$(CC) -o $@ $^ -lX11 -lXrandr

# 编译screen_ds_once
screen_ds_once: screen_ds_once.c
	$(CC) -o $@ $^ -lX11 -lXrandr

# 编译touch_set
touch_set: touch_set.c
	$(CC) -o $@ $^ -lX11 -lXi -ludev

# 编译touch_ds
touch_ds: touch_ds.c
	$(CC) -o $@ $^ -lX11 -lXi -ludev

# 编译usb_ds
usb_ds: usb_ds.c
	$(CC) -o $@ $^ -ludev

# 编译check_save
check_save: check_save.c
	$(CC) -o $@ $^ $(GTK_FLAGS)

# 安装：编译全部并复制到 opt/ktouch
install: all
	@mkdir -p $(DESTDIR)
	@for target in $(TARGETS); do \
		install -m 755 $$target $(DESTDIR)/; \
	done
	@echo "已安装到 $(DESTDIR)/"

# 清理生成的文件
clean:
	rm -f $(TARGETS)

# 声明伪目标
.PHONY: all clean install
