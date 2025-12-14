# HTTP响应文本示例（用于Socket工具模拟）

## 1. 获取板卡信息接口 `/api/v1/external/qyw/boardinfo`

### 完整HTTP响应文本：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 1234

{
  "code": 0,
  "message": "success",
  "data": [
    {
      "chassisName": "机箱1",
      "chassisNumber": 1,
      "boardName": "板卡1",
      "boardNumber": 1,
      "boardType": 1,
      "boardAddress": "192.168.1.100",
      "boardStatus": 0,
      "voltage": 12.5,
      "current": 2.3,
      "temperature": 45.6,
      "fanSpeeds": [
        {
          "fanName": "风扇1",
          "speed": 3000
        },
        {
          "fanName": "风扇2",
          "speed": 2800
        }
      ],
      "taskInfos": [
        {
          "taskID": "task-001",
          "taskStatus": 1,
          "serviceName": "算法组件1",
          "serviceUUID": "service-uuid-001",
          "stackName": "业务链路1",
          "stackUUID": "stack-uuid-001"
        },
        {
          "taskID": "task-002",
          "taskStatus": 1,
          "serviceName": "算法组件2",
          "serviceUUID": "service-uuid-002",
          "stackName": "业务链路1",
          "stackUUID": "stack-uuid-001"
        }
      ]
    },
    {
      "chassisName": "机箱1",
      "chassisNumber": 1,
      "boardName": "板卡2",
      "boardNumber": 2,
      "boardType": 1,
      "boardAddress": "192.168.1.101",
      "boardStatus": 0,
      "voltage": 12.3,
      "current": 2.1,
      "temperature": 42.3,
      "fanSpeeds": [
        {
          "fanName": "风扇1",
          "speed": 2900
        }
      ],
      "taskInfos": []
    },
    {
      "chassisName": "机箱2",
      "chassisNumber": 2,
      "boardName": "板卡1",
      "boardNumber": 1,
      "boardType": 2,
      "boardAddress": "192.168.1.200",
      "boardStatus": 1,
      "voltage": 11.8,
      "current": 1.9,
      "temperature": 48.5,
      "fanSpeeds": [],
      "taskInfos": [
        {
          "taskID": "task-003",
          "taskStatus": 3,
          "serviceName": "算法组件3",
          "serviceUUID": "service-uuid-003",
          "stackName": "业务链路2",
          "stackUUID": "stack-uuid-002"
        }
      ]
    }
  ]
}
```

### 字段说明：
- `code`: 0表示成功
- `message`: 响应消息
- `data`: 板卡信息数组
  - `chassisName`: 机箱名称
  - `chassisNumber`: 机箱号（1-9）
  - `boardName`: 板卡名称
  - `boardNumber`: 板卡槽位号（1-12）
  - `boardType`: 板卡类型
  - `boardAddress`: 板卡IP地址
  - `boardStatus`: 板卡状态（0-正常，1-异常，2-不在位）
  - `voltage`: 电压（浮点数）
  - `current`: 电流（浮点数）
  - `temperature`: 温度（浮点数）
  - `fanSpeeds`: 风扇信息数组
    - `fanName`: 风扇名称
    - `speed`: 风扇转速（整数）
  - `taskInfos`: 任务信息数组
    - `taskID`: 任务ID
    - `taskStatus`: 任务状态（1-运行中，2-已完成，3-异常，0-其他）
    - `serviceName`: 算法组件名称
    - `serviceUUID`: 算法组件唯一标识
    - `stackName`: 业务链路名称
    - `stackUUID`: 业务链路唯一标识

---

## 2. 获取业务链路信息接口 `/api/v1/external/qyw/stackinfo`

### 完整HTTP响应文本：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 2345

{
  "code": 0,
  "message": "success",
  "data": [
    {
      "stackName": "业务链路1",
      "stackUUID": "stack-uuid-001",
      "stackLabelInfos": [
        {
          "stackLabelName": "模式1",
          "stackLabelUUID": "label-uuid-001"
        }
      ],
      "stackDeployStatus": 1,
      "stackRunningStatus": 1,
      "serviceInfos": [
        {
          "serviceName": "算法组件1",
          "serviceUUID": "service-uuid-001",
          "serviceStatus": 2,
          "serviceType": 0,
          "taskInfos": [
            {
              "taskID": "task-001",
              "taskStatus": 1,
              "cpuCores": 4.0,
              "cpuUsed": 1.5,
              "cpuUsage": 37.5,
              "memorySize": 8192.0,
              "memoryUsed": 2048.0,
              "memoryUsage": 25.0,
              "netReceive": 1024.5,
              "netSent": 512.3,
              "gpuMemUsed": 0.0,
              "chassisName": "机箱1",
              "chassisNumber": 1,
              "boardName": "板卡1",
              "boardNumber": 1,
              "boardAddress": "192.168.1.100"
            }
          ]
        }
      ]
    }
  ]
}
```

---

## 3. 心跳检测接口 `/api/v1/sys-config/client/up?clientIp=192.168.6.222`

### 完整HTTP响应文本：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 45

{
  "code": 0,
  "message": "success",
  "data": "success"
}
```

---

## 4. 部署业务链路接口 `/api/v1/stacks/labels/deploy`

### 完整HTTP响应文本：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 456

{
  "code": 0,
  "message": "success",
  "data": [
    {
      "successStackInfos": [
        {
          "stackName": "业务链路1",
          "stackUUID": "stack-uuid-001",
          "message": "部署成功"
        }
      ],
      "failureStackInfos": [
        {
          "stackName": "业务链路2",
          "stackUUID": "stack-uuid-002",
          "message": "部署失败：资源不足"
        }
      ]
    }
  ]
}
```

---

## 5. 停用业务链路接口 `/api/v1/stacks/labels/undeploy`

### 完整HTTP响应文本：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 456

{
  "code": 0,
  "message": "success",
  "data": [
    {
      "successStackInfos": [
        {
          "stackName": "业务链路1",
          "stackUUID": "stack-uuid-001",
          "message": "停用成功"
        }
      ],
      "failureStackInfos": []
    }
  ]
}
```

---

## 6. 业务链路复位接口 `/api/v1/stacks/reset`

### 完整HTTP响应文本：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 45

{
  "code": 0,
  "message": "success",
  "data": "success"
}
```

---

## 7. 板卡异常上报接口 `/api/v1/alert/board`（本地接口）

### HTTP请求示例：

```
POST /api/v1/alert/board HTTP/1.1
Host: localhost:8888
Content-Type: application/json
Content-Length: 234

{
  "chassisName": "机箱1",
  "chassisNumber": 1,
  "boardName": "板卡1",
  "boardNumber": 1,
  "boardType": 1,
  "boardAddress": "192.168.1.100",
  "boardStatus": 1,
  "alertMessages": [
    "板卡温度过高：65°C",
    "板卡电压异常：11.2V"
  ]
}
```

### HTTP响应示例：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 45

{
  "code": 0,
  "message": "success",
  "data": "success"
}
```

### 字段说明：
- `chassisName`: 机箱名称（字符串）
- `chassisNumber`: 机箱号（整数，1-9）
- `boardName`: 板卡名称（字符串）
- `boardNumber`: 板卡槽位号（整数，1-12）
- `boardType`: 板卡类型（整数）
- `boardAddress`: 板卡IP地址（字符串）
- `boardStatus`: 板卡状态（整数，0-正常，1-异常）
- `alertMessages`: 告警信息列表（字符串数组）

### 简化请求示例（最小化）：

```
POST /api/v1/alert/board HTTP/1.1
Host: localhost:8888
Content-Type: application/json

{"chassisNumber":1,"boardNumber":1,"boardAddress":"192.168.1.100","boardStatus":1,"alertMessages":["板卡异常"]}
```

---

## 8. 组件异常上报接口 `/api/v1/alert/service`（本地接口）

### HTTP请求示例：

```
POST /api/v1/alert/service HTTP/1.1
Host: localhost:8888
Content-Type: application/json
Content-Length: 567

{
  "stackName": "业务链路1",
  "stackUUID": "stack-uuid-001",
  "serviceName": "算法组件1",
  "serviceUUID": "service-uuid-001",
  "taskAlertInfos": [
    {
      "taskID": "task-001",
      "taskStatus": 3,
      "chassisName": "机箱1",
      "chassisNumber": 1,
      "boardName": "板卡1",
      "boardNumber": 1,
      "boardType": 1,
      "boardAddress": "192.168.1.100",
      "boardStatus": 1,
      "alertMessages": [
        "任务CPU使用率过高：95%",
        "任务内存溢出"
      ]
    },
    {
      "taskID": "task-002",
      "taskStatus": 3,
      "chassisName": "机箱1",
      "chassisNumber": 1,
      "boardName": "板卡2",
      "boardNumber": 2,
      "boardType": 1,
      "boardAddress": "192.168.1.101",
      "boardStatus": 0,
      "alertMessages": [
        "任务响应超时"
      ]
    }
  ]
}
```

### HTTP响应示例：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 45

{
  "code": 0,
  "message": "success",
  "data": "success"
}
```

### 字段说明：
- `stackName`: 业务链路名称（字符串）
- `stackUUID`: 业务链路唯一标识（字符串）
- `serviceName`: 算法组件名称（字符串）
- `serviceUUID`: 算法组件唯一标识（字符串）
- `taskAlertInfos`: 任务告警信息列表（数组）
  - `taskID`: 任务ID（字符串）
  - `taskStatus`: 任务状态（整数，1-运行中，2-已完成，3-异常，0-其他）
  - `chassisName`: 机箱名称（字符串）
  - `chassisNumber`: 机箱号（整数，1-9）
  - `boardName`: 板卡名称（字符串）
  - `boardNumber`: 板卡槽位号（整数，1-12）
  - `boardType`: 板卡类型（整数）
  - `boardAddress`: 板卡IP地址（字符串）
  - `boardStatus`: 板卡状态（整数，0-正常，1-异常）
  - `alertMessages`: 告警信息列表（字符串数组）

### 简化请求示例（最小化）：

```
POST /api/v1/alert/service HTTP/1.1
Host: localhost:8888
Content-Type: application/json

{"stackName":"业务链路1","stackUUID":"stack-uuid-001","serviceName":"算法组件1","serviceUUID":"service-uuid-001","taskAlertInfos":[{"taskID":"task-001","taskStatus":3,"chassisNumber":1,"boardNumber":1,"boardAddress":"192.168.1.100","alertMessages":["任务异常"]}]}
```

---

## 注意事项：

1. **HTTP头格式**：
   - 第一行：`HTTP/1.1 200 OK`
   - 必须包含 `Content-Type: application/json`
   - `Content-Length` 应该是响应体的实际字节数（不包括HTTP头）

2. **响应体格式**：
   - 必须是有效的JSON格式
   - 所有字符串字段都可以用实际值替换
   - 数组可以为空 `[]`

3. **在Socket工具中使用**：
   - 将完整的HTTP响应文本（包括HTTP头和响应体）作为响应发送
   - 确保每行以 `\r\n` 结尾（Windows）或 `\n` 结尾（Unix/Linux）
   - HTTP头和响应体之间必须有一个空行

4. **简化版本**（最小化响应）：
   如果只需要测试基本功能，可以使用最小化的响应：
   ```
   HTTP/1.1 200 OK
   Content-Type: application/json
   
   {"code":0,"message":"success","data":[]}
   ```

---

## Content-Length 计算方法

### 1. 基本概念

`Content-Length` 是HTTP响应头中的一个字段，表示**响应体的字节长度**（不包括HTTP头部分）。

**重要说明：**
- `Content-Length` 只计算响应体的字节数，不包括HTTP头
- HTTP头和响应体之间必须有一个空行（`\r\n\r\n` 或 `\n\n`）
- 计算时需要考虑字符编码（UTF-8、ASCII等）

### 2. 计算方法

#### 方法一：手动计算（适用于简单响应）

对于纯ASCII字符的JSON响应，可以直接数字符数：

**示例1：简单响应**
```json
{"code":0,"message":"success","data":"success"}
```
- 字符数：45个字符
- Content-Length: `45`

**示例2：格式化JSON（注意空格和换行）**
```json
{
  "code": 0,
  "message": "success",
  "data": "success"
}
```
- 如果使用格式化JSON，需要计算所有字符（包括空格、换行符）
- 换行符在不同系统上可能不同：
  - Windows: `\r\n` (2字节)
  - Unix/Linux: `\n` (1字节)
  - Mac (旧版): `\r` (1字节)

#### 方法二：使用工具计算（推荐）

**在Linux/macOS上：**
```bash
# 计算JSON字符串的字节数
echo -n '{"code":0,"message":"success","data":"success"}' | wc -c
# 输出: 45

# 或者使用printf
printf '{"code":0,"message":"success","data":"success"}' | wc -c
```

**在Python中：**
```python
import json

# 方法1：计算JSON字符串的字节数
response_body = json.dumps({"code": 0, "message": "success", "data": "success"})
content_length = len(response_body.encode('utf-8'))
print(f"Content-Length: {content_length}")

# 方法2：使用紧凑格式（无空格）
response_body = json.dumps({"code": 0, "message": "success", "data": "success"}, separators=(',', ':'))
content_length = len(response_body.encode('utf-8'))
print(f"Content-Length: {content_length}")
```

**在C++中：**
```cpp
#include <string>
#include <iostream>

std::string response_body = R"({"code":0,"message":"success","data":"success"})";
size_t content_length = response_body.length();  // 对于ASCII字符，length()等于字节数
// 对于UTF-8，需要使用UTF-8编码后的字节数
```

#### 方法三：使用在线工具

可以使用在线JSON格式化工具，然后查看字节数。

### 3. 实际示例

#### 示例1：心跳检测响应

**响应体：**
```json
{
  "code": 0,
  "message": "success",
  "data": "success"
}
```

**计算过程：**
- 格式化版本（包含空格和换行）：
  - 如果使用 `\n` 换行：约 60-65 字节
  - 如果使用 `\r\n` 换行：约 65-70 字节
- 紧凑版本（无空格）：
  ```json
  {"code":0,"message":"success","data":"success"}
  ```
  - 字节数：45 字节

**推荐使用紧凑版本：**
```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 45

{"code":0,"message":"success","data":"success"}
```

#### 示例2：板卡信息响应

**响应体（紧凑格式）：**
```json
{"code":0,"message":"success","data":[{"chassisName":"机箱1","chassisNumber":1,"boardName":"板卡1","boardNumber":1,"boardType":1,"boardAddress":"192.168.1.100","boardStatus":0,"voltage":12.5,"current":2.3,"temperature":45.6,"fanSpeeds":[],"taskInfos":[]}]}
```

**计算：**
- 中文字符在UTF-8编码中通常占3字节
- "机箱1" = 3 + 3 + 1 = 7字节
- "板卡1" = 3 + 3 + 1 = 7字节
- 总字节数需要实际计算

**使用Python计算：**
```python
import json

data = {
    "code": 0,
    "message": "success",
    "data": [{
        "chassisName": "机箱1",
        "chassisNumber": 1,
        "boardName": "板卡1",
        "boardNumber": 1,
        "boardType": 1,
        "boardAddress": "192.168.1.100",
        "boardStatus": 0,
        "voltage": 12.5,
        "current": 2.3,
        "temperature": 45.6,
        "fanSpeeds": [],
        "taskInfos": []
    }]
}

response_body = json.dumps(data, ensure_ascii=False, separators=(',', ':'))
content_length = len(response_body.encode('utf-8'))
print(f"Content-Length: {content_length}")
# 输出: Content-Length: 约 200-250 字节（取决于实际数据）
```

### 4. 注意事项

1. **字符编码**：
   - ASCII字符（0-127）：1字节/字符
   - UTF-8中文字符：通常3字节/字符
   - UTF-8其他字符：1-4字节不等

2. **JSON格式**：
   - 紧凑格式（无空格）：字节数最小
   - 格式化（有空格和换行）：字节数较大，但可读性好
   - 在Socket工具中，建议使用紧凑格式以减少计算错误

3. **常见错误**：
   - ❌ 错误：计算时包含了HTTP头
   - ✅ 正确：只计算响应体的字节数
   - ❌ 错误：使用字符数而不是字节数（对于非ASCII字符）
   - ✅ 正确：使用UTF-8编码后的字节数

4. **动态计算**：
   - 在实际应用中，应该动态计算Content-Length
   - 不要硬编码Content-Length值
   - 如果Content-Length不准确，可能导致客户端无法正确解析响应

### 5. 快速计算技巧

**对于简单响应，可以使用以下公式估算：**
- 纯ASCII JSON：字符数 ≈ 字节数
- 包含中文的JSON：字符数 × 1.5-2（粗略估算）
- 最准确的方法：使用编程语言计算UTF-8编码后的字节数

**推荐做法：**
在Socket工具中测试时，可以：
1. 先不设置Content-Length（某些工具会自动计算）
2. 或者使用略大的值（但不要太大，否则客户端可能一直等待）
3. 或者使用工具自动计算（如Postman、curl等会自动计算）

### 6. 验证方法

发送响应后，可以通过以下方式验证Content-Length是否正确：

```bash
# 使用curl验证
curl -v http://localhost:8888/api/v1/alert/board

# 查看响应头中的Content-Length
# 对比实际响应体的字节数
```

**如果Content-Length不正确：**
- 值太小：客户端可能截断响应
- 值太大：客户端可能一直等待更多数据
- 最佳实践：确保Content-Length与实际响应体字节数完全一致

