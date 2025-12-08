#!/usr/bin/env python3
"""
Mock API Server for /api/v1/external/qyw/boardinfo and /api/v1/external/qyw/stackinfo
使用 Python 标准库实现，无需安装额外依赖
"""

from http.server import HTTPServer, BaseHTTPRequestHandler
import json
import random
from urllib.parse import urlparse


def generate_board_ip_address(chassis_num, board_num):
    """
    根据机箱号和板卡槽位号生成IP地址
    符合 chassis_config.json 中的IP地址模式
    """
    if board_num <= 7:
        # 槽位1-7: 使用 chassisNumber * 2 作为第三段
        third_octet = chassis_num * 2
        if board_num == 6:
            # 槽位6: 第四段固定为170
            fourth_octet = 170
        elif board_num == 7:
            # 槽位7: 第四段固定为180
            fourth_octet = 180
        else:
            # 槽位1-5: 使用公式计算
            fourth_octet = (board_num - 1) * 32 + 5
    elif board_num == 13:
        # 槽位13: 第三段 = chassisNumber * 2, 第四段 = 182
        third_octet = chassis_num * 2
        fourth_octet = 182
    elif board_num == 14:
        # 槽位14: 第三段 = chassisNumber * 2, 第四段 = 183
        third_octet = chassis_num * 2
        fourth_octet = 183
    else:
        # 槽位8-12: 使用 chassisNumber * 2 + 1 作为第三段
        third_octet = chassis_num * 2 + 1
        fourth_octet = (board_num - 8) * 32 + 5
    
    return f"192.168.{third_octet}.{fourth_octet}"


# 全局变量：存储生成的数据，确保两个接口返回一致的任务信息
# 初始时 stacks 为空，只有在调用 deploy API 后才会创建
_shared_data = {
    "stacks": [],  # 初始为空列表
    "boards": None
}


def generate_mock_stack_data():
    """生成模拟业务链路数据"""
    stacks = []
    
    # 生成多个业务链路
    for stack_idx in range(1, 6):  # 5个业务链路
        stack = {
            "stackName": f"Stack_{stack_idx}",
            "stackUUID": f"stack-uuid-{stack_idx}",
            "stackLabelInfos": [
                {
                    "satckLabelName": f"Label_{stack_idx}_{label_idx}",
                    "satckLabelUUID": f"label-uuid-{stack_idx}-{label_idx}"
                }
                for label_idx in range(1, random.randint(2, 4))  # 1-3个标签
            ],
            "stackDelpoyStatus": random.choice([0, 1]),  # 0-未部署；1-已部署
            "stackRunningStatus": random.choice([1, 2]),  # 1-正常运行；2-异常运行
            "serviceInfos": []
        }
        
        # 每个业务链路有多个组件
        for service_idx in range(1, random.randint(2, 5)):  # 1-4个组件
            service = {
                "serviceName": f"Service_{stack_idx}_{service_idx}",
                "serviceUUID": f"service-uuid-{stack_idx}-{service_idx}",
                "serviceStatus": random.choice([0, 1, 2, 3]),  # 0-已停用；1-已启用；2-运行正常；3-运行异常
                "serviceType": random.choice([0, 1, 2]),  # 0-普通；1-公共链路引用；2-公共链路自有
                "taskInfos": []
            }
            
            # 每个组件有多个任务
            if service["serviceStatus"] in [1, 2, 3]:  # 如果组件已启用或运行中
                task_count = random.randint(1, 4)
                for task_idx in range(task_count):
                    # 随机分配板卡位置
                    chassis_num = random.randint(1, 9)
                    board_num = random.randint(1, 14)  # 支持1-14个槽位
                    
                    # 生成资源使用情况
                    cpu_cores = round(random.uniform(2.0, 16.0), 2)
                    cpu_used = round(random.uniform(0.5, cpu_cores * 0.8), 2)
                    cpu_usage = round((cpu_used / cpu_cores) * 100, 2) if cpu_cores > 0 else 0
                    
                    memory_size = round(random.uniform(4.0, 64.0), 2)
                    memory_used = round(random.uniform(1.0, memory_size * 0.8), 2)
                    memory_usage = round((memory_used / memory_size) * 100, 2) if memory_size > 0 else 0
                    
                    task = {
                        "taskID": f"task-{stack_idx}-{service_idx}-{task_idx+1}",
                        "taskStatus": random.choice([1, 2, 3, 0]),  # 1-运行中, 2-已完成, 3-异常, 0-其他
                        "cpuCores": cpu_cores,
                        "cpuUsed": cpu_used,
                        "cpuUsage": cpu_usage,
                        "memorySize": memory_size,
                        "memoryUsed": memory_used,
                        "memoryUsage": memory_usage,
                        "netReceive": round(random.uniform(0.0, 1000.0), 2),  # MB/s
                        "netSent": round(random.uniform(0.0, 1000.0), 2),  # MB/s
                        "gpuMemUsed": round(random.uniform(0.0, 24.0), 2) if random.random() > 0.5 else 0.0,  # GB
                        "chassisName": f"Chassis_{chassis_num}",
                        "chassisNumber": chassis_num,
                        "boardName": f"Board_{chassis_num}_{board_num}",
                        "boardNumber": board_num,
                        "boardAddress": generate_board_ip_address(chassis_num, board_num)
                    }
                    service["taskInfos"].append(task)
            
            stack["serviceInfos"].append(service)
        
        stacks.append(stack)
    
    return stacks


def generate_mock_board_data_from_stacks(stacks):
    """
    根据 stackinfo 数据生成 boardinfo 数据
    确保任务信息在两个接口中保持一致
    """
    # 初始化板卡数据结构
    boards_dict = {}  # key: (chassis_num, board_num), value: board dict
    
    # 创建所有板卡
    for chassis_num in range(1, 10):  # 9个机箱 (1-9)
        chassis_name = f"Chassis_{chassis_num}"
        for board_num in range(1, 15):  # 每个机箱14个板卡 (1-14)
            key = (chassis_num, board_num)
            boards_dict[key] = {
                "chassisName": chassis_name,
                "chassisNumber": chassis_num,
                "boardName": f"Board_{chassis_num}_{board_num}",
                "boardNumber": board_num,
                "boardType": random.choice([0, 1, 2, 3, 4, 5, 6]),  # 板卡类型
                "boardAddress": generate_board_ip_address(chassis_num, board_num),
                "boardStatus": random.choice([0, 1]),  # 0-正常, 1-异常
                "voltage": round(random.uniform(11.5, 13.5), 2),
                "current": round(random.uniform(1.5, 3.0), 2),
                "temperature": round(random.uniform(35.0, 55.0), 2),
                "fanSpeeds": [
                    {
                        "fanName": f"Fan_{i+1}",
                        "speed": random.randint(2000, 4000)
                    }
                    for i in range(random.randint(1, 3))  # 1-3个风扇
                ],
                "taskInfos": []
            }
    
    # 从 stackinfo 中提取任务，分配到对应的板卡
    for stack in stacks:
        stack_name = stack["stackName"]
        stack_uuid = stack["stackUUID"]
        
        for service in stack["serviceInfos"]:
            service_name = service["serviceName"]
            service_uuid = service["serviceUUID"]
            
            for task in service["taskInfos"]:
                # 获取任务的板卡位置
                chassis_num = task["chassisNumber"]
                board_num = task["boardNumber"]
                key = (chassis_num, board_num)
                
                # 将任务添加到对应板卡（只包含 boardinfo 需要的字段）
                board_task = {
                    "taskID": task["taskID"],
                    "taskStatus": task["taskStatus"],
                    "serviceName": service_name,
                    "serviceUUID": service_uuid,
                    "stackName": stack_name,
                    "stackUUID": stack_uuid
                }
                
                if key in boards_dict:
                    boards_dict[key]["taskInfos"].append(board_task)
    
    # 转换为列表并排序
    boards = list(boards_dict.values())
    boards.sort(key=lambda x: (x["chassisNumber"], x["boardNumber"]))
    
    return boards


def generate_mock_board_data():
    """生成模拟板卡数据（使用共享的 stackinfo 数据）"""
    # 如果 stacks 为空，生成只有板卡信息没有任务的数据
    if not _shared_data["stacks"]:
        # 生成空的板卡数据（没有任务）
        boards_dict = {}
        for chassis_num in range(1, 10):  # 9个机箱 (1-9)
            chassis_name = f"Chassis_{chassis_num}"
            for board_num in range(1, 15):  # 每个机箱14个板卡 (1-14)
                key = (chassis_num, board_num)
                boards_dict[key] = {
                    "chassisName": chassis_name,
                    "chassisNumber": chassis_num,
                    "boardName": f"Board_{chassis_num}_{board_num}",
                    "boardNumber": board_num,
                    "boardType": random.choice([0, 1, 2, 3, 4, 5, 6]),
                    "boardAddress": generate_board_ip_address(chassis_num, board_num),
                    "boardStatus": random.choice([0, 1]),
                    "voltage": round(random.uniform(11.5, 13.5), 2),
                    "current": round(random.uniform(1.5, 3.0), 2),
                    "temperature": round(random.uniform(35.0, 55.0), 2),
                    "fanSpeeds": [
                        {
                            "fanName": f"Fan_{i+1}",
                            "speed": random.randint(2000, 4000)
                        }
                        for i in range(random.randint(1, 3))
                    ],
                    "taskInfos": []  # 没有任务
                }
        boards = list(boards_dict.values())
        boards.sort(key=lambda x: (x["chassisNumber"], x["boardNumber"]))
        _shared_data["boards"] = boards
        return boards
    
    # 如果已经有 stacks 数据，根据 stacks 生成 boardinfo
    if _shared_data["boards"] is None:
        _shared_data["boards"] = generate_mock_board_data_from_stacks(_shared_data["stacks"])
    
    return _shared_data["boards"]


class APIHandler(BaseHTTPRequestHandler):
    """API 请求处理器"""
    
    def do_GET(self):
        """处理 GET 请求"""
        parsed_path = urlparse(self.path)
        
        if parsed_path.path == '/api/v1/external/qyw/boardinfo':
            self.handle_board_info()
        elif parsed_path.path == '/health':
            self.handle_health()
        else:
            self.send_error(404, "Not Found")
    
    def do_POST(self):
        """处理 POST 请求"""
        parsed_path = urlparse(self.path)
        
        if parsed_path.path == '/api/v1/external/qyw/stackinfo':
            self.handle_stack_info()
        elif parsed_path.path == '/api/v1/stacks/labels/deploy':
            self.handle_deploy_stacks()
        elif parsed_path.path == '/api/v1/stacks/labels/undeploy':
            self.handle_undeploy_stacks()
        else:
            self.send_error(404, "Not Found")
    
    def handle_board_info(self):
        """处理板卡信息请求"""
        try:
            board_data = generate_mock_board_data()
            
            response = {
                "code": 0,
                "message": "success",
                "data": board_data
            }
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps(response, ensure_ascii=False).encode('utf-8'))
        
        except Exception as e:
            error_response = {
                "code": -1,
                "message": f"获取板卡信息失败: {str(e)}",
                "data": []
            }
            self.send_response(500)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(error_response, ensure_ascii=False).encode('utf-8'))
    
    def handle_stack_info(self):
        """处理业务链路信息请求"""
        try:
            # 返回当前的 stacks 数据（如果为空则返回空列表）
            stack_data = _shared_data["stacks"] if _shared_data["stacks"] else []
            
            response = {
                "code": 0,
                "message": "success",
                "data": stack_data
            }
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps(response, ensure_ascii=False).encode('utf-8'))
        
        except Exception as e:
            error_response = {
                "code": -1,
                "message": f"获取业务链路信息失败: {str(e)}",
                "data": []
            }
            self.send_response(500)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(error_response, ensure_ascii=False).encode('utf-8'))
    
    def handle_deploy_stacks(self):
        """处理业务链路部署请求"""
        try:
            # 读取请求体
            content_length = int(self.headers.get('Content-Length', 0))
            if content_length == 0:
                self.send_error(400, "Bad Request: Empty body")
                return
            
            request_body = self.rfile.read(content_length)
            request_data = json.loads(request_body.decode('utf-8'))
            
            # 解析请求参数
            stack_labels = request_data.get('stackLabels', [])
            stop = request_data.get('stop', 0)
            account = request_data.get('account', '')
            password = request_data.get('password', '')
            
            # 验证参数
            if not stack_labels or not isinstance(stack_labels, list):
                error_response = {
                    "code": -1,
                    "message": "stackLabels 参数无效",
                    "data": [{
                        "successStackInfos": [],
                        "failureStackInfos": []
                    }]
                }
                self.send_response(400)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(error_response, ensure_ascii=False).encode('utf-8'))
                return
            
            # 如果 stacks 为空，创建新的 stackinfo 数据
            if not _shared_data["stacks"]:
                _shared_data["stacks"] = generate_mock_stack_data()
                _shared_data["boards"] = generate_mock_board_data_from_stacks(_shared_data["stacks"])
            
            stacks = _shared_data["stacks"]
            
            # 根据标签查找业务链路
            success_stacks = []
            failure_stacks = []
            
            for label_uuid in stack_labels:
                found = False
                for stack in stacks:
                    # 检查业务链路是否包含该标签
                    for label_info in stack.get("stackLabelInfos", []):
                        if label_info.get("satckLabelUUID") == label_uuid:
                            found = True
                            # 模拟部署：80%成功率
                            if random.random() < 0.8:
                                # 部署成功：更新部署状态
                                stack["stackDelpoyStatus"] = 1
                                success_stacks.append({
                                    "stackName": stack["stackName"],
                                    "stackUUID": stack["stackUUID"],
                                    "message": f"业务链路 {stack['stackName']} 部署成功"
                                })
                            else:
                                # 部署失败
                                failure_stacks.append({
                                    "stackName": stack["stackName"],
                                    "stackUUID": stack["stackUUID"],
                                    "message": f"业务链路 {stack['stackName']} 部署失败：资源不足"
                                })
                            break
                    if found:
                        break
                
                if not found:
                    # 未找到对应的业务链路
                    failure_stacks.append({
                        "stackName": "",
                        "stackUUID": "",
                        "message": f"未找到标签UUID为 {label_uuid} 的业务链路"
                    })
            
            # 更新 boardinfo 数据（因为部署状态改变了）
            _shared_data["boards"] = generate_mock_board_data_from_stacks(stacks)
            
            # 构建响应
            response = {
                "code": 0,
                "message": "success",
                "data": [{
                    "successStackInfos": success_stacks,
                    "failureStackInfos": failure_stacks
                }]
            }
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps(response, ensure_ascii=False).encode('utf-8'))
        
        except json.JSONDecodeError:
            error_response = {
                "code": -1,
                "message": "无效的JSON格式",
                "data": [{
                    "successStackInfos": [],
                    "failureStackInfos": []
                }]
            }
            self.send_response(400)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(error_response, ensure_ascii=False).encode('utf-8'))
        
        except Exception as e:
            error_response = {
                "code": -1,
                "message": f"部署业务链路失败: {str(e)}",
                "data": [{
                    "successStackInfos": [],
                    "failureStackInfos": []
                }]
            }
            self.send_response(500)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(error_response, ensure_ascii=False).encode('utf-8'))
    
    def handle_undeploy_stacks(self):
        """处理业务链路停用请求"""
        try:
            # 读取请求体
            content_length = int(self.headers.get('Content-Length', 0))
            if content_length == 0:
                self.send_error(400, "Bad Request: Empty body")
                return
            
            request_body = self.rfile.read(content_length)
            request_data = json.loads(request_body.decode('utf-8'))
            
            # 解析请求参数
            stack_labels = request_data.get('stackLabels', [])
            
            # 验证参数
            if not stack_labels or not isinstance(stack_labels, list):
                error_response = {
                    "code": -1,
                    "message": "stackLabels 参数无效",
                    "data": [{
                        "successStackInfos": [],
                        "failureStackInfos": []
                    }]
                }
                self.send_response(400)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(error_response, ensure_ascii=False).encode('utf-8'))
                return
            
            # 获取当前的 stacks 数据
            stacks = _shared_data["stacks"] if _shared_data["stacks"] else []
            
            # 根据标签查找并移除业务链路
            success_stacks = []
            failure_stacks = []
            stacks_to_remove = []  # 记录要移除的 stack UUID
            
            for label_uuid in stack_labels:
                found = False
                for stack in stacks:
                    # 检查业务链路是否包含该标签
                    for label_info in stack.get("stackLabelInfos", []):
                        if label_info.get("satckLabelUUID") == label_uuid:
                            found = True
                            # 模拟停用：80%成功率
                            if random.random() < 0.8:
                                # 停用成功：标记为移除
                                if stack["stackUUID"] not in stacks_to_remove:
                                    stacks_to_remove.append(stack["stackUUID"])
                                    success_stacks.append({
                                        "stackName": stack["stackName"],
                                        "stackUUID": stack["stackUUID"],
                                        "message": f"业务链路 {stack['stackName']} 停用成功"
                                    })
                            else:
                                # 停用失败
                                failure_stacks.append({
                                    "stackName": stack["stackName"],
                                    "stackUUID": stack["stackUUID"],
                                    "message": f"业务链路 {stack['stackName']} 停用失败：资源占用"
                                })
                            break
                    if found:
                        break
                
                if not found:
                    # 未找到对应的业务链路
                    failure_stacks.append({
                        "stackName": "",
                        "stackUUID": "",
                        "message": f"未找到标签UUID为 {label_uuid} 的业务链路"
                    })
            
            # 移除停用成功的业务链路
            if stacks_to_remove:
                _shared_data["stacks"] = [
                    stack for stack in stacks 
                    if stack["stackUUID"] not in stacks_to_remove
                ]
            
            # 如果所有 stacks 都被移除，清空列表
            if not _shared_data["stacks"]:
                _shared_data["stacks"] = []
            
            # 更新 boardinfo 数据（因为 stacks 改变了）
            _shared_data["boards"] = None  # 重置 boards，下次调用时会重新生成
            generate_mock_board_data()  # 重新生成 boardinfo
            
            # 构建响应
            response = {
                "code": 0,
                "message": "success",
                "data": [{
                    "successStackInfos": success_stacks,
                    "failureStackInfos": failure_stacks
                }]
            }
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps(response, ensure_ascii=False).encode('utf-8'))
        
        except json.JSONDecodeError:
            error_response = {
                "code": -1,
                "message": "无效的JSON格式",
                "data": [{
                    "successStackInfos": [],
                    "failureStackInfos": []
                }]
            }
            self.send_response(400)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(error_response, ensure_ascii=False).encode('utf-8'))
        
        except Exception as e:
            error_response = {
                "code": -1,
                "message": f"停用业务链路失败: {str(e)}",
                "data": [{
                    "successStackInfos": [],
                    "failureStackInfos": []
                }]
            }
            self.send_response(500)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(error_response, ensure_ascii=False).encode('utf-8'))
    
    def handle_health(self):
        """处理健康检查请求"""
        response = {"status": "ok"}
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(response).encode('utf-8'))
    
    def log_message(self, format, *args):
        """自定义日志输出"""
        print(f"[{self.address_string()}] {format % args}")


def run_server(port=8080):
    """启动服务器"""
    server_address = ('0.0.0.0', port)
    httpd = HTTPServer(server_address, APIHandler)
    
    print("=" * 60)
    print("Mock API Server 启动中...")
    print(f"GET  接口: http://localhost:{port}/api/v1/external/qyw/boardinfo")
    print(f"POST 接口: http://localhost:{port}/api/v1/external/qyw/stackinfo")
    print(f"POST 接口: http://localhost:{port}/api/v1/stacks/labels/deploy")
    print(f"POST 接口: http://localhost:{port}/api/v1/stacks/labels/undeploy")
    print(f"健康检查: http://localhost:{port}/health")
    print("按 Ctrl+C 停止服务器")
    print("=" * 60)
    
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n服务器已停止")
        httpd.server_close()


if __name__ == '__main__':
    run_server(port=8080)

