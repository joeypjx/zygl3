# 资源管理系统 (zygl3)

一个基于DDD（领域驱动设计）的资源监控和管理系统，用于监控和管理机箱、板卡、业务链路等资源。

## 功能特性

### 1. 领域模型（Domain Layer）
- **机箱（Chassis）**：包含14块板卡
- **板卡（Board）**：3种类型（计算板卡、交换板卡、电源板卡）
- **业务链路（Stack）**：包含多个组件
- **组件（Service）**：包含多个任务
- **任务（Task）**：运行实例，包含详细的资源使用情况

### 2. 数据存储（Infrastructure Layer）
- **内存数据库**：使用内存存储，支持线程安全访问
- **仓储模式**：抽象的数据访问层
- **初始化**：系统启动时自动初始化9个机箱（每个14块板卡）

### 3. 数据采集
- **周期性采集**：每10秒自动从外部API获取最新数据
- **API客户端**：使用cpp-httplib进行HTTP调用
- **JSON解析**：使用nlohmann/json解析API响应
- **自动更新**：将API数据转换为领域对象并更新到数据库

### 4. UDP组播服务
- **监听器**：接收前端请求（234.186.1.98:4106）
- **广播器**：发送响应报文（234.186.1.99:4106）
- **数据协议**：
  - 板卡状态：9个机箱×12块板卡=108字节
  - 任务状态：9个机箱×12块板卡×8个任务=864字节

## 系统架构

### DDD分层架构
```
┌─────────────────────────────────────────────┐
│              Domain Layer                   │
│  (Chassis, Board, Stack, Service, Task)     │
└─────────────────────────────────────────────┘
                    ▲
                    │
┌─────────────────────────────────────────────┐
│          Infrastructure Layer               │
│  - InMemoryChassisRepository               │
│  - InMemoryStackRepository                 │
│  - DataCollectorService                    │
│  - QywApiClient                            │
└─────────────────────────────────────────────┘
                    ▲
                    │
┌─────────────────────────────────────────────┐
│            Interfaces Layer                 │
│  - ResourceMonitorBroadcaster (UDP)        │
│  - ResourceMonitorListener (UDP)           │
└─────────────────────────────────────────────┘
```

### 功能单元划分
系统按功能划分为6个核心单元，详见 [单元架构文档](docs/UNIT_ARCHITECTURE.md)：

1. **资源采集单元** - 板卡和机箱数据采集
2. **资源数据存取单元** - 机箱和板卡数据存储
3. **业务采集单元** - 业务链路数据采集
4. **业务数据存取单元** - 业务链路数据存储
5. **前端组播交互单元** - UDP组播通信基础设施
6. **命令处理和数据组合单元** - 命令处理和数据格式转换

## 构建系统

### 前置要求
- C++17 编译器
- CMake 3.10+

### 编译步骤

```bash
# 创建构建目录
mkdir build && cd build

# 生成构建文件
cmake ..

# 编译
make

# 运行
./zygl3
```

## 配置说明

### API配置
在 `main.cpp` 中修改API地址：

```cpp
std::string apiBaseUrl = "your-api-server";
int apiPort = 8080;
```

### UDP组播配置
在 `main.cpp` 中修改组播地址和端口：

```cpp
auto broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
    chassisRepo, "234.186.1.99", 0x100A);  // 响应组播
auto listener = std::make_shared<ResourceMonitorListener>(
    broadcaster, "234.186.1.98", 0x100A);  // 请求组播
```

## 目录结构

```
zygl3/
├── domain/                           # 领域层
│   ├── board.h                      # 板卡实体
│   ├── chassis.h                    # 机箱聚合根
│   ├── service.h                    # 组件实体
│   ├── stack.h                      # 业务链路聚合根
│   ├── task.h                       # 任务实体
│   ├── value_objects.h              # 值对象
│   ├── i_chassis_repository.h       # 机箱仓储接口
│   └── i_stack_repository.h         # 业务链路仓储接口
├── infrastructure/                    # 基础设施层
│   ├── persistence/                  # 持久化
│   │   ├── in_memory_chassis_repository.h
│   │   └── in_memory_stack_repository.h
│   ├── config/                       # 配置
│   │   └── chassis_factory.h        # 机箱工厂
│   ├── collectors/                   # 采集器
│   │   ├── data_collector_service.h
│   │   └── data_collector_service.cpp
│   └── api_client/                   # API客户端
│       ├── qyw_api_client.h
│       ├── qyw_api_client.cpp
│       ├── httplib.h                # cpp-httplib
│       └── json.hpp                 # nlohmann/json
├── interfaces/                        # 接口层
│   └── udp/
│       ├── resource_monitor_broadcaster.h
│       └── resource_monitor_broadcaster.cpp
├── main.cpp                          # 主程序
├── CMakeLists.txt                    # CMake配置
├── docs/
│   ├── UNIT_ARCHITECTURE.md         # 单元架构划分文档
│   ├── TEST_CASES.md                 # 单元测试用例设计文档
│   ├── Server_API.txt                # API接口文档
│   ├── UDP.txt                       # UDP协议文档
│   └── Dialog.txt                    # 设计讨论文档
└── README.md                         # 本文件

## 工作流程

### 1. 系统启动
```
1. 初始化9个机箱（每个14块板卡）
2. 启动UDP监听器和广播器
3. 启动数据采集服务
4. 系统运行，等待UDP请求
```

### 2. 数据采集
```
每10秒循环执行：
1. 调用外部API获取板卡信息
2. 调用外部API获取业务链路信息
3. 解析JSON响应
4. 转换为领域对象
5. 更新内存数据库
```

### 3. UDP响应
```
1. 接收到前端请求
2. 从内存数据库读取当前状态
3. 构建1000字节响应报文
4. 通过UDP组播发送响应
```

## 技术栈

- **语言**：C++17
- **架构**：DDD（领域驱动设计）
- **HTTP客户端**：cpp-httplib
- **JSON解析**：nlohmann/json
- **网络**：POSIX Socket（UDP组播）
- **构建系统**：CMake

## 开发规范

- 遵循DDD分层架构
- 使用命名空间组织代码（app::domain, app::infrastructure, app::interfaces）
- 使用智能指针管理内存
- 线程安全的仓储实现
- 跨平台支持（macOS, Linux, Windows）

## License

Copyright © 2024
