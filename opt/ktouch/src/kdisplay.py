#!/usr/bin/env python3
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
import logging
import time
import os
from display_control import DisplayControl
import sys

import sv_ttk

def get_real_path(relative_path):
    """获取资源的正确绝对路径"""
    try:
        # 打包后的情况
        base_path = sys._MEIPASS
    except AttributeError:
        # 开发环境的情况
        base_path = os.path.abspath(".")
    
    return os.path.join(base_path, relative_path)


def get_display_info():
    # 默认临时文件路径
    temp_file = "/tmp/display_selection.txt"
    
    # 检查环境变量中是否有自定义路径
    if 'DISPLAY_SELECTION_FILE' in os.environ:
        temp_file = os.environ['DISPLAY_SELECTION_FILE']
    
    display_info = {}
    
    if os.path.exists(temp_file):
        with open(temp_file, 'r') as f:
            for line in f:
                if '=' in line:
                    key, value = line.strip().split('=', 1)
                    display_info[key] = value
        
        # 转换为整数
        for key in ['MONITOR', 'X', 'Y', 'WIDTH', 'HEIGHT']:
            if key in display_info:
                try:
                    display_info[key] = int(display_info[key])
                except ValueError:
                    # 如果转换失败，使用默认值
                    if key == 'MONITOR':
                        display_info[key] = 0
                    elif key in ['X', 'Y']:
                        display_info[key] = 0
                    elif key == 'WIDTH':
                        display_info[key] = 1920
                    elif key == 'HEIGHT':
                        display_info[key] = 1080
    
    return display_info

class DisplaySettingsGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("显示器设置工具(for UOS)")
        #  """根据/tmp/display_selection.txt设置窗口位置"""
        display_info = get_display_info()
        target_x = 100
        target_x = 100
        window_width = 950  # 增加窗口宽度
        window_height = 600  # 增加窗口高度

        try:
            # 读取配置
            x = display_info.get('X', 0)
            y = display_info.get('Y', 0)
            width = display_info.get('WIDTH', 1280)
            height = display_info.get('HEIGHT', 720)

            target_x = int( x + (width - window_width) //  2)
            target_y = int(y + (height - window_height) // 2)
        except:
            pass
 
        self.root.geometry(f"{window_width}x{window_height}+{target_x}+{target_y}")
        self.root.resizable(True, True)
        
        
        # 显示器控制对象
        self.display_control = DisplayControl()
        
        # 显示器信息
        self.monitors = []
        self.monitor_vars = []
        self.resolution_vars = []
        self.rotation_vars = []
        self.primary_vars = []
        
        # 旋转方向映射
        self.rotation_names = {
            1: "正常",
            2: "向左90度",
            4: "翻转",
            8: "向右90度"
        }
        
        # 创建界面
        self.create_widgets()
        
        # 获取显示器信息
        self.refresh_monitor_info()


    def create_widgets(self):
        # 主框架
        main_frame = ttk.Frame(self.root, padding="10")  # 增加内边距
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # 配置网格权重
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        main_frame.rowconfigure(1, weight=1)
        
        # 标题
        # title_label = ttk.Label(main_frame, text="显示器设置工具(for UOS)", font=("Arial", 12, "bold"))
        # title_label.grid(row=0, column=0, columnspan=2, pady=(0, 15))  # 增加下边距
        
        # 左侧设置面板
        settings_frame = ttk.LabelFrame(main_frame, text="显示器设置", padding="10")  # 增加内边距
        settings_frame.grid(row=1, column=0,columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S), padx=(0, 15))  # 增加右边距
        
        # 右侧示意图面板
        diagram_frame = ttk.LabelFrame(main_frame, text="显示器排列示意图", padding="10")  # 增加内边距
        diagram_frame.grid(row=1, column=2, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # 配置网格权重
        main_frame.rowconfigure(1, weight=1)
        main_frame.columnconfigure(1, weight=1)
        settings_frame.columnconfigure(0, weight=2)
        settings_frame.rowconfigure(0, weight=1)
        diagram_frame.columnconfigure(0, weight=1)
        diagram_frame.rowconfigure(0, weight=1)
        
        # 画布用于显示示意图
        self.canvas = tk.Canvas(diagram_frame, bg="#f0f0f0", relief=tk.SUNKEN, bd=1)
        self.canvas.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # 创建设置区域的滚动框架
        self.scrollable_frame = ttk.Frame(settings_frame)
        self.scrollable_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # 创建滚动条
        scrollbar = ttk.Scrollbar(settings_frame, orient="vertical")
        scrollbar.grid(row=0, column=1, sticky=(tk.N, tk.S))
        
        # 创建画布用于滚动
        self.settings_canvas = tk.Canvas(self.scrollable_frame, yscrollcommand=scrollbar.set)
        self.settings_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        
        # 配置滚动条
        scrollbar.config(command=self.settings_canvas.yview)
        
        # 创建设置区域的内部框架
        self.settings_inner_frame = ttk.Frame(self.settings_canvas, padding="1")  # 增加内边距
        self.settings_canvas.create_window((0, 0), window=self.settings_inner_frame, anchor="nw")
        
        # 绑定配置事件以更新滚动区域
        self.settings_inner_frame.bind("<Configure>", self.on_settings_frame_configure)
        
        # 按钮框架
        button_frame = ttk.Frame(main_frame)
        button_frame.grid(row=2, column=0, columnspan=2, pady=(15, 0))  # 增加上边距
        
        # 刷新按钮
        refresh_btn = ttk.Button(button_frame, text="刷新显示器信息", command=self.refresh_monitor_info)
        refresh_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        # 应用并保存按钮
        apply_save_btn = ttk.Button(button_frame, text="应用并保存设置", command=self.apply_and_save_settings)
        apply_save_btn.pack(side=tk.LEFT)

    def on_settings_frame_configure(self, event):
        """更新滚动区域"""
        self.settings_canvas.configure(scrollregion=self.settings_canvas.bbox("all"))

    def refresh_monitor_info(self):
        """获取显示器信息并更新界面"""
        try:
            self.monitors = self.display_control.get_monitors()
            self.update_settings_ui()
            self.update_diagram()
        except Exception as e:
            messagebox.showerror("错误", f"获取显示器信息时出错: {str(e)}")

    def update_settings_ui(self):
        """更新设置界面"""
        # 清除现有控件
        for widget in self.settings_inner_frame.winfo_children():
            widget.destroy()
        
        self.monitor_vars = []
        self.resolution_vars = []
        self.rotation_vars = []
        self.primary_vars = []
        
        # 获取所有可用的位置选项
        position_options = [str(i+1) for i in range(len(self.monitors))]
        
        # 为每个显示器创建设置控件
        for i, monitor in enumerate(self.monitors):
            monitor_frame = ttk.LabelFrame(self.settings_inner_frame, text=monitor['name'])
            monitor_frame.grid(row=i, column=0, sticky=(tk.W, tk.E), padx=(0,10),pady=(0, 15), ipadx=5, ipady=3)  # 增加下边距
            monitor_frame.columnconfigure(1, weight=1)
            
            # 分辨率设置（放在第一行）
            ttk.Label(monitor_frame, text="分辨率:").grid(row=0, column=0, sticky=tk.W, padx=(0, 20))  # 增加右边距
            res_var = tk.StringVar()
            resolutions = [mode['label'] for mode in monitor['modes']]
            # print(resolutions)
            
            # 设置分辨率
            current_res_index = 0
            if monitor["current_mode"][0] == 0:
                # 自定义分辨率
                resolutions = [f"自定义:{monitor['current_mode'][1]}x{monitor['current_mode'][2]}@{monitor['current_mode'][3]}Hz"] + resolutions
            else:
                for idx, mode in enumerate(monitor['modes']):
                    if mode['id'] == monitor['current_mode'][0]:
                        current_res_index = idx
                        break

            res_combo = ttk.Combobox(monitor_frame, textvariable=res_var, values=resolutions, state="readonly", width=25,height=20)  # 增加宽度
            res_combo.current(current_res_index)
            res_combo.grid(row=0, column=1, sticky=(tk.W, tk.E), pady=5, padx=5)
            
            # 绑定分辨率变化事件
            res_combo.bind('<<ComboboxSelected>>', 
                          lambda e, m=monitor, v=res_var: self.on_resolution_change(m, v))
            
            self.resolution_vars.append(res_var)
            
            # 第二行：位置、旋转和主显示器设置
            options_frame = ttk.Frame(monitor_frame)
            options_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
            
            # 位置设置
            ttk.Label(options_frame, text="位置:").pack(side=tk.LEFT, padx=(0, 5))
            pos_var = tk.StringVar(value=str(monitor['position']))
            pos_combo = ttk.Combobox(options_frame, textvariable=pos_var, values=position_options, 
                                    state="readonly", width=5)
            pos_combo.pack(side=tk.LEFT, padx=(0, 10))
            pos_combo.bind('<<ComboboxSelected>>', 
                          lambda e, m=monitor, v=pos_var: self.update_monitor_position(m, v))
            self.monitor_vars.append(pos_var)
            
            # 分隔线
            ttk.Separator(options_frame, orient='vertical').pack(side=tk.LEFT, padx=10, fill=tk.Y)
            
            # 旋转设置
            ttk.Label(options_frame, text="旋转:").pack(side=tk.LEFT, padx=(0, 5))
            rotation_var = tk.StringVar(value=str(monitor['rotation']))
            rotation_combo = ttk.Combobox(options_frame, textvariable=rotation_var, 
                                         values=["正常", "向左90度", "翻转", "向右90度"], state="readonly", width=10)
            rotation_combo.pack(side=tk.LEFT, padx=(0, 10))
            
            # 设置显示文本为旋转方向名称
            rotation_name = self.rotation_names.get(monitor['rotation'], "正常")
            rotation_combo.set(rotation_name)
            
            # 绑定旋转变化事件
            rotation_combo.bind('<<ComboboxSelected>>', 
                               lambda e, m=monitor, v=rotation_var: self.on_rotation_change(m, v))
            
            self.rotation_vars.append(rotation_var)
            
            # 分隔线
            ttk.Separator(options_frame, orient='vertical').pack(side=tk.LEFT, padx=10, fill=tk.Y)
            
            # 主屏设置
            primary_var = tk.BooleanVar(value=monitor['is_primary'])
            primary_check = ttk.Checkbutton(options_frame, text="主显示器", 
                                           variable=primary_var,
                                           command=lambda v=primary_var, m=monitor: self.update_primary_monitor(v, m))
            primary_check.pack(side=tk.LEFT)

            self.primary_vars.append(primary_var)

    def on_resolution_change(self, monitor, var):
        """当分辨率改变时更新显示器信息"""
        selected_res = var.get()
        
        if selected_res == "自定义分辨率":
            self.prompt_custom_resolution(monitor, var)
        else:
            # 解析分辨率并更新显示器信息
            if "自定义" in selected_res:
                selected_res = selected_res[4:]
            parts = selected_res.split(' @ ')
            resolution = parts[0].split('x')
            width = int(resolution[0])
            height = int(resolution[1])
            
            monitor['width'] = width
            monitor['height'] = height
            
            # 更新所有显示器的坐标
            self.update_all_monitor_positions()
            
            # 更新示意图
            self.update_diagram()

    def update_all_monitor_positions(self):
        """更新所有显示器的坐标"""
        # 按位置分组显示器
        position_groups = {}
        for monitor in self.monitors:
            pos = monitor['position']
            if pos not in position_groups:
                position_groups[pos] = []
            position_groups[pos].append(monitor)
        
        # 计算每个位置组的X坐标
        x_positions = {}
        x_offset = 0
        for pos in sorted(position_groups.keys()):
            # 使用组中第一个显示器的宽度
            monitor = position_groups[pos][0]
            x_positions[pos] = x_offset
            x_offset += monitor['width']
        
        # 更新所有显示器的坐标
        for monitor in self.monitors:
            position = monitor['position']
            monitor['x'] = x_positions.get(position, 0)
            monitor['y'] = 0  # 假设所有显示器在垂直方向对齐

    def prompt_custom_resolution(self, monitor, res_var):
        """提示用户输入自定义分辨率"""
        dialog = CustomResolutionDialog(self.root, monitor)
        result = dialog.result
        
        if result:
            width, height, refresh_rate, force_custom = result
            monitor['custom_width'] = width
            monitor['custom_height'] = height
            monitor['custom_refresh_rate'] = refresh_rate
            monitor['force_custom'] = force_custom
            
            # 更新显示器信息
            monitor['width'] = width
            monitor['height'] = height
            
            # 更新所有显示器的坐标
            self.update_all_monitor_positions()
            
            # 更新下拉框显示
            
            res_var.set(f"自定义:{width}x{height}@{refresh_rate}Hz")
            
            # 更新示意图
            self.update_diagram()

    def on_rotation_change(self, monitor, var):
        """当旋转方向改变时更新显示器信息"""
        rotation_str = var.get()
        rotation_map = {
            "正常": 1,
            "向左90度": 2,
            "翻转": 4,
            "向右90度": 8
        }
        rotation = rotation_map.get(rotation_str, 1)
        old_rotation = monitor["rotation"]

        if old_rotation in [1, 4]:
            if rotation in [2,8]:
                buff = monitor['width']
                monitor['width'] = monitor['height']
                monitor['height'] = buff
        else:
            if rotation in [1,4]:
                buff = monitor['width']
                monitor['width'] = monitor['height']
                monitor['height'] = buff


        monitor['rotation'] = rotation

        self.update_all_monitor_positions()
        
        # 更新示意图
        self.update_diagram()

    def update_monitor_position(self, monitor, var):
        """更新显示器位置"""
        try:
            position = int(var.get())
            if 1 <= position <= len(self.monitors):
                old_position = monitor.get('position', 1)
                monitor['position'] = position
                
                # 更新所有显示器的坐标
                self.update_all_monitor_positions()
                
                self.update_diagram()
        except ValueError:
            pass

    def update_primary_monitor(self, var, monitor):
        """更新主显示器设置"""
        if var.get():
            # 取消其他显示器的选中状态
            for i, primary_var in enumerate(self.primary_vars):
                if primary_var != var:
                    primary_var.set(False)
            for m in self.monitors:
                m["is_primary"] = False
            
            # 设置主显示器
            monitor['is_primary'] = True
            
            # 更新示意图
            self.update_diagram()
        else:
            # 不允许取消主显示器，至少需要有一个主显示器
            var.set(True)
            # messagebox.showwarning("警告", "至少需要有一个主显示器")

    def update_diagram(self):
        """更新显示器排列示意图，根据实际坐标进行渲染"""
        
        
        if not self.monitors:
            return

        self.canvas.delete("all")
            
        # 计算所有显示器的边界
        min_x = min(monitor.get('x', 0) for monitor in self.monitors)
        max_x = max(monitor.get('x', 0) + monitor['width'] for monitor in self.monitors)
        min_y = min(monitor.get('y', 0) for monitor in self.monitors)
        max_y = max(monitor.get('y', 0) + monitor['height'] for monitor in self.monitors)
        
        # 计算画布大小和缩放比例
        canvas_width = self.canvas.winfo_width()
        canvas_height = self.canvas.winfo_height()
        
        if canvas_width <= 1 or canvas_height <= 1:
            # 画布尚未渲染，使用默认大小
            canvas_width = 400
            canvas_height = 300
            
        # 计算总宽度和高度
        total_width = max_x - min_x
        total_height = max_y - min_y
        
        # 计算缩放比例，保留边距
        scale_x = 0.8 * canvas_width / total_width if total_width > 0 else 1
        scale_y = 0.8 * canvas_height / total_height if total_height > 0 else 1
        scale = min(scale_x, scale_y)
        
        # 计算偏移量，使所有显示器居中显示
        offset_x = (canvas_width - total_width * scale) / 2 - min_x * scale
        offset_y = (canvas_height - total_height * scale) / 2 - min_y * scale
        
        # 按坐标和分辨率分组显示器
        coord_groups = {}
        for monitor in self.monitors:
            x = monitor.get('x', 0)
            y = monitor.get('y', 0)
            width = monitor['width']
            height = monitor['height']
            
            # 创建分组键
            group_key = (x, y, width, height)
            
            if group_key not in coord_groups:
                coord_groups[group_key] = []
            coord_groups[group_key].append(monitor)
        
        # 绘制每个组的显示器
        for group_key, monitors in coord_groups.items():
            x, y, width, height = group_key
            
            # 计算在画布上的位置
            canvas_x = offset_x + x * scale
            canvas_y = offset_y + y * scale
            canvas_width_scaled = width * scale
            canvas_height_scaled = height * scale
            
            # 绘制组的背景矩形（仅在多个显示器共享相同坐标和分辨率时）
            if len(monitors) > 1:
                self.canvas.create_rectangle(
                    canvas_x, canvas_y, 
                    canvas_x + canvas_width_scaled, canvas_y + canvas_height_scaled,
                    fill="#e0e0e0", outline="#a0a0a0", width=1, dash=(5, 5)
                )
            
            # 收集所有显示器的名称和旋转信息
            all_names = []
            for monitor in monitors:
                rotation_text = self.rotation_names.get(monitor['rotation'], "未知")
                name_text = f"{monitor['name']} "
                if monitor['is_primary']:
                    name_text += " [主]"
                all_names.append(name_text)
            
            # 在组上方显示所有显示器名称
            if all_names:
                name_text = "\n".join(all_names)
                self.canvas.create_text(
                    canvas_x + canvas_width_scaled / 2, canvas_y - 20,
                    text=name_text,
                    font=("Arial", 9, "bold"),
                    justify=tk.CENTER,
                    fill="blue"
                )
            
            # 绘制每个显示器（按面积从大到小排序，小的覆盖大的）
            sorted_monitors = sorted(monitors, key=lambda m: m['width'] * m['height'], reverse=True)
            
            # 计算重叠显示器的偏移量
            offset_step = min(10 * scale, 10)  # 最大偏移10像素
            max_offset = offset_step * (len(sorted_monitors) - 1)
            
            for i, monitor in enumerate(sorted_monitors):
                # 计算显示器的偏移位置
                monitor_x = canvas_x + min(i * offset_step, max_offset)
                monitor_y = canvas_y + min(i * offset_step, max_offset)
                
                # 绘制矩形
                fill_color = "lightblue" if monitor['is_primary'] else "white"
                rect_id = self.canvas.create_rectangle(
                    monitor_x, monitor_y, 
                    monitor_x + canvas_width_scaled, monitor_y + canvas_height_scaled,
                    fill=fill_color, outline="black", width=2
                )
                
                # 添加显示器分辨率和旋转信息
                rotation_text = self.rotation_names.get(monitor['rotation'], "未知")
                text_lines = [
                    f"{monitor['width']}x{monitor['height']}",
                    f"旋转: {rotation_text}"
                ]
                text_content = "\n".join(text_lines)
                
                text_id = self.canvas.create_text(
                    monitor_x + canvas_width_scaled/2, monitor_y + canvas_height_scaled/2,
                    text=text_content,
                    font=("Arial", 8),
                    justify=tk.CENTER
                )
            
            # 添加坐标标签
            coord_text = f"({x}, {y})"
            self.canvas.create_text(
                canvas_x + canvas_width_scaled / 2, canvas_y + canvas_height_scaled + 15,
                text=coord_text,
                font=("Arial", 8),
                fill="green"
            )
    
    def apply_settings(self):
        """应用显示器设置"""
        try:
            # 检查位置冲突
            position_groups = {}
            for monitor in self.monitors:
                pos = monitor['position']
                if pos not in position_groups:
                    position_groups[pos] = []
                position_groups[pos].append(monitor)
            
            # 检查是否有重叠的显示器
            has_overlap = any(len(monitors) > 1 for monitors in position_groups.values())
            
            if has_overlap:
                overlap_info = []
                for pos, monitors in position_groups.items():
                    if len(monitors) > 1:
                        monitor_names = ", ".join([m['name'] for m in monitors])
                        overlap_info.append(f"位置 {pos}: {monitor_names}")
                
                warning_msg = "以下位置的显示器将重叠显示:\n" + "\n".join(overlap_info)
                if not messagebox.askyesno("位置重叠警告", f"{warning_msg}\n\n是否继续?"):
                    return
            
            # 计算每个位置组的X坐标
            x_positions = {}
            x_offset = 0
            for pos in sorted(position_groups.keys()):
                # 使用组中第一个显示器的宽度
                monitor = position_groups[pos][0]
                x_positions[pos] = x_offset
                x_offset += monitor['width']

             # 在设置主显示器之前调用SwitchMode
            self.display_control.switch_mode()  
            
            # 为每个显示器应用设置
            for i, monitor in enumerate(self.monitors):
                # 设置分辨率
                selected_res = self.resolution_vars[i].get()
                # print(selected_res)
                if selected_res:
                    
                    if "自定义" in selected_res:
                        config=""
                        # 使用自定义分辨率
                        # 设置旋转
                        rotation_str = self.rotation_vars[i].get()
                        rotation_map = {
                            "正常": 1,
                            "向左90度": 2,
                            "翻转": 4,
                            "向右90度": 8
                        }
                        rotation = rotation_map.get(rotation_str, 1)
                        
                        # 设置位置
                        position = monitor['position']
                        x = x_positions.get(position, 0)
                        y = 0
                        
                        # 设置主显示器
                        isPrimary="N"
                        if self.primary_vars[i].get():
                            isPrimary="P"
                        
                        # 添加强制标志
                        force_flag = "F" if monitor.get('force_custom', False) else "A"
                        
                        config=f'{selected_res[4:]}|{x},{y}|{rotation}|{isPrimary}|{force_flag}'
                        # print(config)
                        self.display_control.save_custom_resolution(monitor['name'], config)
                        continue
                    else:
                        self.display_control.remove_custom_resolution(monitor['name'])
                        # 解析分辨率和刷新率
                        parts = selected_res.split(' @ ')
                        resolution = parts[0].split('x')
                        width = int(resolution[0])
                        height = int(resolution[1])
                        refresh_rate = float(parts[1].replace('Hz', ''))
                        
                        # 优先使用Mode接口
                        mode_id = None
                        for mode in monitor['modes']:
                            if mode['label'] == selected_res:
                                mode_id = mode['id']
                                break
                        
                        if mode_id and mode_id != 'custom':
                            self.display_control.set_mode(monitor['path'], mode_id)
                        else:
                            # 备用方案：使用SetModeBySize和SetRefreshRate接口
                            self.display_control.set_mode_by_size(monitor['path'], width, height)
                            self.display_control.set_refresh_rate(monitor['path'], refresh_rate)
                
                    # 设置旋转
                    rotation_str = self.rotation_vars[i].get()
                    rotation_map = {
                        "正常": 1,
                        "向左90度": 2,
                        "翻转": 4,
                        "向右90度": 8
                    }
                    rotation = rotation_map.get(rotation_str, 1)
                    self.display_control.set_rotation(monitor['path'], rotation)
                    
                    # 设置位置
                    position = monitor['position']
                    x = x_positions.get(position, 0)
                    y = 0  # 假设所有显示器在垂直方向对齐
                    self.display_control.set_position(monitor['path'], x, y)
                    
                    # 设置主显示器
                    if self.primary_vars[i].get():
                        self.display_control.set_primary(monitor['name'])
            
            # 应用更改
            self.display_control.apply_changes()
            
            # 等待1秒后刷新显示器信息
            self.root.after(1000, self.refresh_monitor_info)
            
            # messagebox.showinfo("成功", "显示器设置已应用")
            
        except Exception as e:
            messagebox.showerror("错误", f"应用设置时出错: {str(e)}")

    def apply_and_save_settings(self):
        """应用并保存设置"""
        try:
            # 先应用设置
            self.apply_settings()
            
            # 3秒后保存设置
            self.root.after(3000, self.save_settings_with_delay)
            
        except Exception as e:
            messagebox.showerror("错误", f"应用设置时出错: {str(e)}")

    def save_settings_with_delay(self):
        """延迟保存设置"""
        try:
            file_path = "/tmp/ktouch/display_update.txt"
            dir_path = "/tmp/ktouch"
            # 检查目录是否存在
            if os.path.exists(dir_path):
                # 写入文件（不存在则创建）
                with open(file_path, 'w') as f:
                    f.write("1")
                    
            self.display_control.save_settings()
            
            # messagebox.showinfo("成功", "显示器设置已保存")
        except Exception as e:
            messagebox.showerror("错误", f"保存设置时出错: {str(e)}")

    def on_resize(self, event):
        """处理窗口大小变化事件"""
        if event.widget == self.root:
            self.update_diagram()


class CustomResolutionDialog:
    """自定义分辨率对话框"""
    def __init__(self, parent, monitor):
        self.parent = parent
        self.monitor = monitor
        self.result = None
        
        self.dialog = tk.Toplevel(parent)
        self.dialog.title("自定义分辨率")
        self.dialog.geometry("350x250")  # 增加高度以容纳新控件
        self.dialog.transient(parent)
        self.dialog.grab_set()
        
        # 加载之前保存的自定义分辨率
        custom_width = monitor.get('custom_width', 1920)
        custom_height = monitor.get('custom_height', 1080)
        custom_refresh = monitor.get('custom_refresh_rate', 60.0)
        force_custom = monitor.get('force_custom', False)
        
        # 宽度设置
        ttk.Label(self.dialog, text="宽度:").grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
        self.width_var = tk.StringVar(value=str(custom_width))
        ttk.Entry(self.dialog, textvariable=self.width_var).grid(row=0, column=1, padx=5, pady=5, sticky=(tk.W, tk.E))
        
        # 高度设置
        ttk.Label(self.dialog, text="高度:").grid(row=1, column=0, padx=5, pady=5, sticky=tk.W)
        self.height_var = tk.StringVar(value=str(custom_height))
        ttk.Entry(self.dialog, textvariable=self.height_var).grid(row=1, column=1, padx=5, pady=5, sticky=(tk.W, tk.E))
        
        # 刷新率设置
        ttk.Label(self.dialog, text="刷新率 (Hz):").grid(row=2, column=0, padx=5, pady=5, sticky=tk.W)
        self.refresh_var = tk.StringVar(value=str(custom_refresh))
        ttk.Entry(self.dialog, textvariable=self.refresh_var).grid(row=2, column=1, padx=5, pady=5, sticky=(tk.W, tk.E))
        
        # 强制使用自定义分辨率复选框
        self.force_var = tk.BooleanVar(value=force_custom)
        force_check = ttk.Checkbutton(
            self.dialog, 
            text="强制使用自定义分辨率而不是选取驱动分辨率",
            variable=self.force_var
        )
        force_check.grid(row=3, column=0, columnspan=2, padx=5, pady=10, sticky=tk.W)
        
        # 按钮框架
        button_frame = ttk.Frame(self.dialog)
        button_frame.grid(row=4, column=0, columnspan=2, pady=10)
        
        ttk.Button(button_frame, text="确定", command=self.on_ok).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="取消", command=self.on_cancel).pack(side=tk.LEFT, padx=5)
        
        # 配置网格权重
        self.dialog.columnconfigure(1, weight=1)
        
        self.dialog.wait_window()
    
    def on_ok(self):
        """确定按钮处理"""
        try:
            width = int(self.width_var.get())
            height = int(self.height_var.get())
            refresh_rate = float(self.refresh_var.get())
            force_custom = self.force_var.get()
            
            if width <= 0 or height <= 0 or refresh_rate <= 0:
                raise ValueError("值必须为正数")
            
            self.result = (width, height, refresh_rate, force_custom)
            self.dialog.destroy()
            
        except ValueError as e:
            messagebox.showerror("错误", f"请输入有效的数值: {str(e)}")
    
    def on_cancel(self):
        """取消按钮处理"""
        self.dialog.destroy()


def main():
    root = tk.Tk()

    # Import the tcl file
    root.tk.call('source', '/opt/ktouch/forest-light.tcl')

    # Set the theme with the theme_use method
    ttk.Style().theme_use('forest-light')

    app = DisplaySettingsGUI(root)
    root.bind('<Configure>', app.on_resize)

    # sv_ttk.use_light_theme()
    root.mainloop()

if __name__ == "__main__":
    main()