# Mock API Server 使用说明

## 概述

`mock_api_server_simple.py` 是一个使用 Python 标准库实现的模拟 API 服务器，用于测试和开发。它提供了以下 API 接口：

- **GET** `/api/v1/external/qyw/boardinfo` - 获取所有板卡信息和状态
- **POST** `/api/v1/external/qyw/stackinfo` - 获取所有业务链路信息和状态
- **POST** `/api/v1/stacks/labels/deploy` - 根据业务链路标签批量启用业务链路
- **POST** `/api/v1/stacks/labels/undeploy` - 根据业务链路标签批量停用业务链路
- **GET** `/health` - 健康检查

## 启动服务器

### 方法 1: 直接运行

```bash
cd /root/zygl3
python3 tests/mock_api_server_simple.py
```

### 方法 2: 指定端口

修改文件末尾的端口号，或使用环境变量：

```bash
PORT=8080 python3 tests/mock_api_server_simple.py
```

默认端口：**8080**

服务器启动后会显示：

```
============================================================
Mock API Server 启动中...
GET  接口: http://localhost:8080/api/v1/external/qyw/boardinfo
POST 接口: http://localhost:8080/api/v1/external/qyw/stackinfo
POST 接口: http://localhost:8080/api/v1/stacks/labels/deploy
POST 接口: http://localhost:8080/api/v1/stacks/labels/undeploy
健康检查: http://localhost:8080/health
按 Ctrl+C 停止服务器
============================================================
```

## API 使用示例

### 1. 获取板卡信息 (GET)

**请求：**
```bash
curl http://localhost:8080/api/v1/external/qyw/boardinfo
```

**响应示例：**
```json
{
  "code": 0,
  "message": "success",
  "data": [
    {
      "chassisName": "Chassis_1",
      "chassisNumber": 1,
      "boardName": "Board_1_1",
      "boardNumber": 1,
      "boardType": 2,
      "boardAddress": "192.168.2.5",
      "boardStatus": 0,
      "voltage": 12.5,
      "current": 2.1,
      "temperature": 45.3,
      "fanSpeeds": [
        {"fanName": "Fan_1", "speed": 3000}
      ],
      "taskInfos": []
    }
  ]
}
```

**说明：**
- 初始状态下，`taskInfos` 为空数组
- 调用 deploy API 后，`taskInfos` 会包含任务信息

---

### 2. 获取业务链路信息 (POST)

**请求：**
```bash
curl -X POST http://localhost:8080/api/v1/external/qyw/stackinfo \
  -H "Content-Type: application/json"
```

**响应示例：**
```json
{
  "code": 0,
  "message": "success",
  "data": [
    {
      "stackName": "Stack_1",
      "stackUUID": "stack-uuid-1",
      "stackLabelInfos": [
        {
          "stackLabelName": "Label_1_1",
          "stackLabelUUID": "label-uuid-1-1"
        }
      ],
      "stackDeployStatus": 1,
      "stackRunningStatus": 1,
      "serviceInfos": [
        {
          "serviceName": "Service_1_1",
          "serviceUUID": "service-uuid-1-1",
          "serviceStatus": 2,
          "serviceType": 0,
          "taskInfos": [
            {
              "taskID": "task-1-1-1",
              "taskStatus": 1,
              "cpuCores": 8.0,
              "cpuUsed": 4.5,
              "cpuUsage": 56.25,
              "memorySize": 16.0,
              "memoryUsed": 8.2,
              "memoryUsage": 51.25,
              "netReceive": 123.45,
              "netSent": 67.89,
              "gpuMemUsed": 4.0,
              "chassisName": "Chassis_1",
              "chassisNumber": 1,
              "boardName": "Board_1_5",
              "boardNumber": 5,
              "boardAddress": "192.168.2.133"
            }
          ]
        }
      ]
    }
  ]
}
```

**说明：**
- 初始状态下，返回空数组 `[]`
- 调用 deploy API 后，才会返回业务链路数据

---

### 3. 部署业务链路 (POST)

**请求：**
```bash
curl -X POST http://localhost:8080/api/v1/stacks/labels/deploy \
  -H "Content-Type: application/json" \
  -d '{
    "stackLabels": ["label-uuid-1-1", "label-uuid-2-1"],
    "stop": 0,
    "account": "admin",
    "password": "12q12w12ee"
  }'
```

**请求参数：**
- `stackLabels` (必填): 标签 UUID 列表，例如 `["label-uuid-1-1"]`
- `stop` (可选): `0` - 启动服务，不排他；`1` - 先停止其他业务再启动
- `account` (必填): 用户账号
- `password` (必填): 密码

**响应示例：**
```json
{
  "code": 0,
  "message": "success",
  "data": [
    {
      "successStackInfos": [
        {
          "stackName": "Stack_1",
          "stackUUID": "stack-uuid-1",
          "message": "业务链路 Stack_1 部署成功"
        }
      ],
      "failureStackInfos": []
    }
  ]
}
```

**说明：**
- 首次调用 deploy API 时，会创建业务链路数据
- 部署成功后，`stackDeployStatus` 会被设置为 `1`
- 部署后，`/api/v1/external/qyw/stackinfo` 和 `/api/v1/external/qyw/boardinfo` 会返回相应的数据

---

### 4. 停用业务链路 (POST)

**请求：**
```bash
curl -X POST http://localhost:8080/api/v1/stacks/labels/undeploy \
  -H "Content-Type: application/json" \
  -d '{
    "stackLabels": ["label-uuid-1-1"]
  }'
```

**请求参数：**
- `stackLabels` (必填): 标签 UUID 列表

**响应示例：**
```json
{
  "code": 0,
  "message": "success",
  "data": [
    {
      "successStackInfos": [
        {
          "stackName": "Stack_1",
          "stackUUID": "stack-uuid-1",
          "message": "业务链路 Stack_1 停用成功"
        }
      ],
      "failureStackInfos": []
    }
  ]
}
```

**说明：**
- 停用成功后，对应的业务链路会被移除
- 如果所有业务链路都被停用，`/api/v1/external/qyw/stackinfo` 会返回空数组 `[]`
- 停用后，`/api/v1/external/qyw/boardinfo` 中的 `taskInfos` 会相应更新

---

### 5. 健康检查 (GET)

**请求：**
```bash
curl http://localhost:8080/health
```

**响应示例：**
```json
{
  "status": "ok"
}
```

---

## 完整工作流程示例

### 场景：部署业务链路并查看信息

```bash
# 1. 启动服务器
python3 tests/mock_api_server_simple.py

# 2. 查看初始状态（另一个终端）
curl http://localhost:8080/api/v1/external/qyw/stackinfo
# 返回: {"code":0,"message":"success","data":[]}

curl http://localhost:8080/api/v1/external/qyw/boardinfo | jq '.data[0].taskInfos'
# 返回: []

# 3. 部署业务链路
curl -X POST http://localhost:8080/api/v1/stacks/labels/deploy \
  -H "Content-Type: application/json" \
  -d '{
    "stackLabels": ["label-uuid-1-1"],
    "stop": 0,
    "account": "admin",
    "password": "12q12w12ee"
  }'

# 4. 查看部署后的数据
curl http://localhost:8080/api/v1/external/qyw/stackinfo | jq '.data | length'
# 返回: 5 (业务链路数量)

curl http://localhost:8080/api/v1/external/qyw/boardinfo | jq '[.data[].taskInfos] | flatten | length'
# 返回: 任务数量

# 5. 停用业务链路
curl -X POST http://localhost:8080/api/v1/stacks/labels/undeploy \
  -H "Content-Type: application/json" \
  -d '{
    "stackLabels": ["label-uuid-1-1"]
  }'

# 6. 查看停用后的状态
curl http://localhost:8080/api/v1/external/qyw/stackinfo | jq '.data | length'
# 返回: 4 (剩余业务链路数量，或 0 如果全部停用)
```

---

## Python 使用示例

```python
import requests
import json

BASE_URL = "http://localhost:8080"

# 1. 获取板卡信息
response = requests.get(f"{BASE_URL}/api/v1/external/qyw/boardinfo")
print("板卡信息:", response.json())

# 2. 获取业务链路信息
response = requests.post(f"{BASE_URL}/api/v1/external/qyw/stackinfo")
print("业务链路信息:", response.json())

# 3. 部署业务链路
deploy_data = {
    "stackLabels": ["label-uuid-1-1"],
    "stop": 0,
    "account": "admin",
    "password": "12q12w12ee"
}
response = requests.post(
    f"{BASE_URL}/api/v1/stacks/labels/deploy",
    json=deploy_data
)
print("部署结果:", response.json())

# 4. 停用业务链路
undeploy_data = {
    "stackLabels": ["label-uuid-1-1"]
}
response = requests.post(
    f"{BASE_URL}/api/v1/stacks/labels/undeploy",
    json=undeploy_data
)
print("停用结果:", response.json())
```

---

## 注意事项

1. **数据状态管理**：
   - 初始状态下，`stacks` 为空数组
   - 调用 `deploy` API 后，才会创建业务链路数据
   - 调用 `undeploy` API 后，会移除对应的业务链路
   - 如果所有业务链路都被停用，`stacks` 会变为空数组

2. **数据一致性**：
   - `boardinfo` 和 `stackinfo` 中的任务信息保持一致
   - 部署/停用操作会同步更新两个接口的数据

3. **模拟行为**：
   - 部署和停用操作有 80% 的成功率（随机模拟）
   - 数据是随机生成的，每次启动服务器数据可能不同

4. **IP 地址生成**：
   - 板卡 IP 地址根据机箱号和槽位号生成，符合 `chassis_config.json` 中的模式

---

## 故障排查

### 问题：服务器无法启动

**解决方案：**
- 检查端口是否被占用：`lsof -i :8080`
- 尝试使用其他端口
- 检查 Python 版本：`python3 --version`（需要 Python 3.6+）

### 问题：API 返回 404

**解决方案：**
- 检查请求路径是否正确
- 确保服务器正在运行
- 检查请求方法（GET/POST）是否正确

### 问题：API 返回 400 Bad Request

**解决方案：**
- 检查请求体格式是否为有效的 JSON
- 检查必填参数是否提供
- 检查 `Content-Type` 头是否为 `application/json`

---

## 相关文件

- `tests/mock_api_server_simple.py` - Mock API 服务器实现
- `docs/API.md` - API 接口文档
- `chassis_config.json` - 机箱配置（用于 IP 地址生成）


