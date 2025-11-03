# 系统单元架构划分文档

本文档定义了资源管理系统(zygl3)的六个核心单元及其职责划分。

## 单元概览

```
单元1: 资源采集单元
单元2: 资源数据存取单元
单元3: 业务采集单元
单元4: 业务数据存取单元
单元5: 前端组播交互单元
单元6: 命令处理和数据组合单元
```

---

## 单元1: 资源采集单元

### 职责
- 从外部API采集板卡和机箱的实时状态信息
- 发送IP心跳检测，保持与外部系统的连接
- 检测板卡离线状态，维护板卡在线/离线状态

### 包含的组件

#### 文件位置
- `infrastructure/collectors/data_collector_service.cpp`
- `infrastructure/collectors/data_collector_service.h`
- `infrastructure/api_client/qyw_api_client.cpp` (部分)
- `infrastructure/api_client/qyw_api_client.h` (部分)

#### 核心函数
```cpp
// 数据采集服务
DataCollectorService::CollectBoardInfo()
  └─ 调用 QywApiClient::GetBoardInfo() 获取板卡信息
  └─ 转换为领域对象并更新仓储

DataCollectorService::SendHeartbeat()
  └─ 调用 QywApiClient::SendHeartbeat() 发送IP心跳

DataCollectorService::CheckAndMarkOfflineBoards()
  └─ 检查板卡超时并标记为离线

// API客户端（查询接口）
QywApiClient::GetBoardInfo()
  └─ 调用外部API: GET /api/v1/external/qyw/boardinfo
  └─ 解析JSON响应
  └─ 返回 BoardInfoResponse 列表

QywApiClient::SendHeartbeat(clientIp)
  └─ 调用外部API: GET /api/v1/sys-config/client/up?clientIp=xxx
  └─ 返回操作结果
```

### 数据流
```
外部API → QywApiClient::GetBoardInfo() 
       → DataCollectorService::CollectBoardInfo()
       → 转换为领域对象
       → 单元2: 资源数据存取单元
```

---

## 单元2: 资源数据存取单元

### 职责
- 提供机箱和板卡数据的存储和查询接口
- 线程安全的内存存储实现
- 支持机箱、板卡的状态更新和查询

### 包含的组件

#### 文件位置
- `infrastructure/persistence/in_memory_chassis_repository.h`
- `domain/i_chassis_repository.h`
- `domain/chassis.h`
- `domain/board.h`
- `domain/value_objects.h` (部分)

#### 核心接口和实现
```cpp
// 仓储接口
IChassisRepository
  ├─ Save(std::shared_ptr<Chassis>)
  ├─ FindByNumber(int chassisNumber)
  ├─ GetAll()
  ├─ UpdateBoard(int chassisNumber, int boardNumber, const Board&)
  └─ Size()

// 仓储实现
InMemoryChassisRepository
  └─ 线程安全的内存存储
  └─ 使用 std::map<int, std::shared_ptr<Chassis>> 存储

// 领域模型
Chassis (聚合根)
  └─ 包含14块 Board
  └─ 提供 GetAllBoards() 等接口

Board (实体)
  └─ 板卡基本信息（IP、槽位、类型）
  └─ 板卡状态（正常、异常、离线）
  └─ 传感器信息（电压、电流、温度、风扇）
  └─ 任务列表（TaskStatusInfo）
```

### 数据关系
- **依赖关系**: 单元1 更新数据到 单元2
- **被使用**: 单元5、单元6 从 单元2 读取数据

---

## 单元3: 业务采集单元

### 职责
- 从外部API采集业务链路(Stack)的实时状态信息
- 采集业务链路中的组件(Service)和任务(Task)信息
- 采集任务资源使用情况（CPU、内存、网络、GPU等）

### 包含的组件

#### 文件位置
- `infrastructure/collectors/data_collector_service.cpp`
- `infrastructure/collectors/data_collector_service.h`
- `infrastructure/api_client/qyw_api_client.cpp` (部分)
- `infrastructure/api_client/qyw_api_client.h` (部分)

#### 核心函数
```cpp
// 数据采集服务
DataCollectorService::CollectStackInfo()
  └─ 调用 QywApiClient::GetStackInfo() 获取业务链路信息
  └─ 转换为领域对象并更新仓储

// API客户端（查询接口）
QywApiClient::GetStackInfo()
  └─ 调用外部API: POST /api/v1/external/qyw/stackinfo
  └─ 解析JSON响应
  └─ 返回 StackInfoResponse 列表
```

### 数据流
```
外部API → QywApiClient::GetStackInfo() 
       → DataCollectorService::CollectStackInfo()
       → 转换为领域对象
       → 单元4: 业务数据存取单元
```

---

## 单元4: 业务数据存取单元

### 职责
- 提供业务链路数据的存储和查询接口
- 线程安全的内存存储实现
- 支持业务链路、组件、任务的状态更新和查询

### 包含的组件

#### 文件位置
- `infrastructure/persistence/in_memory_stack_repository.h`
- `domain/i_stack_repository.h`
- `domain/stack.h`
- `domain/service.h`
- `domain/task.h`
- `domain/value_objects.h` (部分)

#### 核心接口和实现
```cpp
// 仓储接口
IStackRepository
  ├─ Save(std::shared_ptr<Stack>)
  ├─ FindByUUID(const std::string& stackUUID)
  ├─ GetAll()
  ├─ Clear()
  └─ Size()

// 仓储实现
InMemoryStackRepository
  └─ 线程安全的内存存储
  └─ 使用 std::map<std::string, std::shared_ptr<Stack>> 存储

// 领域模型
Stack (聚合根)
  └─ 包含多个 Service
  └─ 业务链路基本信息（名称、UUID、标签）
  └─ 部署状态、运行状态

Service (实体)
  └─ 包含多个 Task
  └─ 组件基本信息（名称、UUID、类型）
  └─ 组件状态（已停用、已启用、运行正常、运行异常）

Task (实体)
  └─ 任务基本信息（ID、状态、板卡地址）
  └─ 资源使用情况（ResourceUsage）
```

### 数据关系
- **依赖关系**: 单元3 更新数据到 单元4
- **被使用**: 单元5、单元6 从 单元4 读取数据

---

## 单元5: 前端组播交互单元

### 职责
- 提供UDP组播的底层通信能力
- 负责UDP socket的创建、绑定、发送和接收
- 管理组播地址和端口配置
- 提供组播服务的启动和停止控制

### 包含的组件

#### 文件位置
- `interfaces/udp/resource_monitor_broadcaster.h`
- `interfaces/udp/resource_monitor_broadcaster.cpp` (部分)

#### 核心函数
```cpp
// UDP广播器基础设施
ResourceMonitorBroadcaster::Start()
  └─ 启动UDP组播发送服务

ResourceMonitorBroadcaster::Stop()
  └─ 停止UDP组播发送服务

ResourceMonitorBroadcaster::SetCommand(...)
  └─ 设置UDP命令码配置

// UDP监听器基础设施
ResourceMonitorListener::Start()
  └─ 启动UDP组播接收服务

ResourceMonitorListener::Stop()
  └─ 停止UDP组播接收服务

ResourceMonitorListener::SetCommand(...)
  └─ 设置UDP命令码配置
```

### 说明
- 本单元只负责**通信基础设施**，不包含命令处理逻辑
- 命令处理和数据组合属于**单元6**的职责
- 本单元提供底层的UDP发送/接收能力给单元6使用

---

## 单元6: 命令处理和数据组合单元

### 职责
- 接收和解析来自外部的各种命令（UDP、HTTP）
- 执行命令对应的业务逻辑
- 将领域数据组合成各种协议格式（UDP报文、HTTP响应、API请求）
- 进行数据格式转换和序列化/反序列化

### 包含的组件

#### 文件位置
- `interfaces/udp/resource_monitor_broadcaster.cpp` (命令处理部分)
- `interfaces/udp/resource_monitor_broadcaster.h` (命令处理部分)
- `interfaces/http/alert_receiver_server.cpp`
- `interfaces/http/alert_receiver_server.h`
- `infrastructure/api_client/qyw_api_client.cpp` (控制命令部分)
- `infrastructure/api_client/qyw_api_client.h` (控制命令部分)

#### 子模块1: UDP命令处理模块

**职责**: 处理UDP协议的命令请求和响应

```cpp
// 请求接收和解析
ResourceMonitorListener::ListenLoop()
  ├─ 接收资源监控请求 (F000H)
  ├─ 接收任务查询请求 (F005H)
  ├─ 接收任务启动请求 (F003H)
  └─ 接收任务停止请求 (F004H)

// 响应数据组合和发送
ResourceMonitorBroadcaster::BuildResponseData(response)
  ├─ 从单元2读取板卡状态
  ├─ 从单元4读取任务状态
  ├─ MapBoardStatusToArray() - 映射板卡状态到字节数组
  └─ MapTaskStatusToArray() - 映射任务状态到字节数组

ResourceMonitorBroadcaster::SendResponse(requestId)
  └─ 组合资源监控响应报文并发送

ResourceMonitorBroadcaster::BuildTaskQueryResponse(response, request)
  └─ 通过机箱号、板卡号、任务序号查找任务
  └─ 组合任务查询响应报文

ResourceMonitorBroadcaster::HandleTaskStartRequest(request)
  ├─ 解析工作模式/标签
  ├─ 调用单元6的API控制命令模块执行部署
  └─ 组合并发送任务启动响应

ResourceMonitorBroadcaster::HandleTaskStopRequest(request)
  ├─ 调用单元6的API控制命令模块执行停用
  └─ 组合并发送任务停止响应

ResourceMonitorBroadcaster::SendFaultReport(faultDescription)
  └─ 组合故障上报报文并发送

// 数据转换工具
ResourceMonitorBroadcaster::WorkModeToLabel(workMode)
  └─ 工作模式/标签转换

ResourceMonitorBroadcaster::IpStringToUint32(ipStr)
  └─ IP地址格式转换
```

#### 子模块2: HTTP告警处理模块

**职责**: 处理HTTP协议的告警上报请求

```cpp
// 请求处理
AlertReceiverServer::HandleBoardAlert(req, res)
  ├─ 解析板卡异常上报JSON请求
  ├─ 转换为 BoardAlertRequest 结构
  ├─ 调用单元5发送UDP故障上报
  └─ 组合HTTP响应

AlertReceiverServer::HandleServiceAlert(req, res)
  ├─ 解析组件异常上报JSON请求
  ├─ 转换为 ServiceAlertRequest 结构
  ├─ 调用单元5发送UDP故障上报
  └─ 组合HTTP响应

// 响应组合
AlertReceiverServer::SendSuccessResponse(res)
  └─ 组合成功响应JSON

AlertReceiverServer::SendErrorResponse(res, message)
  └─ 组合错误响应JSON
```

#### 子模块3: API控制命令模块

**职责**: 执行业务控制命令（部署、停用、复位）

```cpp
// 业务链路部署
QywApiClient::DeployStacks(labels, account, password, stop)
  ├─ 组合部署请求JSON
  ├─ 调用外部API: POST /api/v1/stacks/labels/deploy
  ├─ ParseDeployResponse() - 解析部署响应
  └─ 返回 DeployResponse

// 业务链路停用
QywApiClient::UndeployStacks(labels)
  ├─ 组合停用请求JSON
  ├─ 调用外部API: POST /api/v1/stacks/labels/undeploy
  ├─ ParseDeployResponse() - 解析停用响应
  └─ 返回 DeployResponse

// 业务链路复位
QywApiClient::ResetStacks()
  ├─ 调用外部API: GET /api/v1/stacks/labels/reset
  └─ 返回操作结果

// 响应解析
QywApiClient::ParseDeployResponse(jsonStr)
  └─ 解析部署/停用响应的JSON
  └─ 提取成功和失败的业务链路信息
```

### 数据流
```
外部请求 → 单元6解析 → 单元6处理
                ↓
        单元2/单元4读取数据
                ↓
        单元6组合响应 → 单元5发送
```

---

## 独立于各单元的功能

以下功能不属于上述6个单元，作为系统基础设施：

### CLI命令处理
- **文件**: `interfaces/cli/cli_service.cpp`, `cli_service.h`
- **职责**: 提供交互式命令行接口，用于调试和查询

### 配置管理
- **文件**: 
  - `infrastructure/config/config_manager.cpp`, `config_manager.h`
  - `infrastructure/config/chassis_factory.h`
- **职责**: 系统配置加载、机箱拓扑初始化

---

## 单元依赖关系图

```
┌─────────────────┐
│   外部API系统    │
└────────┬────────┘
         │
    ┌────┴────┐
    │         │
┌───▼────┐ ┌─▼──────┐
│单元1:  │ │单元3:  │
│资源采集│ │业务采集│
└───┬────┘ └─┬──────┘
    │        │
    ▼        ▼
┌────────┐ ┌────────┐
│单元2:  │ │单元4:  │
│资源存取│ │业务存取│
└───┬────┘ └───┬────┘
    │          │
    └────┬─────┘
         │
    ┌────▼────┐
    │单元6:   │
    │命令处理 │
    │数据组合 │
    └────┬────┘
         │
    ┌────▼────┐
    │单元5:   │
    │组播交互 │
    └────┬────┘
         │
    ┌────▼────┐
    │   前端   │
    └─────────┘
```

---

## 单元职责边界说明

### 清晰的边界
- **单元1和单元3**: 只负责数据采集，不涉及命令处理
- **单元2和单元4**: 纯数据存取，无业务逻辑
- **单元5**: 只提供通信能力，不处理命令逻辑
- **单元6**: 处理所有命令，但依赖单元5进行通信

### 接口约定
- 单元1 → 单元2: 通过 `IChassisRepository` 接口
- 单元3 → 单元4: 通过 `IStackRepository` 接口
- 单元6 → 单元2/单元4: 通过仓储接口读取数据
- 单元6 → 单元5: 通过 `ResourceMonitorBroadcaster` 发送UDP

---

## 扩展性说明

### 单元6的扩展性
单元6设计为可扩展的命令处理中心，未来可以添加：
- 新的命令协议支持（如WebSocket、gRPC）
- 新的命令类型处理
- 新的数据格式转换器
- 命令处理中间件（日志、权限、限流等）

### 各单元的独立性
每个单元都可以独立：
- 进行单元测试
- 替换实现（如将单元2的内存存储替换为数据库存储）
- 优化性能（如单元1的采集频率、单元6的并发处理）

---

## 总结

通过这6个单元的划分，系统实现了：
- ✅ **职责清晰**: 每个单元有明确的单一职责
- ✅ **低耦合**: 单元间通过接口交互，减少直接依赖
- ✅ **高内聚**: 相关功能集中在同一单元内
- ✅ **可扩展**: 单元6作为命令处理中心，易于扩展新协议
- ✅ **可测试**: 每个单元可以独立测试和验证

---

**文档版本**: v1.0  
**最后更新**: 2024

