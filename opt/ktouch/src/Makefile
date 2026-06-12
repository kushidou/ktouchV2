# 定义编译器
CC = gcc

# 定义pkg-config参数
GTK_FLAGS = $(shell pkg-config --cflags --libs gtk+-3.0)

# 定义目标文件
TARGETS = screen_binder screen_ds touch_ds usb_ds touch_set screen_ds_once check_save

# 默认目标
all: $(TARGETS)

# 编译screen_binder
screen_binder: screen_binder.c touch_listen.c
	$(CC) -o $@ $^ $(GTK_FLAGS) -lpthread -lX11 -lXrandr
	install -m 755 $@ ../

# 编译screen_ds
screen_ds: screen_ds.c
	$(CC) -o $@ $^ -lX11 -lXrandr
	install -m 755 $@ ../

# 编译screen_ds_once
screen_ds_once: screen_ds_once.c
	$(CC) -o $@ $^ -lX11 -lXrandr
	install -m 755 $@ ../

# 编译touch_set
touch_set: touch_set.c
	$(CC) -o $@ $^ -lX11 -lXi -ludev
	install -m 755 $@ ../

# 编译touch_ds
touch_ds: touch_ds.c
	$(CC) -o $@ $^ -lX11 -lXi -ludev
	install -m 755 $@ ../

# 编译usb_ds
usb_ds: usb_ds.c
	$(CC) -o $@ $^ -ludev
	install -m 755 $@ ../

# 编译check_save
check_save: check_save.c
	$(CC) -o $@ $^ $(GTK_FLAGS)
	install -m 755 $@ ../

# 清理生成的文件
clean:
	rm -f $(TARGETS)

# 声明伪目标
.PHONY: all clean