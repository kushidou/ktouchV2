#!/usr/bin/env python3
import os
import time
import logging
import signal
import sys
import re
import argparse
from pathlib import Path
from datetime import datetime, timedelta

# 配置日志
def setup_logging():
    apppath="/opt/ktouch"
    log_file = os.path.join(apppath, 'sub_modules.log')
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s display_monitor [%(levelname)s] %(message)s',
        handlers=[
            logging.FileHandler(log_file, encoding='utf-8'),
            logging.StreamHandler(sys.stdout)
        ]
    )
    return logging.getLogger(__name__)

logger = setup_logging()

class DisplayChecker:
    def __init__(self, display_env=":0"):
        """初始化显示器检查器"""
        self.display_env = display_env
        self.display_name = None
        self.config_data = None
        self.current_settings = None
        
    def parse_config_file(self, config_path):
        """
        解析配置文件
        格式: 分辨率@刷新率|坐标|旋转方向|是否是主屏|模式标志
        示例: 1920x1080@60.0Hz|0,0|1|P|A 或 1920x1080@60.0Hz|0,0|1|P|F
        F: 必须通过cvt生成分辨率并设置
        A: 沿用原来的方法（即xrandr存在分辨率就使用系统分辨率，否则使用cvt）
        """
        try:
            with open(config_path, 'r') as f:
                content = f.read().strip()
            
            # 从文件名获取显示器名称（去掉后缀）
            self.display_name = Path(config_path).stem
            
            # 解析配置内容
            parts = content.split('|')
            if len(parts) != 5:
                raise ValueError("配置文件格式错误，应该有5个部分")
            
            # 解析分辨率和刷新率
            res_refresh_match = re.match(r'(\d+)x(\d+)@([\d.]+)Hz', parts[0])
            if not res_refresh_match:
                raise ValueError("分辨率刷新率格式错误")
            
            width, height, refresh_rate = res_refresh_match.groups()
            
            # 解析坐标
            pos_match = re.match(r'(-?\d+),(-?\d+)', parts[1])
            if not pos_match:
                raise ValueError("坐标格式错误")
            
            pos_x, pos_y = pos_match.groups()
            
            # 解析旋转方向
            rotation_map = {'1': 'normal', '2': 'left', '4': 'inverted', '8': 'right'}
            rotation = rotation_map.get(parts[2])
            if not rotation:
                raise ValueError(f"旋转方向错误: {parts[2]}")
            
            # 解析主屏设置
            primary = parts[3] == 'P'
            
            # 解析模式标志
            mode_flag = parts[4].strip()
            if mode_flag not in ['F', 'A']:
                raise ValueError(f"模式标志错误: {mode_flag}，应该是 F 或 A")
            
            self.config_data = {
                'width': int(width),
                'height': int(height),
                'refresh_rate': float(refresh_rate),
                'pos_x': int(pos_x),
                'pos_y': int(pos_y),
                'rotation': rotation,
                'primary': primary,
                'mode_flag': mode_flag  # 新增字段
            }
            
            return True
            
        except Exception as e:
            logger.error(f"解析配置文件错误: {e}")
            return False
    
    def is_display_connected(self):
        """检查显示器是否连接"""
        env = os.environ.copy()
        env['DISPLAY'] = self.display_env
        
        try:
            import subprocess
            result = subprocess.run(
                ['xrandr'], 
                env=env, 
                capture_output=True, 
                text=True
            )
            
            if result.returncode != 0:
                logger.error("获取显示器信息失败")
                return False
            
            # 查找显示器连接状态
            lines = result.stdout.split('\n')
            for line in lines:
                if line.startswith(f'{self.display_name} '):
                    if 'connected' in line and 'disconnected' not in line:
                        return True
                    else:
                        logger.warning(f"显示器 {self.display_name} 未连接或已断开")
                        return False
            
            logger.warning(f"未找到显示器 {self.display_name}")
            return False
            
        except Exception as e:
            logger.error(f"检查显示器连接状态错误: {e}")
            return False
    
    def run_xrandr_command(self, cmd, ignore_errors=False):
        """运行xrandr命令"""
        env = os.environ.copy()
        env['DISPLAY'] = self.display_env
        
        try:
            import subprocess
            result = subprocess.run(cmd, shell=True, env=env, capture_output=True, text=True)
            if result.returncode != 0:
                if ignore_errors:
                    # 检查是否是"已经存在"之类的错误
                    error_lower = result.stderr.lower()
                    if any(keyword in error_lower for keyword in ['already exists', 'already set', 'exist']):
                        logger.info(f"忽略预期中的错误: {result.stderr.strip()}")
                        return True
                    else:
                        logger.warning(f"命令执行失败但忽略错误: {cmd}")
                        logger.warning(f"错误信息: {result.stderr}")
                        return True
                else:
                    logger.error(f"命令执行失败: {cmd}")
                    logger.error(f"错误信息: {result.stderr}")
                    return False
            return True
        except Exception as e:
            logger.error(f"执行命令错误: {e}")
            return False
    
    def get_current_display_settings(self):
        """获取当前显示器的设置"""
        # 首先检查显示器是否连接
        if not self.is_display_connected():
            return None
            
        env = os.environ.copy()
        env['DISPLAY'] = self.display_env
        
        try:
            import subprocess
            result = subprocess.run(
                ['xrandr'], 
                env=env, 
                capture_output=True, 
                text=True
            )
            
            if result.returncode != 0:
                logger.error("获取显示器信息失败")
                return None
            
            # 解析xrandr输出，找到指定显示器的当前设置
            lines = result.stdout.split('\n')
            current_settings = {}
            
            for line in lines:
                # 查找目标显示器的连接状态行
                if line.startswith(f'{self.display_name} '):
                    # 检查是否是主屏
                    current_settings['primary'] = 'primary' in line
                    
                    # 提取当前分辨率
                    res_match = re.search(r'(\d+x\d+)\+(-?\d+)\+(-?\d+)', line)
                    if res_match:
                        current_settings['resolution'] = res_match.group(1)
                        current_settings['pos_x'] = int(res_match.group(2))
                        current_settings['pos_y'] = int(res_match.group(3))
                    
                    # 提取旋转信息
                    if 'inverted' in line and 'x axis y axis' not in line:
                        current_settings['rotation'] = 'inverted'
                    elif 'left' in line and 'x axis y axis' not in line:
                        current_settings['rotation'] = 'left'
                    elif 'right' in line and 'x axis y axis' not in line:
                        current_settings['rotation'] = 'right'
                    else:
                        current_settings['rotation'] = 'normal'
                    
                    # 继续查找当前模式行以获取刷新率
                    for next_line in lines[lines.index(line)+1:]:
                        if next_line.strip() and not next_line.startswith(' '):
                            break
                        
                        # 查找当前模式（带*的）
                        if '*'+'+' in next_line or '* ' in next_line:
                            rate_match = re.search(r'(\d+\.\d+)\*', next_line)
                            if rate_match:
                                current_settings['refresh_rate'] = float(rate_match.group(1))
                            break
            
            self.current_settings = current_settings
            return current_settings
            
        except Exception as e:
            logger.error(f"获取当前显示器设置错误: {e}")
            return None
    
    def get_display_modes(self):
        """
        获取显示器支持的模式列表
        返回: 字典，键为分辨率，值为该分辨率下支持的刷新率列表
        """
        # 首先检查显示器是否连接
        if not self.is_display_connected():
            return {}
            
        env = os.environ.copy()
        env['DISPLAY'] = self.display_env
        
        try:
            import subprocess
            result = subprocess.run(
                ['xrandr'], 
                env=env, 
                capture_output=True, 
                text=True
            )
            
            if result.returncode != 0:
                logger.error("获取显示器信息失败")
                return {}
            
            # 解析xrandr输出，找到指定显示器的模式
            lines = result.stdout.split('\n')
            modes = {}
            in_target_display = False
            current_resolution = None
            
            for line in lines:
                # 检查是否进入目标显示器的部分
                if line.startswith(f'{self.display_name} '):
                    in_target_display = True
                    continue
                elif in_target_display and line.strip() and not line.startswith(' '):
                    # 新的显示器部分开始，退出
                    break
                
                if in_target_display:
                    # 解析分辨率行
                    res_match = re.search(r'^\s*(\d+x\d+i?)\s+', line)
                    if res_match:
                        current_resolution = res_match.group(1)
                        modes[current_resolution] = []
                    
                    # 解析刷新率
                    if current_resolution:
                        rate_matches = re.findall(r'(\d+\.\d+)\*?\+?', line)
                        for rate in rate_matches:
                            modes[current_resolution].append(float(rate))
            
            return modes
            
        except Exception as e:
            logger.error(f"获取显示器模式错误: {e}")
            return {}
    
    def get_existing_modelines(self):
        """获取已经存在的自定义模式"""
        env = os.environ.copy()
        env['DISPLAY'] = self.display_env
        
        try:
            import subprocess
            result = subprocess.run(
                ['xrandr'], 
                env=env, 
                capture_output=True, 
                text=True
            )
            
            if result.returncode != 0:
                return []
            
            # 查找自定义模式
            lines = result.stdout.split('\n')
            modelines = []
            capturing_modelines = True
            
            for line in lines:
                if re.match(r'^\S+ connected', line):
                    capturing_modelines = False
                    continue
                
                if capturing_modelines and line.strip():
                    mode_match = re.match(r'^\s*(\S+)\s+.*?(\d+\.\d+)\s+.*?(\d+)\s+.*?(\d+)\s+.*?(\d+)\s+.*?(\d+)\s+.*?(\d+)\s+.*?(\d+)\s+.*?(\d+)', line)
                    if mode_match:
                        modelines.append(mode_match.group(1))
            
            return modelines
            
        except Exception as e:
            logger.error(f"获取已存在模式错误: {e}")
            return []
    
    def find_matching_mode(self, modes):
        """在支持的模式中查找匹配的模式（允许刷新率误差）"""
        target_res = f"{self.config_data['width']}x{self.config_data['height']}"
        target_refresh = self.config_data['refresh_rate']
        
        # 检查精确匹配的分辨率
        if target_res in modes:
            for refresh_rate in modes[target_res]:
                # 允许±2fps的误差
                if abs(refresh_rate - target_refresh) <= 2.0:
                    return f"{target_res} {refresh_rate}"
        
        # 检查隔行扫描变体
        interlaced_res = f"{target_res}i"
        if interlaced_res in modes:
            for refresh_rate in modes[interlaced_res]:
                if abs(refresh_rate - target_refresh) <= 2.0:
                    return f"{interlaced_res} {refresh_rate}"
        
        return None
    
    def create_and_add_mode(self):
        """使用cvt创建并添加新的显示模式"""
        width = self.config_data['width']
        height = self.config_data['height']
        refresh_rate = self.config_data['refresh_rate']
        
        # 生成模式名称
        mode_name = f"{width}x{height}_{refresh_rate}"
        
        # 检查模式是否已经存在
        existing_modelines = self.get_existing_modelines()
        if mode_name in existing_modelines:
            logger.info(f"模式 {mode_name} 已经存在，跳过创建")
        else:
            # 使用cvt生成模式行
            try:
                import subprocess
                result = subprocess.run(
                    ['cvt', str(width), str(height), str(refresh_rate)], 
                    capture_output=True, 
                    text=True
                )
                
                if result.returncode != 0:
                    logger.error("cvt命令执行失败")
                    return None
                
                # 从cvt输出中提取模式行
                lines = result.stdout.split('\n')
                modeline = None
                for line in lines:
                    if line.startswith('Modeline '):
                        modeline = line.replace('Modeline ', '').strip()
                        break
                
                if not modeline:
                    logger.error("无法从cvt输出中提取模式行")
                    return None
                
                # 添加新模式
                add_mode_cmd = f"xrandr --newmode {mode_name} {modeline.split(' ', 1)[1]}"
                if not self.run_xrandr_command(add_mode_cmd, ignore_errors=True):
                    return None
                logger.info(f"成功创建新模式: {mode_name}")
                
            except Exception as e:
                logger.error(f"创建模式错误: {e}")
                return None
        
        # 将新模式添加到显示器
        add_to_output_cmd = f"xrandr --addmode {self.display_name} {mode_name}"
        if not self.run_xrandr_command(add_to_output_cmd, ignore_errors=True):
            return None
        
        return mode_name
    
    def parse_resolution(self, resolution_str):
        """解析分辨率字符串，返回宽高元组"""
        try:
            if 'x' in resolution_str:
                width, height = resolution_str.split('x')
                height = height.replace('i', '')
                return int(width), int(height)
            return 0, 0
        except:
            return 0, 0
    
    def compare_settings(self):
        """比较当前设置与配置设置"""
        if not self.current_settings or not self.config_data:
            return False, "无法获取当前设置或配置数据"
        
        # 检查分辨率（允许±10像素误差）
        current_res = self.current_settings.get('resolution', '')
        current_width, current_height = self.parse_resolution(current_res)
        target_width = self.config_data['width']
        target_height = self.config_data['height']
        
        width_diff = abs(current_width - target_width)
        height_diff = abs(current_height - target_height)
        
        if width_diff > 10 or height_diff > 10:
            return False, f"分辨率不匹配: 当前 {current_res}, 配置 {target_width}x{target_height}"
        
        # 检查刷新率（允许±2fps误差）
        current_refresh = self.current_settings.get('refresh_rate', 0)
        target_refresh = self.config_data['refresh_rate']
        refresh_diff = abs(current_refresh - target_refresh)
        
        if refresh_diff > 2.0:
            return False, f"刷新率不匹配: 当前 {current_refresh:.1f}Hz, 配置 {target_refresh:.1f}Hz"
        
        # 检查旋转
        current_rotation = self.current_settings.get('rotation', 'normal')
        if current_rotation != self.config_data['rotation']:
            return False, f"旋转不匹配: 当前 {current_rotation}, 配置 {self.config_data['rotation']}"
        
        # 检查主屏设置
        current_primary = self.current_settings.get('primary', False)
        if current_primary != self.config_data['primary']:
            primary_status_current = "是" if current_primary else "否"
            primary_status_target = "是" if self.config_data['primary'] else "否"
            return False, f"主屏设置不匹配: 当前 {primary_status_current}, 配置 {primary_status_target}"
        
        # 位置不进行校验
        match_details = [
            f"分辨率: {current_res}",
            f"刷新率: {current_refresh:.1f}Hz",
            f"旋转: {current_rotation}",
            f"主屏: {'是' if current_primary else '否'}",
            "位置: 跳过校验"
        ]
        
        return True, " | ".join(match_details)
    
    def configure_display(self):
        """配置显示器"""
        if not self.config_data:
            logger.error("没有可用的配置数据")
            return False
        
        # 检查显示器是否连接
        if not self.is_display_connected():
            logger.warning(f"显示器 {self.display_name} 未连接，跳过配置")
            return True  # 返回True表示跳过而不是失败
        
        # 获取当前支持的模式
        modes = self.get_display_modes()
        if not modes:
            logger.error(f"无法获取显示器 {self.display_name} 的模式信息")
            return False
        
        logger.info(f"显示器 {self.display_name} 支持的模式:")
        for res, rates in modes.items():
            logger.info(f"  {res}: {rates}")
        
        # 根据模式标志决定行为
        mode_flag = self.config_data.get('mode_flag', 'A')  # 默认为A
        
        if mode_flag == 'F':
            # F模式：必须使用cvt生成分辨率
            logger.info("模式标志为F，强制使用cvt生成分辨率")
            mode_name = self.create_and_add_mode()
            if not mode_name:
                logger.error("创建新模式失败")
                return False
        else:
            # A模式：沿用原来的方法
            logger.info("模式标志为A，使用自动模式选择")
            mode_name = self.find_matching_mode(modes)
            
            # 如果不支持，创建新模式
            if not mode_name:
                logger.info("显示器不支持该模式，尝试创建新模式...")
                mode_name = self.create_and_add_mode()
                if not mode_name:
                    logger.error("创建新模式失败")
                    return False
            else:
                logger.info(f"找到匹配模式: {mode_name}")
        
        # 构建xrandr命令
        cmd_parts = ["xrandr", f"--output {self.display_name}"]
        
        # 添加模式
        if ' ' in mode_name:
            resolution, rate = mode_name.split(' ', 1)
            cmd_parts.append(f"--mode {resolution}")
            cmd_parts.append(f"--rate {rate}")
        else:
            cmd_parts.append(f"--mode {mode_name}")
        
        # 添加位置
        cmd_parts.append(f"--pos {self.config_data['pos_x']}x{self.config_data['pos_y']}")
        
        # 添加旋转
        cmd_parts.append(f"--rotate {self.config_data['rotation']}")
        
        # 如果是主屏
        if self.config_data['primary']:
            cmd_parts.append("--primary")
        
        # 启用显示器
        cmd_parts.append("--auto")
        
        # 执行最终配置命令
        final_cmd = " ".join(cmd_parts)
        logger.info(f"执行命令: {final_cmd}")
        
        if self.run_xrandr_command(final_cmd):
            logger.info("显示器配置成功!")
            return True
        else:
            logger.error("显示器配置失败!")
            return False
    
    def check_and_configure(self, config_file_path, is_first=False):
        """检查并配置显示器 - 主要入口方法"""
        # 解析配置文件
        if not self.parse_config_file(config_file_path):
            logger.error(f"解析配置文件失败: {config_file_path}")
            return False
        
        # 检查显示器是否连接
        if not self.is_display_connected():
            logger.warning(f"显示器 {self.display_name} 未连接，跳过配置")
            return True  # 返回True表示跳过而不是失败

        # 如果是首次运行，无视显示器配置
        if is_first:
            # logger.info("启动程序后首次设置,开始配置显示器")
            return self.configure_display()
        
        # 获取当前设置
        current_settings = self.get_current_display_settings()
        if not current_settings:
            logger.warning("无法获取当前显示器设置，尝试直接配置...")
            return self.configure_display()
        
        logger.info("当前显示器设置:")
        for key, value in current_settings.items():
            logger.info(f"  {key}: {value}")
        
        # 比较设置
        match, message = self.compare_settings()
        
        if match:
            logger.info(f"✓ 设置匹配: {message}")
            return True
        else:
            logger.info(f"✗ 设置不匹配: {message}")
            logger.info("开始配置显示器...")
            return self.configure_display()


class FileMonitor:
    def __init__(self):
        self.os_version_file = Path("/etc/os-version")
        self.screen_file = Path("/tmp/ktouch/screen.txt")
        self.settings_dir = Path("/opt/ktouch/display_config")
        self.display_update_file = Path("/tmp/ktouch/display_update.txt")
        
        # 存储文件最后修改时间和内容
        self.last_mod_time = None
        self.last_content = None  # 新增：存储文件内容
        self.last_display_update_time = 0
        
        # 防止重复触发的机制
        self.last_operation_time = None
        self.cooldown_period = 15  # 冷却时间15秒
        
        # 设置信号处理，用于优雅退出
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGTERM, self.signal_handler)
        
        self.running = True
        self.display_checker = DisplayChecker()

    def signal_handler(self, signum, frame):
        """处理中断信号"""
        logger.info(f"接收到信号 {signum}，正在退出...")
        self.running = False

    def check_os_version(self):
        """检查 /etc/os-version 文件是否存在"""
        if not self.os_version_file.exists():
            logger.error(f"文件 {self.os_version_file} 不存在，退出脚本")
            sys.exit(1)
        logger.info(f"检测到 {self.os_version_file} 文件，继续执行")

    def wait_for_screen_file(self):
        """等待 screen.txt 文件生成"""
        while not self.screen_file.exists() and self.running:
            logger.info(f"等待 {self.screen_file} 文件生成...")
            time.sleep(5)
        
        if not self.running:
            return False
            
        # 获取初始修改时间和内容
        self.last_mod_time = self.screen_file.stat().st_mtime
        self.last_content = self.read_file_content(self.screen_file)
        logger.info(f"检测到 {self.screen_file} 文件，初始修改时间: {time.ctime(self.last_mod_time)}")
        return True

    def read_file_content(self, file_path):
        """读取文件内容"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                return f.read().strip()
        except Exception as e:
            logger.error(f"读取文件内容失败 {file_path}: {e}")
            return None

    def is_in_cooldown_period(self):
        """检查是否在冷却期内"""
        if self.last_operation_time is None:
            return False
        
        elapsed_time = time.time() - self.last_operation_time
        return elapsed_time < self.cooldown_period

    def update_file_modification_time(self):
        """更新文件修改时间（用于防止重复触发）"""
        try:
            current_time = time.time()
            os.utime(self.screen_file, (current_time, current_time))
            self.last_mod_time = current_time
            logger.info(f"已更新文件修改时间以防止重复触发")
            return True
        except Exception as e:
            logger.error(f"更新文件修改时间失败: {e}")
            return False

    def check_file_modified(self):
        """检查文件是否被修改（同时检查修改时间和内容）"""
        if not self.screen_file.exists():
            logger.warning(f"文件 {self.screen_file} 不存在")
            return False
        
        # 检查是否在冷却期内
        if self.is_in_cooldown_period():
            logger.info("在冷却期内，跳过文件修改检查")
            return False
        
        try:
            current_mod_time = self.screen_file.stat().st_mtime
            current_content = self.read_file_content(self.screen_file)
            
            # 如果无法读取内容，则跳过
            if current_content is None:
                return False
            
            # 检查修改时间是否变化
            if current_mod_time != self.last_mod_time:
                logger.info(f"检测到文件修改时间变化 - 原时间: {time.ctime(self.last_mod_time)}, 新时间: {time.ctime(current_mod_time)}")
                
                # 检查内容是否变化
                if current_content != self.last_content:
                    logger.info("检测到文件内容实际发生变化，需要执行分辨率设置")
                    self.last_mod_time = current_mod_time
                    self.last_content = current_content
                    return True
                else:
                    logger.info("文件修改时间变化但内容未变，只更新修改时间")
                    self.last_mod_time = current_mod_time
                    # 更新文件修改时间防止重复触发
                    self.update_file_modification_time()
                    return False
                    
        except OSError as e:
            logger.error(f"无法获取文件状态: {e}")
            
        return False

    def check_display_update_file(self):
        """检查display_update.txt文件内容是否为1"""
        if not self.display_update_file.exists():
            # 文件不存在，忽略检测
            return False
        
        try:
            # 检查文件修改时间，避免重复处理
            current_mtime = self.display_update_file.stat().st_mtime
            if current_mtime <= self.last_display_update_time:
                return False
                
            self.last_display_update_time = current_mtime
            
            content = self.read_file_content(self.display_update_file)
            if content == "1":
                logger.info(f"检测到 {self.display_update_file} 文件内容为1，立即进行分辨率设置")
                
                # 将文件内容改为0
                try:
                    with open(self.display_update_file, 'w') as f:
                        f.write("0")
                    logger.info(f"已将 {self.display_update_file} 文件内容修改为0")
                except Exception as e:
                    logger.error(f"修改display_update.txt文件失败: {e}")
                
                return True
                
        except Exception as e:
            logger.error(f"检查display_update.txt文件失败: {e}")
            
        return False

    def execute_display_settings(self, is_first=False):
        """执行 display_settings 文件夹中的所有文件 - 直接调用方法"""
        # 检查目录是否存在
        if not self.settings_dir.exists():
            logger.error(f"目录 {self.settings_dir} 不存在")
            return False
        
        # 获取目录中的所有文件
        setting_files = list(self.settings_dir.glob("*"))
        if not setting_files:
            logger.warning(f"目录 {self.settings_dir} 中没有文件")
            return True
        
        success_count = 0
        total_count = len(setting_files)
        skipped_count = 0
        
        for file_path in setting_files:
            if file_path.is_file():
                try:
                    logger.info(f"处理显示器配置文件: {file_path}")
                    
                    # 直接调用DisplayChecker的方法，而不是通过子进程
                    result = self.display_checker.check_and_configure(str(file_path), is_first)
                    if result:
                        logger.info(f"成功处理: {file_path}")
                        success_count += 1
                    else:
                        # 检查是否是跳过（显示器未连接）
                        if not self.display_checker.is_display_connected():
                            logger.warning(f"跳过未连接的显示器: {file_path}")
                            skipped_count += 1
                        else:
                            logger.error(f"处理失败: {file_path}")
                        
                except Exception as e:
                    logger.error(f"处理异常: {file_path}, 异常: {e}")
        
        logger.info(f"执行完成: 成功 {success_count}, 跳过 {skipped_count}, 总计 {total_count} 个文件")
        
        # 记录操作时间并更新文件修改时间
        self.last_operation_time = time.time()
        
        # 等待15秒后更新文件修改时间，防止重复触发
        logger.info(f"等待 {self.cooldown_period} 秒后更新文件修改时间...")
        time.sleep(self.cooldown_period)
        
        if self.running:
            self.update_file_modification_time()
        
        return success_count > 0  # 只要有一个成功就返回True

    def initial_resolution_check(self):
        """初始分辨率检查 - 检查十次，每次间隔15秒"""
        logger.info("开始初始分辨率检查流程...")
        
        # 检查目录是否存在
        if not self.settings_dir.exists():
            logger.error(f"目录 {self.settings_dir} 不存在")
            return False
        
        # 获取目录中的所有文件
        setting_files = list(self.settings_dir.glob("*"))
        if not setting_files:
            logger.warning(f"目录 {self.settings_dir} 中没有文件")
            return True
        
        # 进行十次检查，每次间隔15秒
        max_attempts = 10
        check_interval = 15  # 秒
        
        for attempt in range(1, max_attempts + 1):
            if not self.running:
                logger.info("程序正在退出，终止初始分辨率检查")
                return False
                
            logger.info(f"初始分辨率检查 - 第 {attempt}/{max_attempts} 次")
            
            success_count = 0
            total_count = len(setting_files)
            need_adjustment = False
            
            for file_path in setting_files:
                if not self.running:
                    break
                    
                if file_path.is_file():
                    try:
                        logger.info(f"检查显示器配置文件: {file_path}")
                        
                        # 为每个文件创建新的DisplayChecker实例
                        display_checker = DisplayChecker()
                        
                        # 解析配置文件
                        if not display_checker.parse_config_file(str(file_path)):
                            logger.error(f"解析配置文件失败: {file_path}")
                            continue
                        
                        # 检查显示器是否连接
                        if not display_checker.is_display_connected():
                            logger.warning(f"显示器 {display_checker.display_name} 未连接，跳过检查")
                            continue
                        
                        # 获取当前设置
                        current_settings = display_checker.get_current_display_settings()
                        if not current_settings:
                            logger.warning(f"无法获取显示器 {display_checker.display_name} 的当前设置")
                            need_adjustment = True
                            continue
                        
                        # 比较设置
                        match, message = display_checker.compare_settings()
                        
                        if match:
                            logger.info(f"✓ 显示器 {display_checker.display_name} 设置匹配: {message}")
                            success_count += 1
                        else:
                            logger.info(f"✗ 显示器 {display_checker.display_name} 设置不匹配: {message}")
                            need_adjustment = True
                            
                            # 立即进行配置
                            logger.info(f"立即配置显示器 {display_checker.display_name}...")
                            if display_checker.configure_display():
                                logger.info(f"显示器 {display_checker.display_name} 配置成功")
                                success_count += 1
                            else:
                                logger.error(f"显示器 {display_checker.display_name} 配置失败")
                                
                    except Exception as e:
                        logger.error(f"处理异常: {file_path}, 异常: {e}")
            
            # 检查是否所有显示器都配置正确
            if success_count == total_count:
                logger.info(f"✓ 第 {attempt} 次检查完成: 所有显示器配置正确")
                break
            else:
                logger.info(f"第 {attempt} 次检查完成: {success_count}/{total_count} 个显示器配置正确")
                
                # 如果不是最后一次检查，等待间隔时间
                if attempt < max_attempts:
                    if need_adjustment:
                        logger.info(f"等待 {check_interval} 秒后进行下一次检查...")
                        for i in range(check_interval):
                            if not self.running:
                                break
                            time.sleep(1)
        
        logger.info("初始分辨率检查流程完成")
        return True

    def monitor(self):
        """主监控循环"""
        logger.info("启动文件监控脚本...")
        
        # 检查os-version文件
        self.check_os_version()

        logger.info("进行首次分辨率设置")
        self.execute_display_settings(is_first=True)
        time.sleep(10)
        
        # 等待screen文件生成
        if not self.wait_for_screen_file():
            return

        # 在screen.txt生成后，进行初始分辨率检查
        logger.info("screen.txt文件已生成，开始初始分辨率检查...")
        self.initial_resolution_check()
        
        logger.info("开始监控文件变化...")
        
        while self.running:
            try:
                # 检查display_update.txt文件
                if self.check_display_update_file():
                    logger.info("检测到display_update.txt触发，立即执行分辨率设置...")
                    self.execute_display_settings()
                    logger.info("display_update.txt触发操作完成，继续监控...")
                
                # 检查screen.txt文件是否被修改
                if self.check_file_modified():
                    logger.info("检测到screen.txt文件内容变化，执行display_settings中的文件...")
                    self.execute_display_settings()
                    logger.info("screen.txt变化操作完成，继续监控...")
                
                # 等待1秒后继续检查
                time.sleep(1)
                    
            except KeyboardInterrupt:
                logger.info("用户中断监控")
                break
            except Exception as e:
                logger.error(f"监控过程中发生异常: {e}")
                time.sleep(10)
        
        logger.info("监控脚本已停止")


def main():
    """主函数"""
    monitor = FileMonitor()
    monitor.monitor()


if __name__ == "__main__":
    main()