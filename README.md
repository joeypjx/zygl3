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
- **支持协议**：资源监控、任务查询、任务启动/停止、机箱复位/自检、BMC查询

### 5. HTTP告警接收服务
- **告警接收**：接收板卡异常和组件异常上报
- **心跳检测**：定期向上游API发送IP心跳
- **主备支持**：仅在主节点模式下发送心跳

### 6. 高可用性（HA）支持
- **主备切换**：支持双节点主备模式
- **角色协商**：自动进行主备角色选举
- **故障检测**：主节点故障时自动切换
- **优先级配置**：支持通过优先级控制主备选举

### 7. BMC数据接收
- **组播接收**：接收BMC组播数据
- **实时更新**：自动更新板卡状态

### 8. CLI命令行服务
- **交互式命令**：提供命令行交互界面
- **资源查询**：查询机箱、板卡、业务链路信息

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

# 运行（使用默认配置文件 config.json）
./zygl

# 运行（指定配置文件）
./zygl -c config_full.json
./zygl --config /path/to/config.json

# 查看帮助
./zygl -h
./zygl --help
```

### 安装部署

系统提供了安装脚本，可以自动编译、安装和配置：

```bash
# 运行安装脚本
./install.sh

# 卸载
./uninstall.sh
```

## 配置说明

系统支持通过配置文件进行灵活配置，配置文件采用 JSON 格式。

### 配置文件类型

1. **config.json** - 主配置文件（必需）
   - API 配置（地址、端口、端点路径）
   - 告警服务器配置
   - UDP 组播配置
   - 数据采集配置
   - HA 配置
   - 日志配置

2. **config_full.json** - 完整版配置文件（可选）
   - 包含所有配置项和详细说明
   - 适用于需要完整配置的场景

3. **chassis_config.json** - 机箱配置文件（可选）
   - 定义系统中所有机箱和板卡的配置
   - 如果不存在，系统会使用默认配置或从 config.json 读取

### 配置文件指定方式

系统支持三种方式指定配置文件，优先级从高到低：

1. **命令行参数**（最高优先级）
   ```bash
   ./zygl -c config_full.json
   ./zygl --config /absolute/path/to/config.json
   ```

2. **环境变量**
   ```bash
   export ZYGL_CONFIG=/path/to/config.json
   ./zygl
   ```

3. **默认值**
   - 如果未指定，默认使用 `config.json`

### 配置文件示例

#### 基本配置（config.json）
```json
{
  "api": {
    "base_url": "localhost",
    "port": 8080,
    "endpoints": {
      "boardinfo": "/api/v1/external/qyw/boardinfo",
      "stackinfo": "/api/v1/external/qyw/stackinfo"
    }
  },
  "alert_server": {
    "port": 8888,
    "host": "0.0.0.0"
  },
  "udp": {
    "port": 4106,
    "broadcaster": {
      "multicast_group": "234.186.1.99"
    },
    "listener": {
      "multicast_group": "234.186.1.98"
    }
  },
  "collector": {
    "interval_seconds": 10,
    "board_timeout_seconds": 60
  },
  "ha": {
    "enabled": false
  },
  "logging": {
    "log_dir": "./logs",
    "level": "info"
  }
}
```

#### HA 配置（主备模式）
```json
{
  "ha": {
    "enabled": true,
    "multicast_group": "224.100.200.16",
    "heartbeat": {
      "port": 9999,
      "interval_seconds": 3,
      "timeout_seconds": 9
    },
    "priority": 0
  }
}
```

### 机箱配置

机箱配置可以从以下位置加载（按优先级）：

1. **chassis_config.json** - 独立配置文件（最高优先级）
2. **config.json** - 主配置文件中的 `/topology/chassis` 节点
3. **硬编码默认配置** - 如果以上都不存在，使用默认配置（9个机箱，每个14块板卡）

### 配置项说明

详细配置项说明请参考：
- `config_full.json` - 完整配置示例
- `chassis_config.json` - 机箱配置示例

## 目录结构

```
zygl3/
├── src/
│   ├── domain/                       # 领域层
│   │   ├── board.h                   # 板卡实体
│   │   ├── chassis.h                 # 机箱聚合根
│   │   ├── service.h                 # 组件实体
│   │   ├── stack.h                   # 业务链路聚合根
│   │   ├── task.h                    # 任务实体
│   │   ├── value_objects.h           # 值对象
│   │   ├── i_chassis_repository.h    # 机箱仓储接口
│   │   └── i_stack_repository.h      # 业务链路仓储接口
│   ├── infrastructure/               # 基础设施层
│   │   ├── persistence/              # 持久化
│   │   ├── config/                   # 配置管理
│   │   ├── collectors/               # 采集器
│   │   ├── api_client/               # API客户端
│   │   ├── ha/                       # 高可用性
│   │   └── controller/               # 控制器
│   └── interfaces/                   # 接口层
│       ├── udp/                      # UDP组播
│       ├── http/                     # HTTP服务
│       ├── cli/                      # 命令行接口
│       └── bmc/                      # BMC接收
├── third_party/                      # 第三方库
├── tests/                            # 测试代码
├── docs/                             # 文档
├── config.json                       # 主配置文件
├── config_full.json                  # 完整版配置文件
├── chassis_config.json               # 机箱配置文件
├── main.cpp                          # 主程序
├── CMakeLists.txt                    # CMake配置
├── install.sh                        # 安装脚本
└── README.md                         # 本文件

## 工作流程

### 1. 系统启动
```
1. 加载配置文件（支持命令行参数、环境变量或默认配置）
2. 初始化日志系统
3. 从配置文件或默认配置初始化机箱拓扑（9个机箱，每个14块板卡）
4. 创建API客户端、数据采集服务
5. 启动UDP监听器和广播器
6. 启动HTTP告警接收服务器
7. 启动BMC接收器
8. 启动CLI命令行服务
9. 如果启用HA，启动心跳服务
10. 启动数据采集服务（后台线程）
11. 系统运行，等待请求
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
- **日志库**：spdlog
- **网络**：POSIX Socket（UDP组播、TCP HTTP）
- **构建系统**：CMake
- **线程**：std::thread（多线程支持）

## 开发规范

- 遵循DDD分层架构
- 使用命名空间组织代码（app::domain, app::infrastructure, app::interfaces）
- 使用智能指针管理内存
- 线程安全的仓储实现
- 跨平台支持（macOS, Linux, Windows）
- 配置文件支持JSON格式
- 支持命令行参数和环境变量配置

## 快速开始

### 1. 准备配置文件

复制并修改配置文件：

```bash
cp config_full.json config.json
# 编辑 config.json，修改API地址等配置
```

### 2. 配置机箱拓扑（可选）

如果需要自定义机箱配置：

```bash
# 编辑 chassis_config.json
# 定义机箱和板卡的IP地址、类型等信息
```

### 3. 编译和运行

```bash
mkdir build && cd build
cmake ..
make
./zygl -c ../config.json
```

### 4. 查看日志

```bash
# 日志文件位置（默认）
tail -f logs/zygl.log
```

## 常见问题

### Q: 如何指定不同的配置文件？
A: 使用 `-c` 或 `--config` 参数：
```bash
./zygl -c /path/to/config.json
```

### Q: 如果没有 chassis_config.json 会怎样？
A: 系统会按以下顺序尝试加载：
1. 从 `chassis_config.json` 加载
2. 从 `config.json` 的 `/topology/chassis` 节点加载
3. 使用硬编码默认配置（9个机箱，每个14块板卡）

### Q: 如何启用HA功能？
A: 在配置文件中设置 `ha.enabled = true`，并配置相关参数。

### Q: 如何查看系统状态？
A: 系统提供CLI命令行接口，可以通过交互式命令查询资源状态。

## License

Copyright © 2024
