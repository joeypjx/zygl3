# HTTP响应文本示例（用于Socket工具模拟）

## 1. 获取板卡信息接口 `/api/v1/external/qyw/boardinfo`

### 完整HTTP响应文本：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 1283

{"code":0,"message":"success","data":[{"chassisName":"Chassis1","chassisNumber":1,"boardName":"Board1","boardNumber":1,"boardType":1,"boardAddress":"192.168.1.100","boardStatus":0,"voltage":12.5,"current":2.3,"temperature":45.6,"fanSpeeds":[{"fanName":"Fan1","speed":3000},{"fanName":"Fan2","speed":2800}],"taskInfos":[{"taskID":"task-001","taskStatus":1,"serviceName":"AlgorithmComponent1","serviceUUID":"service-uuid-001","stackName":"BusinessLink1","stackUUID":"stack-uuid-001"},{"taskID":"task-002","taskStatus":1,"serviceName":"AlgorithmComponent2","serviceUUID":"service-uuid-002","stackName":"BusinessLink1","stackUUID":"stack-uuid-001"}]},{"chassisName":"Chassis1","chassisNumber":1,"boardName":"Board2","boardNumber":2,"boardType":1,"boardAddress":"192.168.1.101","boardStatus":0,"voltage":12.3,"current":2.1,"temperature":42.3,"fanSpeeds":[{"fanName":"Fan1","speed":2900}],"taskInfos":[]},{"chassisName":"Chassis2","chassisNumber":2,"boardName":"Board1","boardNumber":1,"boardType":2,"boardAddress":"192.168.1.200","boardStatus":1,"voltage":11.8,"current":1.9,"temperature":48.5,"fanSpeeds":[],"taskInfos":[{"taskID":"task-003","taskStatus":3,"serviceName":"AlgorithmComponent3","serviceUUID":"service-uuid-003","stackName":"BusinessLink2","stackUUID":"stack-uuid-002"}]}]}
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
Content-Length: 665

{"code":0,"message":"success","data":[{"stackName":"BusinessLink1","stackUUID":"stack-uuid-001","stackLabelInfos":[{"stackLabelName":"Mode1","stackLabelUUID":"label-uuid-001"}],"stackDeployStatus":1,"stackRunningStatus":1,"serviceInfos":[{"serviceName":"AlgorithmComponent1","serviceUUID":"service-uuid-001","serviceStatus":2,"serviceType":0,"taskInfos":[{"taskID":"task-001","taskStatus":1,"cpuCores":4.0,"cpuUsed":1.5,"cpuUsage":37.5,"memorySize":8192.0,"memoryUsed":2048.0,"memoryUsage":25.0,"netReceive":1024.5,"netSent":512.3,"gpuMemUsed":0.0,"chassisName":"Chassis1","chassisNumber":1,"boardName":"Board1","boardNumber":1,"boardAddress":"192.168.1.100"}]}]}]}
```

---

## 3. 心跳检测接口 `/api/v1/sys-config/client/up?clientIp=192.168.6.222`

### 完整HTTP响应文本：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 47

{"code":0,"message":"success","data":"success"}
```

---

## 4. 部署业务链路接口 `/api/v1/stacks/labels/deploy`

### 完整HTTP响应文本：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 280

{"code":0,"message":"success","data":[{"successStackInfos":[{"stackName":"BusinessLink1","stackUUID":"stack-uuid-001","message":"Deploy success"}],"failureStackInfos":[{"stackName":"BusinessLink2","stackUUID":"stack-uuid-002","message":"Deploy failed: Insufficient resources"}]}]}
```

---

## 5. 停用业务链路接口 `/api/v1/stacks/labels/undeploy`

### 完整HTTP响应文本：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 174

{"code":0,"message":"success","data":[{"successStackInfos":[{"stackName":"BusinessLink1","stackUUID":"stack-uuid-001","message":"Undeploy success"}],"failureStackInfos":[]}]}
```

---

## 6. 业务链路复位接口 `/api/v1/stacks/reset`

### 完整HTTP响应文本：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 47

{"code":0,"message":"success","data":"success"}
```

---

## 7. 板卡异常上报接口 `/api/v1/alert/board`（本地接口）

### HTTP请求示例：

```
POST /api/v1/alert/board HTTP/1.1
Host: localhost:8888
Content-Type: application/json
Content-Length: 228

{"chassisName":"Chassis1","chassisNumber":1,"boardName":"Board1","boardNumber":1,"boardType":1,"boardAddress":"192.168.1.100","boardStatus":1,"alertMessages":["Board temperature too high: 65°C","Board voltage abnormal: 11.2V"]}
```

### HTTP响应示例：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 47

{"code":0,"message":"success","data":"success"}
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

{"chassisNumber":1,"boardNumber":1,"boardAddress":"192.168.1.100","boardStatus":1,"alertMessages":["Board abnormal"]}
```

---

## 8. 组件异常上报接口 `/api/v1/alert/service`（本地接口）

### HTTP请求示例：

```
POST /api/v1/alert/service HTTP/1.1
Host: localhost:8888
Content-Type: application/json
Content-Length: 616

{"stackName":"BusinessLink1","stackUUID":"stack-uuid-001","serviceName":"AlgorithmComponent1","serviceUUID":"service-uuid-001","taskAlertInfos":[{"taskID":"task-001","taskStatus":3,"chassisName":"Chassis1","chassisNumber":1,"boardName":"Board1","boardNumber":1,"boardType":1,"boardAddress":"192.168.1.100","boardStatus":1,"alertMessages":["Task CPU usage too high: 95%","Task memory overflow"]},{"taskID":"task-002","taskStatus":3,"chassisName":"Chassis1","chassisNumber":1,"boardName":"Board2","boardNumber":2,"boardType":1,"boardAddress":"192.168.1.101","boardStatus":0,"alertMessages":["Task response timeout"]}]}
```

### HTTP响应示例：

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 47

{"code":0,"message":"success","data":"success"}
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

{"stackName":"BusinessLink1","stackUUID":"stack-uuid-001","serviceName":"AlgorithmComponent1","serviceUUID":"service-uuid-001","taskAlertInfos":[{"taskID":"task-001","taskStatus":3,"chassisNumber":1,"boardNumber":1,"boardAddress":"192.168.1.100","alertMessages":["Task abnormal"]}]}
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
- 字符数：47个字符
- Content-Length: `47`

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
  - 字节数：47 字节

**推荐使用紧凑版本：**
```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 47

{"code":0,"message":"success","data":"success"}
```

#### 示例2：板卡信息响应

**响应体（紧凑格式）：**
```json
{"code":0,"message":"success","data":[{"chassisName":"Chassis1","chassisNumber":1,"boardName":"Board1","boardNumber":1,"boardType":1,"boardAddress":"192.168.1.100","boardStatus":0,"voltage":12.5,"current":2.3,"temperature":45.6,"fanSpeeds":[],"taskInfos":[]}]}
```

**计算：**
- 中文字符在UTF-8编码中通常占3字节
- "Chassis1" = 8字节（纯ASCII）
- "Board1" = 6字节（纯ASCII）
- 总字节数需要实际计算

**使用Python计算：**
```python
import json

data = {
    "code": 0,
    "message": "success",
    "data": [{
        "chassisName": "Chassis1",
        "chassisNumber": 1,
        "boardName": "Board1",
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

---

## HTTP响应格式错误处理说明

### 当前代码的错误处理机制

根据 `QywApiClient` 的实现，当接收到格式错误的HTTP响应时，代码会按以下方式处理：

#### 1. HTTP状态码检查

**处理逻辑：**
- 如果 `HTTP状态码 != 200`：记录错误日志，返回空结果或 `false`
- 如果 `res == nullptr`（连接失败）：记录错误日志，返回空结果或 `false`

**示例代码：**
```cpp
if (res && res->status == 200) {
    // 继续处理响应
} else {
    spdlog::error("获取板卡信息失败，状态码: {}", (res ? res->status : -1));
    return result;  // 返回空结果
}
```

**在Socket工具中模拟错误响应：**
```
HTTP/1.1 500 Internal Server Error
Content-Type: application/json
Content-Length: 57

{"code":-1,"message":"Internal server error","data":null}
```

#### 2. JSON解析错误

**处理逻辑：**
- 使用 `try-catch` 捕获 `json::exception`
- 捕获到异常时：记录错误日志（`spdlog::error`），返回空结果或 `false`
- **不会抛出异常到上层**，程序继续运行

**可能触发JSON解析错误的情况：**
- 响应体不是有效的JSON格式
- JSON格式不完整（被截断）
- 响应体为空
- 响应体包含非JSON内容（如HTML错误页面）

**示例代码：**
```cpp
try {
    json j = json::parse(res->body);
    // 解析JSON...
} catch (const json::exception& e) {
    spdlog::error("JSON 解析错误: {}", e.what());
    return result;  // 返回空结果
}
```

**在Socket工具中模拟JSON错误响应：**
```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 28

这不是有效的JSON格式
```

#### 3. 响应格式验证

**处理逻辑：**
- 检查 `code` 字段是否存在且为 `0`
- 检查 `data` 字段是否存在
- 如果格式不符合预期，**静默返回空结果**（不记录错误）

**示例代码：**
```cpp
// 解析标准响应格式：{ "code": 0, "message": "success", "data": [...] }
if (j.contains("code") && j["code"] == 0 && j.contains("data")) {
    // 解析data字段...
} else {
    // 格式不符合，静默返回空result（不记录日志）
    return result;
}
```

**在Socket工具中模拟格式错误响应：**

**情况1：code != 0**
```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 51

{"code":-1,"message":"Parameter error","data":null}
```
**处理结果**：静默返回空结果，不记录错误日志

**情况2：缺少data字段**
```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 30

{"code":0,"message":"success"}
```
**处理结果**：静默返回空结果，不记录错误日志

**情况3：code字段缺失**
```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 31

{"message":"success","data":[]}
```
**处理结果**：静默返回空结果，不记录错误日志

#### 4. 字段缺失处理

**处理逻辑：**
- 使用 `value()` 方法提供默认值，字段缺失不会报错
- 使用 `contains()` 检查可选字段是否存在
- 缺失的字段会被设置为默认值（字符串为空，数字为0，数组为空）

**示例代码：**
```cpp
boardInfo.chassisName = boardJson.value("chassisName", "");  // 缺失则默认为空字符串
boardInfo.chassisNumber = boardJson.value("chassisNumber", 0);  // 缺失则默认为0

if (boardJson.contains("fanSpeeds")) {  // 检查可选字段
    // 解析fanSpeeds...
}
```

**在Socket工具中模拟字段缺失响应：**
```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 75

{"code":0,"message":"success","data":[{"chassisNumber":1,"boardNumber":1}]}
```
**处理结果**：缺失的字段（如`chassisName`、`boardAddress`等）会被设置为默认值，不会报错

### 错误处理总结

| 错误类型 | 处理方式 | 日志级别 | 返回值 |
|---------|---------|---------|--------|
| HTTP状态码 != 200 | 记录错误日志 | `error` | 空结果/`false` |
| 连接失败 (res == nullptr) | 记录错误日志 | `error` | 空结果/`false` |
| JSON解析异常 | 记录错误日志 | `error` | 空结果/`false` |
| code != 0 | 静默处理 | 无日志 | 空结果 |
| 缺少data字段 | 静默处理 | 无日志 | 空结果 |
| 缺少code字段 | 静默处理 | 无日志 | 空结果 |
| 字段缺失（使用value默认值） | 使用默认值 | 无日志 | 正常处理 |

### 测试建议

在Socket工具中测试时，可以模拟以下错误场景：

1. **HTTP错误状态码**：
   ```
   HTTP/1.1 404 Not Found
   HTTP/1.1 500 Internal Server Error
   HTTP/1.1 503 Service Unavailable
   ```

2. **无效JSON格式**：
   ```
   HTTP/1.1 200 OK
   Content-Type: application/json
   
   这不是JSON
   ```

3. **格式不符合预期**：
   ```
   HTTP/1.1 200 OK
   Content-Type: application/json
   
   {"code":-1,"message":"Error","data":null}
   ```

4. **字段缺失**：
   ```
   HTTP/1.1 200 OK
   Content-Type: application/json
   
   {"code":0,"data":[]}
   ```

### 注意事项

1. **静默失败**：某些格式错误（如`code != 0`）会静默返回空结果，不会记录错误日志，这可能导致问题难以发现。

2. **建议改进**：可以考虑在格式不符合预期时也记录警告日志，便于调试：
   ```cpp
   if (!j.contains("code") || j["code"] != 0 || !j.contains("data")) {
       spdlog::warn("响应格式不符合预期: code={}, hasData={}", 
                    j.value("code", -1), j.contains("data"));
       return result;
   }
   ```

3. **GetStackInfo特殊处理**：`GetStackInfo` 方法使用 `success` 参数来明确表示API调用是否成功，这是更好的设计模式。

