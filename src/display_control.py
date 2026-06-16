#!/usr/bin/env python3
import dbus
import logging
import time
import os
from typing import List, Dict, Any
import sys

setting_file_dir="/opt/ktouch/display_config"


class DisplayControl:
    def __init__(self):
        self.logger = logging.getLogger('DisplayControl')
        self.logger.setLevel(logging.INFO)
        
        # 创建控制台处理器
        ch = logging.StreamHandler()
        ch.setLevel(logging.INFO)
        formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
        ch.setFormatter(formatter)
        self.logger.addHandler(ch)
        
        self.bus = None
        self.display_obj = None
        self.display_props = None
        self.display_interface = None
        
        self.connect_to_dbus()

    def connect_to_dbus(self):
        """连接到DBus服务"""
        try:
            self.bus = dbus.SessionBus()
            self.display_obj = self.bus.get_object('com.deepin.daemon.Display', '/com/deepin/daemon/Display')
            self.display_props = dbus.Interface(self.display_obj, 'org.freedesktop.DBus.Properties')
            self.display_interface = dbus.Interface(self.display_obj, 'com.deepin.daemon.Display')
            self.logger.info("成功连接到DBus显示服务")
        except Exception as e:
            self.logger.error(f"连接DBus显示服务失败: {str(e)}")
            raise

    # def get_monitors(self) -> List[Dict[str, Any]]:
    #     """获取所有显示器信息"""
    #     self.logger.info("开始获取显示器信息")
    #     try:
    #         monitors_paths = self.display_props.Get('com.deepin.daemon.Display', 'Monitors')
    #         self.logger.info(f"找到 {len(monitors_paths)} 个显示器")
            
    #         monitors = []
    #         for monitor_path in monitors_paths:
    #             monitor_obj = self.bus.get_object('com.deepin.daemon.Display', monitor_path)
    #             monitor_props = dbus.Interface(monitor_obj, 'org.freedesktop.DBus.Properties')
                
    #             # 获取显示器属性
    #             name = str(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Name'))
    #             enabled = str(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Enabled'))
    #             modes = monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Modes')
    #             current_mode = monitor_props.Get('com.deepin.daemon.Display.Monitor', 'CurrentMode')
    #             x = int(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'X'))
    #             y = int(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Y'))
    #             width = int(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Width'))
    #             height = int(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Height'))
    #             rotation = int(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Rotation'))
                
    #             # 获取主显示器
    #             primary = self.display_props.Get('com.deepin.daemon.Display', 'Primary')
    #             is_primary = (primary == name)
                
    #             # 检查是否存在配置文件
    #             config_exists = self.check_config_exists(name)
                
    #             # 解析可用模式，过滤出60fps的模式
    #             available_modes = []
    #             for mode in modes:
    #                 mode_id = int(mode[0])
    #                 mode_width = int(mode[1])
    #                 mode_height = int(mode[2])
    #                 refresh_rate = int(mode[3])
                    
    #                 # 过滤55-65Hz的刷新率
    #                 if 55 <= refresh_rate <= 65:
    #                     available_modes.append({
    #                         'id': mode_id,
    #                         'width': mode_width,
    #                         'height': mode_height,
    #                         'refresh_rate': refresh_rate,
    #                         'label': f"{mode_width}x{mode_height} @ {refresh_rate:.2f}Hz"
    #                     })
                
    #             # 添加自定义分辨率选项
    #             available_modes.append({
    #                 'id': 'custom',
    #                 'width': width,  # 默认使用当前宽度
    #                 'height': height,  # 默认使用当前高度
    #                 'refresh_rate': 60.0,  # 默认刷新率
    #                 'label': "自定义分辨率"
    #             })
                
    #             # 如果有配置文件，设置current_mode为id=0
    #             if config_exists:
    #                 current_mode = [0, width, height, 60]  # id=0表示自定义模式
    #                 self.logger.info(f"显示器 {name} 使用自定义分辨率模式")
                
    #             monitors.append({
    #                 'path': monitor_path,
    #                 'name': name,
    #                 'enabled': enabled,
    #                 'x': x,
    #                 'y': y,
    #                 'width': width,
    #                 'height': height,
    #                 'rotation': rotation,
    #                 'is_primary': is_primary,
    #                 'current_mode': [int(current_mode[0]), int(current_mode[1]), int(current_mode[2]), int(current_mode[3])],
    #                 'modes': available_modes,
    #                 'custom_width': width,  # 自定义宽度
    #                 'custom_height': height,  # 自定义高度
    #                 'custom_refresh_rate': 60.0,  # 自定义刷新率
    #                 'has_custom_config': config_exists  # 是否有自定义配置
    #             })
    #             self.logger.info(f"显示器: {name}, 启用: {enabled}, 位置: ({x}, {y}), 分辨率: {width}x{height}, 旋转: {rotation}, 主屏: {is_primary}")
            
    #         # 估算显示器的位置顺序（基于X坐标）
    #         self.estimate_monitor_positions(monitors)
            
    #         self.logger.info("显示器信息获取完成")
    #         return monitors
            
    #     except Exception as e:
    #         self.logger.error(f"获取显示器信息时出错: {str(e)}")
    #         raise


    def get_monitors(self) -> List[Dict[str, Any]]:
        """获取所有显示器信息"""
        self.logger.info("开始获取显示器信息")
        try:
            monitors_paths = self.display_props.Get('com.deepin.daemon.Display', 'Monitors')
            self.logger.info(f"找到 {len(monitors_paths)} 个显示器")
            
            monitors = []
            for monitor_path in monitors_paths:
                monitor_obj = self.bus.get_object('com.deepin.daemon.Display', monitor_path)
                monitor_props = dbus.Interface(monitor_obj, 'org.freedesktop.DBus.Properties')
                
                # 获取显示器基础属性
                name = str(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Name'))
                enabled = str(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Enabled'))
                modes = monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Modes')
                x = int(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'X'))
                y = int(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Y'))
                width = int(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Width'))
                height = int(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Height'))
                rotation = int(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Rotation'))
                
                # 检查是否存在配置文件并加载
                config_exists = self.check_config_exists(name)
                custom_config = None
                if config_exists:
                    custom_config = self.load_custom_resolution(name)
                    if custom_config:
                        # 从配置文件更新分辨率信息
                        width = custom_config['width']
                        height = custom_config['height']
                        self.logger.info(f"显示器 {name} 加载配置文件中的分辨率: {width}x{height}")
                    else:
                        self.logger.warning(f"显示器 {name} 配置文件存在但解析失败，将使用系统分辨率")
                
                # 确定当前模式（优先使用配置文件信息）
                if config_exists and custom_config:
                    # 配置文件存在且有效时，使用配置的分辨率，模式ID为0，刷新率60
                    current_mode = [0, width, height, 60]
                    self.logger.info(f"显示器 {name} 使用配置文件中的当前模式: {current_mode}")
                else:
                    # 否则使用系统当前模式
                    current_mode = monitor_props.Get('com.deepin.daemon.Display.Monitor', 'CurrentMode')
                    current_mode = [int(current_mode[0]), int(current_mode[1]), int(current_mode[2]), int(current_mode[3])]
                
                # 获取主显示器信息
                primary = self.display_props.Get('com.deepin.daemon.Display', 'Primary')
                is_primary = (primary == name)
                
                # 解析可用模式，过滤出55-65Hz的刷新率
                available_modes = []
                for mode in modes:
                    mode_id = int(mode[0])
                    mode_width = int(mode[1])
                    mode_height = int(mode[2])
                    refresh_rate = int(mode[3])
                    
                    if 55 <= refresh_rate <= 65:
                        available_modes.append({
                            'id': mode_id,
                            'width': mode_width,
                            'height': mode_height,
                            'refresh_rate': refresh_rate,
                            'label': f"{mode_width}x{mode_height} @ {refresh_rate:.2f}Hz"
                        })
                
                # 添加自定义分辨率选项（使用当前有效分辨率作为默认值）
                available_modes.append({
                    'id': 'custom',
                    'width': width,
                    'height': height,
                    'refresh_rate': 60.0,
                    'label': "自定义分辨率"
                })
                
                monitors.append({
                    'path': monitor_path,
                    'name': name,
                    'enabled': enabled,
                    'x': x,
                    'y': y,
                    'width': width,
                    'height': height,
                    'rotation': rotation,
                    'is_primary': is_primary,
                    'current_mode': current_mode,
                    'modes': available_modes,
                    'custom_width': width,
                    'custom_height': height,
                    'custom_refresh_rate': 60.0,
                    'has_custom_config': config_exists
                })
                self.logger.info(f"显示器: {name}, 启用: {enabled}, 位置: ({x}, {y}), 分辨率: {width}x{height}, 旋转: {rotation}, 主屏: {is_primary}")
            
            # 估算显示器的位置顺序（基于X坐标）
            self.estimate_monitor_positions(monitors)
            
            self.logger.info("显示器信息获取完成")
            return monitors
            
        except Exception as e:
            self.logger.error(f"获取显示器信息时出错: {str(e)}")
            raise

    def check_config_exists(self, monitor_name: str) -> bool:
        """检查显示器配置文件是否存在"""
        config_dir = os.path.expanduser(setting_file_dir)
        config_file = os.path.join(config_dir, f"{monitor_name}.conf")
        return os.path.exists(config_file)

    def estimate_monitor_positions(self, monitors: List[Dict[str, Any]]):
        """根据X坐标估算显示器的位置顺序"""
        # 按X坐标排序
        sorted_monitors = sorted(monitors, key=lambda m: m['x'])
        
        seq = 0
        last_x = -1
        # 分配位置编号
        for i, monitor in enumerate(sorted_monitors):
            if monitor['x'] == last_x:
                monitor['position'] = seq
                last_x = int(monitor['x'])
            else:
                seq += 1
                monitor['position'] = seq
                last_x = int(monitor['x'])
            self.logger.info(f"显示器 {monitor['name']} 估算位置: {monitor['position']} (X坐标: {monitor['x']})")

    def enable_monitor(self, monitor_path: str, enabled: bool):
        """启用或禁用显示器"""
        try:
            monitor_obj = self.bus.get_object('com.deepin.daemon.Display', monitor_path)
            monitor_interface = dbus.Interface(monitor_obj, 'com.deepin.daemon.Display.Monitor')
            
            if enabled:
                self.logger.info(f"启用显示器: {monitor_path}")
                monitor_interface.Enable()
            else:
                self.logger.info(f"禁用显示器: {monitor_path}")
                monitor_interface.Disable()
                
        except Exception as e:
            self.logger.error(f"设置显示器启用状态时出错: {str(e)}")
            raise

    def set_mode(self, monitor_path: str, mode_id: int, monitor_name: str = None):
        """通过Mode ID设置显示器模式"""
        try:
            monitor_obj = self.bus.get_object('com.deepin.daemon.Display', monitor_path)
            monitor_interface = dbus.Interface(monitor_obj, 'com.deepin.daemon.Display.Monitor')
            
            self.logger.info(f"设置显示器 {monitor_path} 的Mode ID为 {mode_id}")
            monitor_interface.SetMode(mode_id)
            
            # 如果是系统模式，删除配置文件
            if mode_id != 'custom' and monitor_name:
                self.delete_config(monitor_name)
                
        except Exception as e:
            self.logger.error(f"设置显示器Mode时出错: {str(e)}")
            raise

        # 设置显示模式为填充
        try:
            monitor_props = dbus.Interface(monitor_obj, 'org.freedesktop.DBus.Properties')
            monitor_props.Set('com.deepin.daemon.Display.Monitor', 'CurrentFillMode', dbus.String('Full'))
            self.logger.info(f"已设置显示器 {monitor_path} 的CurrentFillMode为Full")
        except Exception as fill_mode_error:
            self.logger.warning(f"设置CurrentFillMode失败: {str(fill_mode_error)}")

    def set_mode_by_size(self, monitor_path: str, width: int, height: int, monitor_name: str = None, is_custom: bool = False):
        """通过分辨率设置显示器模式"""
        try:
            monitor_obj = self.bus.get_object('com.deepin.daemon.Display', monitor_path)
            monitor_interface = dbus.Interface(monitor_obj, 'com.deepin.daemon.Display.Monitor')
            
            self.logger.info(f"设置显示器 {monitor_path} 的分辨率为 {width}x{height}")
            monitor_interface.SetModeBySize(width, height)
            
            # 如果是自定义分辨率，保存配置并创建need_update文件
            if is_custom and monitor_name:
                self.save_custom_resolution(monitor_name, width, height, 60.0)
                self.create_need_update_file()
            # 如果是系统模式，删除配置文件
            elif monitor_name:
                self.delete_config(monitor_name)
                
        except Exception as e:
            self.logger.error(f"设置显示器分辨率时出错: {str(e)}")
            raise
        
        try:
            monitor_props = dbus.Interface(monitor_obj, 'org.freedesktop.DBus.Properties')
            monitor_props.Set('com.deepin.daemon.Display.Monitor', 'CurrentFillMode', dbus.String('Full'))
            self.logger.info(f"已设置显示器 {monitor_path} 的CurrentFillMode为Full")
        except Exception as fill_mode_error:
            self.logger.warning(f"设置CurrentFillMode失败: {str(fill_mode_error)}")
            

    def set_position(self, monitor_path: str, x: int, y: int):
        """设置显示器位置"""
        try:
            monitor_obj = self.bus.get_object('com.deepin.daemon.Display', monitor_path)
            monitor_interface = dbus.Interface(monitor_obj, 'com.deepin.daemon.Display.Monitor')
            
            self.logger.info(f"设置显示器 {monitor_path} 的位置为 ({x}, {y})")
            monitor_interface.SetPosition(x, y)
                
        except Exception as e:
            self.logger.error(f"设置显示器位置时出错: {str(e)}")
            raise

    def set_refresh_rate(self, monitor_path: str, refresh_rate: float, monitor_name: str = None, is_custom: bool = False):
        """设置显示器刷新率"""
        try:
            monitor_obj = self.bus.get_object('com.deepin.daemon.Display', monitor_path)
            monitor_interface = dbus.Interface(monitor_obj, 'com.deepin.daemon.Display.Monitor')
            
            self.logger.info(f"设置显示器 {monitor_path} 的刷新率为 {refresh_rate}Hz")
            monitor_interface.SetRefreshRate(refresh_rate)
            
            # 如果是自定义刷新率，保存配置并创建need_update文件
            if is_custom and monitor_name:
                # 需要先获取当前分辨率
                monitor_props = dbus.Interface(monitor_obj, 'org.freedesktop.DBus.Properties')
                width = int(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Width'))
                height = int(monitor_props.Get('com.deepin.daemon.Display.Monitor', 'Height'))
                self.save_custom_resolution(monitor_name, width, height, refresh_rate)
                self.create_need_update_file()
            # 如果是系统模式，删除配置文件
            elif monitor_name:
                self.delete_config(monitor_name)
                
        except Exception as e:
            self.logger.error(f"设置显示器刷新率时出错: {str(e)}")
            raise

    def set_rotation(self, monitor_path: str, rotation: int):
        """设置显示器旋转方向"""
        try:
            monitor_obj = self.bus.get_object('com.deepin.daemon.Display', monitor_path)
            monitor_interface = dbus.Interface(monitor_obj, 'com.deepin.daemon.Display.Monitor')
            
            rotation_names = {
                1: "正常",
                2: "向左90度",
                4: "翻转",
                8: "向右90度"
            }
            self.logger.info(f"设置显示器 {monitor_path} 的旋转方向为 {rotation} ({rotation_names.get(rotation, '未知')})")
            monitor_interface.SetRotation(rotation)
                
        except Exception as e:
            self.logger.error(f"设置显示器旋转方向时出错: {str(e)}")
            raise


    def switch_mode(self):
        """切换显示模式，为设置主显示器做准备"""
        try:
            self.logger.info("调用SwitchMode方法准备设置主显示器")
            # 调用SwitchMode方法，第一个参数是byte类型的2，第二个参数是空字符串
            self.display_interface.SwitchMode(dbus.Byte(2), dbus.String(""))
            self.logger.info("SwitchMode方法调用成功")
        except Exception as e:
            self.logger.error(f"调用SwitchMode方法时出错: {str(e)}")
            raise

    def set_primary(self, monitor_name: str):
        """设置主显示器"""
        try:
            self.logger.info(f"设置主显示器: {monitor_name}")
            self.display_interface.SetPrimary(monitor_name)
                
        except Exception as e:
            self.logger.error(f"设置主显示器时出错: {str(e)}")
            raise

    def apply_changes(self):
        """应用更改"""
        try:
            self.logger.info("应用显示设置更改")
            self.display_interface.ApplyChanges()
                
        except Exception as e:
            self.logger.error(f"应用显示设置时出错: {str(e)}")
            raise

    def save_settings(self):
        """保存设置"""
        try:
            self.logger.info("保存显示设置")
            self.display_interface.Save()
                
        except Exception as e:
            self.logger.error(f"保存显示设置时出错: {str(e)}")
            raise

    def save_custom_resolution(self, monitor_name: str, config: str):
        """保存自定义配置到文件，配置格式为“{width}x{height}@{rate}|x,y|rotation|isPrimary”"""
        try:
            config_dir = os.path.expanduser(setting_file_dir)
            os.makedirs(config_dir, exist_ok=True)
            
            config_file = os.path.join(config_dir, f"{monitor_name}.conf")
            
            with open(config_file, 'w') as f:
                f.write(config)
                    
            self.logger.info(f"已保存自定义配置到 {config_file}: {config}")
            
        except Exception as e:
            self.logger.error(f"保存自定义配置时出错: {str(e)}")

    def remove_custom_resolution(self, monitor_name):
        config_dir = os.path.expanduser(setting_file_dir)
        os.makedirs(config_dir, exist_ok=True)
            
        config_file = os.path.join(config_dir, f"{monitor_name}.conf")

        if os.path.exists(config_file):
            try:
                # 删除配置文件
                os.remove(config_file)
                # logging.info(f"已删除显示器 {monitor_name} 的自定义分辨率配置文件")
                return True
            except Exception as e:
                # logging.error(f"删除显示器 {monitor_name} 的自定义分辨率配置文件时出错: {str(e)}")
                return False

        return True
        


    def create_need_update_file(self):
        """创建need_update文件，如果打开失败则再尝试一次"""
        try:
            config_dir = os.path.expanduser(setting_file_dir)
            os.makedirs(config_dir, exist_ok=True)
            
            need_update_file = os.path.join(config_dir, "need_update")
            
            try:
                with open(need_update_file, 'w') as f:
                    f.write('1')
            except:
                # 如果失败，再尝试一次
                try:
                    with open(need_update_file, 'w') as f:
                        f.write('1')
                except Exception as e:
                    self.logger.error(f"写入need_update文件失败: {str(e)}")
                    
        except Exception as e:
            self.logger.error(f"创建need_update文件时出错: {str(e)}")

    def delete_config(self, monitor_name: str):
        """删除配置文件"""
        try:
            config_dir = os.path.expanduser(setting_file_dir)
            config_file = os.path.join(config_dir, f"{monitor_name}.conf")
            
            if os.path.exists(config_file):
                os.remove(config_file)
                self.logger.info(f"已删除配置文件: {config_file}")
                
        except Exception as e:
            self.logger.error(f"删除配置文件时出错: {str(e)}")

    # def load_custom_resolution(self, monitor_name: str):
    #     """
    #     加载自定义分辨率配置文件
        
    #     配置文件新格式: {width}x{height}@{rate}|x,y|rotation|isPrimary
    #     例如: 1920x1080@60|1920,0|0|True
    #     配置文件路径: 同目录下的display_settings目录，以显示器名称.conf命名
        
    #     Args:
    #         monitor_name: 显示器名称
            
    #     Returns:
    #         包含解析后的配置信息的字典，解析失败返回None
    #     """
    #     try:
    #         # 构建配置文件路径：同目录下的display_settings目录，文件名格式为"显示器名称.conf"
    #         import os
    #         # 获取当前文件所在目录
    #         current_dir = os.path.dirname(os.path.abspath(__file__))
    #         # 构建display_settings目录路径
    #         settings_dir = setting_file_dir
    #         # 构建完整配置文件路径
    #         config_path = os.path.join(settings_dir, f"{monitor_name}.conf")
            
    #         # 检查配置文件目录是否存在
    #         if not os.path.exists(settings_dir):
    #             self.logger.warning(f"配置文件目录不存在: {settings_dir}")
    #             return None
                
    #         # 检查配置文件是否存在
    #         if not os.path.exists(config_path):
    #             self.logger.warning(f"显示器 {monitor_name} 配置文件不存在: {config_path}")
    #             return None

    #         with open(config_path, 'r', encoding='utf-8') as f:
    #             content = f.read().strip()
                
    #         # 按分隔符分割配置项
    #         parts = content.split('|')
    #         if len(parts) != 4:
    #             self.logger.error(f"显示器 {monitor_name} 配置文件格式错误，需要4个部分，实际有{len(parts)}个")
    #             return None
                
    #         # 解析分辨率和刷新率部分 (格式: {width}x{height}@{rate})
    #         resolution_part = parts[0].strip()
    #         if '@' not in resolution_part or 'x' not in resolution_part:
    #             self.logger.error(f"显示器 {monitor_name} 分辨率格式错误: {resolution_part}")
    #             return None
                
    #         # 分离分辨率和刷新率
    #         resolution, rate_str = resolution_part.split('@', 1)
    #         width_str, height_str = resolution.split('x', 1)
    #         rate_str = rate_str[0:-2]
    #         print(rate_str)
            
    #         # 解析位置信息 (格式: x,y)
    #         position_part = parts[1].strip()
    #         x_str, y_str = position_part.split(',', 1)
            
    #         # 解析旋转角度和主显示器标识
    #         rotation_str = parts[2].strip()
    #         is_primary_str = parts[3].strip().lower()
            
    #         # 转换为相应的数据类型
    #         return {
    #             'width': int(width_str),
    #             'height': int(height_str),
    #             'refresh_rate': float(rate_str),
    #             'x': int(x_str),
    #             'y': int(y_str),
    #             'rotation': int(rotation_str),
    #             'is_primary': is_primary_str in ('true', '1', 'yes')
    #         }
            
    #     except ValueError as e:
    #         self.logger.error(f"显示器 {monitor_name} 配置文件数值解析失败: {str(e)}")
    #     except FileNotFoundError:
    #         self.logger.warning(f"显示器 {monitor_name} 配置文件未找到: {config_path}")
    #     except Exception as e:
    #         self.logger.error(f"加载显示器 {monitor_name} 配置文件时出错: {str(e)}")
            
    #     return None
        

    def load_custom_resolution(self, monitor_name: str):
        """
        加载自定义分辨率配置文件
        
        配置文件新格式: {width}x{height}@{rate}|x,y|rotation|isPrimary|forceFlag
        例如: 1920x1080@60|1920,0|0|True|F 或 1920x1080@60|1920,0|0|True|A
        配置文件路径: 同目录下的display_settings目录，以显示器名称.conf命名
        
        Args:
            monitor_name: 显示器名称
            
        Returns:
            包含解析后的配置信息的字典，解析失败返回None
        """
        try:
            # 构建配置文件路径：同目录下的display_settings目录，文件名格式为"显示器名称.conf"
            import os
            # 获取当前文件所在目录
            current_dir = os.path.dirname(os.path.abspath(__file__))
            # 构建display_settings目录路径
            settings_dir = setting_file_dir
            # 构建完整配置文件路径
            config_path = os.path.join(settings_dir, f"{monitor_name}.conf")
            
            # 检查配置文件目录是否存在
            if not os.path.exists(settings_dir):
                self.logger.warning(f"配置文件目录不存在: {settings_dir}")
                return None
                
            # 检查配置文件是否存在
            if not os.path.exists(config_path):
                self.logger.warning(f"显示器 {monitor_name} 配置文件不存在: {config_path}")
                return None

            with open(config_path, 'r', encoding='utf-8') as f:
                content = f.read().strip()
                
            # 按分隔符分割配置项，现在可能有4或5个部分（兼容旧格式）
            parts = content.split('|')
            if len(parts) < 4:
                self.logger.error(f"显示器 {monitor_name} 配置文件格式错误，需要至少4个部分，实际有{len(parts)}个")
                return None
                
            # 解析分辨率和刷新率部分 (格式: {width}x{height}@{rate})
            resolution_part = parts[0].strip()
            if '@' not in resolution_part or 'x' not in resolution_part:
                self.logger.error(f"显示器 {monitor_name} 分辨率格式错误: {resolution_part}")
                return None
                
            # 分离分辨率和刷新率
            resolution, rate_str = resolution_part.split('@', 1)
            width_str, height_str = resolution.split('x', 1)
            rate_str = rate_str[0:-2]  # 移除"Hz"
            
            # 解析位置信息 (格式: x,y)
            position_part = parts[1].strip()
            x_str, y_str = position_part.split(',', 1)
            
            # 解析旋转角度和主显示器标识
            rotation_str = parts[2].strip()
            is_primary_str = parts[3].strip().lower()
            
            # 转换为相应的数据类型
            result = {
                'width': int(width_str),
                'height': int(height_str),
                'refresh_rate': float(rate_str),
                'x': int(x_str),
                'y': int(y_str),
                'rotation': int(rotation_str),
                'is_primary': is_primary_str in ('true', '1', 'yes', 'p')
            }
            
            # 如果有第5个部分（强制标志），则解析它
            if len(parts) >= 5:
                force_flag = parts[4].strip().upper()
                result['force_custom'] = (force_flag == 'F')
            else:
                result['force_custom'] = False  # 默认值
                
            return result
            
        except ValueError as e:
            self.logger.error(f"显示器 {monitor_name} 配置文件数值解析失败: {str(e)}")
        except FileNotFoundError:
            self.logger.warning(f"显示器 {monitor_name} 配置文件未找到: {config_path}")
        except Exception as e:
            self.logger.error(f"加载显示器 {monitor_name} 配置文件时出错: {str(e)}")
            
        return None