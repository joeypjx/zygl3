#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BMC UDP 组播报文发送脚本
创建完整的BMC UDP报文（16进制表示）并发送到组播组 224.100.200.15:5715
"""

import socket
import struct
import time
import sys
import signal

# 组播地址和端口
MULTICAST_GROUP = "224.100.200.15"
PORT = 5715

# 报文结构大小（字节）
# UdpFanInfo: 1 + 1 + 4 = 6字节
# UdpSensorInfo: 1 + 1 + 6 + 1 + 1 + 1 + 1 = 12字节
# UdpPowerBoardInfo: 1 + 2 + 2 + 8 + 8 + 8 + 1 + 1 + 12*8 + 1 = 128字节
# UdpSlotBoardInfo: 1 + 2 + 1 + 2 + 8 + 8 + 8 + 1 + 1 + 12*8 + 2 = 130字节
# UdpInfo: 2 + 2 + 2 + 2 + 4 + 2 + 2 + 1 + 1 + 6*6 + 2*128 + 10*130 + 2 = 1612字节

def create_udp_fan_info(fanseq, fanmode, fanspeed):
    """创建风扇信息 (6字节)"""
    return struct.pack('<BBI', fanseq, fanmode, fanspeed)

def create_udp_sensor_info(sensorseq, sensortype, sensorname, sensorvalue_L, sensorvalue_H, sensoralmtype):
    """创建传感器信息 (12字节)"""
    # sensorname 是6字节的字符串
    name_bytes = sensorname.encode('ascii')[:6].ljust(6, b'\x00')
    return struct.pack('<BB6sBBBB', sensorseq, sensortype, name_bytes, 
                      sensorvalue_L, sensorvalue_H, sensoralmtype, 0)

def create_udp_power_board_info(ipmbaddr, moduletype, bmccompany, bmcversion, 
                                 snnum, protime, status, sensornum, sensors):
    """创建电源板信息 (128字节)"""
    data = bytearray(128)
    offset = 0
    
    # ipmbaddr (1字节)
    struct.pack_into('<B', data, offset, ipmbaddr)
    offset += 1
    
    # moduletype (2字节)
    struct.pack_into('<H', data, offset, moduletype)
    offset += 2
    
    # bmccompany (2字节)
    struct.pack_into('<H', data, offset, bmccompany)
    offset += 2
    
    # bmcversion (8字节)
    version_bytes = bmcversion.encode('ascii')[:8].ljust(8, b'\x00')
    data[offset:offset+8] = version_bytes
    offset += 8
    
    # snnum (8字节)
    sn_bytes = snnum.encode('ascii')[:8].ljust(8, b'\x00')
    data[offset:offset+8] = sn_bytes
    offset += 8
    
    # protime (8字节)
    pro_bytes = protime.encode('ascii')[:8].ljust(8, b'\x00')
    data[offset:offset+8] = pro_bytes
    offset += 8
    
    # status (1字节)
    struct.pack_into('<B', data, offset, status)
    offset += 1
    
    # sensornum (1字节)
    struct.pack_into('<B', data, offset, sensornum)
    offset += 1
    
    # sensor[8] (12*8 = 96字节)
    for i, sensor in enumerate(sensors[:8]):
        sensor_data = create_udp_sensor_info(
            sensor.get('sensorseq', 0xFF),
            sensor.get('sensortype', 0),
            sensor.get('sensorname', ''),
            sensor.get('sensorvalue_L', 0),
            sensor.get('sensorvalue_H', 0),
            sensor.get('sensoralmtype', 0)
        )
        data[offset:offset+12] = sensor_data
        offset += 12
    
    # resv[1] (1字节)
    # 已经是0，不需要设置
    
    return bytes(data)

def create_udp_slot_board_info(ipmbaddr, moduletype, prst, bmccompany, bmcversion,
                                snnum, protime, status, sensornum, sensors):
    """创建负载板信息 (130字节)"""
    data = bytearray(130)
    offset = 0
    
    # ipmbaddr (1字节)
    struct.pack_into('<B', data, offset, ipmbaddr)
    offset += 1
    
    # moduletype (2字节)
    struct.pack_into('<H', data, offset, moduletype)
    offset += 2
    
    # prst (1字节)
    struct.pack_into('<B', data, offset, prst)
    offset += 1
    
    # bmccompany (2字节)
    struct.pack_into('<H', data, offset, bmccompany)
    offset += 2
    
    # bmcversion (8字节)
    version_bytes = bmcversion.encode('ascii')[:8].ljust(8, b'\x00')
    data[offset:offset+8] = version_bytes
    offset += 8
    
    # snnum (8字节)
    sn_bytes = snnum.encode('ascii')[:8].ljust(8, b'\x00')
    data[offset:offset+8] = sn_bytes
    offset += 8
    
    # protime (8字节)
    pro_bytes = protime.encode('ascii')[:8].ljust(8, b'\x00')
    data[offset:offset+8] = pro_bytes
    offset += 8
    
    # status (1字节)
    struct.pack_into('<B', data, offset, status)
    offset += 1
    
    # sensornum (1字节)
    struct.pack_into('<B', data, offset, sensornum)
    offset += 1
    
    # sensor[8] (12*8 = 96字节)
    for i, sensor in enumerate(sensors[:8]):
        sensor_data = create_udp_sensor_info(
            sensor.get('sensorseq', 0xFF),
            sensor.get('sensortype', 0),
            sensor.get('sensorname', ''),
            sensor.get('sensorvalue_L', 0),
            sensor.get('sensorvalue_H', 0),
            sensor.get('sensoralmtype', 0)
        )
        data[offset:offset+12] = sensor_data
        offset += 12
    
    # resv[2] (2字节)
    # 已经是0，不需要设置
    
    return bytes(data)

def create_bmc_udp_packet(boxid=1, seqnum=1, moduletype=0x0001, presence_pattern=None):
    """创建完整的BMC UDP报文 (1612字节)
    @param presence_pattern: 在位信息模式，如果为None则基于seqnum生成随机模式
    """
    packet = bytearray(1612)
    offset = 0
    
    # head (2字节) - 0x5AA5
    struct.pack_into('<H', packet, offset, 0x5AA5)
    offset += 2
    
    # msglenth (2字节) - 报文总长度1612
    struct.pack_into('<H', packet, offset, 1612)
    offset += 2
    
    # seqnum (2字节) - 报文编号
    struct.pack_into('<H', packet, offset, seqnum)
    offset += 2
    
    # msgtype (2字节) - 0x0002
    struct.pack_into('<H', packet, offset, 0x0002)
    offset += 2
    
    # timestamp (4字节) - 当前时间戳（秒）
    timestamp = int(time.time())
    struct.pack_into('<I', packet, offset, timestamp)
    offset += 4
    
    # moduletype (2字节)
    struct.pack_into('<H', packet, offset, moduletype)
    offset += 2
    
    # recv[2] (2字节) - 备用，填充0
    offset += 2
    
    # boxname (1字节) - 固定1
    struct.pack_into('<B', packet, offset, 1)
    offset += 1
    
    # boxid (1字节) - 机箱号
    struct.pack_into('<B', packet, offset, boxid)
    offset += 1
    
    # fan[6] (6*6 = 36字节)
    for i in range(6):
        fan_data = create_udp_fan_info(
            fanseq=i,  # 风扇序号0-5
            fanmode=0x00,  # 模式：高4位告警类型，低4位工作模式
            fanspeed=50  # 转速（占空比）
        )
        packet[offset:offset+6] = fan_data
        offset += 6
    
    # dyboard[2] (2*128 = 256字节) - 电源板
    for i in range(2):
        # 创建示例传感器数据
        sensors = []
        for j in range(5):  # 5个传感器
            sensors.append({
                'sensorseq': j,
                'sensortype': 1,  # 温度传感器
                'sensorname': f'TEMP{j}',
                'sensorvalue_L': 0,
                'sensorvalue_H': 25 + j,  # 温度值
                'sensoralmtype': 0
            })
        
        power_board_data = create_udp_power_board_info(
            ipmbaddr=i+1,  # 槽位1,2
            moduletype=0x0100 + i,
            bmccompany=0x1234,
            bmcversion='1.0.0',
            snnum=f'PWR{i+1:06d}',
            protime='20240101',
            status=0,
            sensornum=5,
            sensors=sensors
        )
        packet[offset:offset+128] = power_board_data
        offset += 128
    
    # board[10] (10*130 = 1300字节) - 负载板
    # 注意：协议中负载槽顺序是：槽1、槽2、槽3、槽4、槽6、槽7、槽9、槽10、槽11、槽12
    slot_numbers = [1, 2, 3, 4, 6, 7, 9, 10, 11, 12]
    
    # 生成在位信息模式
    if presence_pattern is None:
        # 基于seqnum生成不同的在位模式（使用位操作和模运算）
        # 这样每次seqnum不同时，在位信息也会不同
        import random
        random.seed(seqnum)  # 使用seqnum作为随机种子，确保可重复但不同
        presence_pattern = {}
        for slot_num in slot_numbers:
            # 80%的概率在位，20%的概率不在位
            presence_pattern[slot_num] = 1 if random.random() < 0.8 else 0
    
    for i, slot_num in enumerate(slot_numbers):
        # 创建示例传感器数据
        sensors = []
        for j in range(6):  # 6个传感器
            sensors.append({
                'sensorseq': j,
                'sensortype': 1,  # 温度传感器
                'sensorname': f'TEMP{j}',
                'sensorvalue_L': 0,
                'sensorvalue_H': 30 + j,  # 温度值
                'sensoralmtype': 0
            })
        
        # 从presence_pattern获取在位信息，如果没有则默认为在位
        prst = presence_pattern.get(slot_num, 1)
        
        slot_board_data = create_udp_slot_board_info(
            ipmbaddr=slot_num,
            moduletype=0x0200 + i,
            prst=prst,  # 在位信息（根据模式变化）
            bmccompany=0x5678,
            bmcversion='2.0.0',
            snnum=f'SLOT{slot_num:02d}',
            protime='20240101',
            status=0,
            sensornum=6,
            sensors=sensors
        )
        packet[offset:offset+130] = slot_board_data
        offset += 130
    
    # tail (2字节) - 0xA55A
    struct.pack_into('<H', packet, offset, 0xA55A)
    
    return bytes(packet)

def send_multicast_packet(packet, multicast_group, port):
    """发送UDP组播报文"""
    # 创建UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # 设置socket选项，允许发送组播
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    
    try:
        # 发送到组播地址
        sock.sendto(packet, (multicast_group, port))
        print(f"✓ 成功发送BMC UDP报文到 {multicast_group}:{port}")
        print(f"  报文大小: {len(packet)} 字节")
        return True
    except Exception as e:
        print(f"✗ 发送失败: {e}")
        return False
    finally:
        sock.close()

def print_hex_packet(packet, max_bytes=64):
    """打印报文的16进制表示（前N字节）"""
    hex_str = ' '.join(f'{b:02X}' for b in packet[:max_bytes])
    print(f"\n报文前{max_bytes}字节（16进制）:")
    print(hex_str)
    if len(packet) > max_bytes:
        print(f"... (总共 {len(packet)} 字节)")

# 全局变量，用于优雅退出
running = True

def signal_handler(sig, frame):
    """处理Ctrl+C信号"""
    global running
    print("\n\n收到退出信号，正在停止...")
    running = False

def main():
    """主函数"""
    global running
    
    # 注册信号处理器
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    print("=" * 60)
    print("BMC UDP 组播报文发送工具")
    print("=" * 60)
    
    # 解析命令行参数
    boxid = 1
    seqnum = 1
    if len(sys.argv) > 1:
        try:
            boxid = int(sys.argv[1])
        except ValueError:
            print(f"警告: 无效的机箱号 '{sys.argv[1]}', 使用默认值 1")
    if len(sys.argv) > 2:
        try:
            seqnum = int(sys.argv[2])
        except ValueError:
            print(f"警告: 无效的报文编号 '{sys.argv[2]}', 使用默认值 1")
    
    print(f"\n配置:")
    print(f"  组播地址: {MULTICAST_GROUP}")
    print(f"  端口: {PORT}")
    print(f"  机箱号: {boxid}")
    print(f"  初始报文编号: {seqnum}")
    print(f"  发送间隔: 5秒")
    print(f"\n提示: 按 Ctrl+C 停止发送\n")
    
    # 只在第一次打印报文16进制表示
    first_packet = True
    
    # 循环发送报文
    while running:
        # 创建BMC UDP报文（每次seqnum不同，负载板卡的在位信息会自动变化）
        packet = create_bmc_udp_packet(boxid=boxid, seqnum=seqnum)
        
        # 只在第一次打印报文16进制表示
        if first_packet:
            print_hex_packet(packet, max_bytes=64)
            first_packet = False
        
        # 发送报文
        timestamp = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
        print(f"\n[{timestamp}] 正在发送报文 (编号: {seqnum})...")
        success = send_multicast_packet(packet, MULTICAST_GROUP, PORT)
        
        if success:
            print(f"  ✓ 发送成功!")
        else:
            print(f"  ✗ 发送失败!")
            break
        
        # 递增报文编号（循环到65535后回到1）
        seqnum = (seqnum % 65535) + 1
        
        # 等待5秒（检查running标志，允许提前退出）
        for _ in range(50):  # 5秒 = 50 * 0.1秒
            if not running:
                break
            time.sleep(0.1)
    
    print(f"\n\n发送已停止。")

if __name__ == "__main__":
    main()

