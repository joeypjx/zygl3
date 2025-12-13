# 主备冗余设计方案

## 1. 架构概述

### 1.1 主备模式
- **主节点（Primary）**：正常提供服务，处理所有请求
- **备节点（Standby）**：热备状态，同步主节点数据，随时准备接管
- **2节点架构**：本设计仅支持2节点主备场景

### 1.2 设计原则
- **自动故障检测**：通过心跳机制检测主节点故障
- **快速切换**：故障检测后自动切换，最小化服务中断
- **数据独立性**：主备节点各自通过API获取最新数据，无需状态同步
- **无单点故障**：主备节点可以独立运行
- **简化设计**：针对2节点场景优化，避免不必要的复杂度

## 2. 核心组件设计

### 2.1 心跳和角色协商服务（HeartbeatService）

```cpp
// src/infrastructure/ha/heartbeat_service.h
class HeartbeatService {
public:
    HeartbeatService(
        const std::string& multicastGroup,  // 组播地址
        uint16_t heartbeatPort,
        int priority = 0,  // 优先级（数值越大优先级越高，0表示使用IP地址比较）
        int heartbeatInterval = 3,  // 心跳间隔（秒）
        int timeoutThreshold = 9   // 超时阈值（秒，3次心跳）
    );
    
    void Start();
    void Stop();
    
    // 核心接口：获取当前角色
    enum class Role { Primary, Standby, Unknown };
    Role GetCurrentRole() const { return m_currentRole.load(); }
    bool IsPrimary() const { return GetCurrentRole() == Role::Primary; }
    
    // 状态回调
    using RoleChangeCallback = std::function<void(Role oldRole, Role newRole)>;
    void SetRoleChangeCallback(RoleChangeCallback callback);
    
private:
    void HeartbeatLoop();  // 主节点：发送心跳
    void ReceiveLoop();    // 接收组播消息
    void CheckAndSwitchRole();  // 检查并切换角色（2节点简化逻辑）
    void SwitchToPrimary();
    void SwitchToStandby();
    
    // 配置参数
    std::string m_multicastGroup;
    uint16_t m_heartbeatPort;
    int m_priority;
    int m_heartbeatInterval;
    int m_timeoutThreshold;
    
    // 核心状态（只需要这些）
    std::atomic<Role> m_currentRole;  // 当前角色（核心属性）
    std::atomic<bool> m_running;
    std::atomic<std::chrono::system_clock::time_point> m_lastPrimaryHeartbeat;  // 最后收到主节点心跳的时间
    
    // 网络通信
    int m_socket;
    std::string m_localNodeId;  // 本节点标识（IP地址）
    
    // 线程管理
    std::thread m_heartbeatThread;
    std::thread m_receiveThread;
    
    // 回调
    RoleChangeCallback m_roleChangeCallback;
};
```

**设计说明**：
- **简化设计**：针对2节点场景，不需要维护节点列表
- **核心逻辑**：
  - 启动时：发送选举公告，如果收到主节点心跳，就是Standby；否则就是Primary
  - 运行时：主节点发送心跳，备节点监听；如果备节点超时未收到心跳，切换为Primary
- **只需要知道**：1) 当前角色 2) 最后收到主节点心跳的时间

### 2.2 服务角色检查机制

**设计原则**：各服务自行检查角色，决定是否执行操作，无需ServiceManager统一管理。

**实现方式**：各服务在构造函数中注入 `HeartbeatService` 的引用，在执行关键操作前检查角色。

#### 2.2.1 ResourceMonitorBroadcaster

```cpp
class ResourceMonitorBroadcaster {
public:
    ResourceMonitorBroadcaster(
        std::shared_ptr<IChassisRepository> chassisRepo,
        std::shared_ptr<IStackRepository> stackRepo,
        std::shared_ptr<QywApiClient> apiClient,
        std::shared_ptr<HeartbeatService> heartbeatService,  // 注入心跳服务
        const std::string& multicastGroup,
        uint16_t port
    );
    
    bool SendResourceMonitorResponse(uint32_t requestId) {
        // 检查角色：只有主节点才发送响应
        if (!m_heartbeatService || !m_heartbeatService->IsPrimary()) {
            spdlog::debug("当前为备节点，不发送资源监控响应");
            return false;
        }
        // ... 发送响应逻辑 ...
    }
    
private:
    std::shared_ptr<HeartbeatService> m_heartbeatService;
};
```

#### 2.2.2 ResourceMonitorListener

```cpp
class ResourceMonitorListener {
public:
    ResourceMonitorListener(
        std::shared_ptr<ResourceMonitorBroadcaster> broadcaster,
        std::shared_ptr<HeartbeatService> heartbeatService,  // 注入心跳服务
        const std::string& multicastGroup,
        uint16_t port
    );
    
    void ListenLoop() {
        while (m_running) {
            // 接收组播请求
            ssize_t recvLen = recvfrom(...);
            
            // 检查角色：只有主节点才处理请求
            if (m_heartbeatService && !m_heartbeatService->IsPrimary()) {
                // 备节点：接收请求但不处理，继续监听
                spdlog::debug("当前为备节点，收到组播请求但不处理");
                continue;
            }
            
            // 主节点：处理请求并转发给broadcaster
            // ... 处理请求逻辑 ...
        }
    }
    
private:
    std::shared_ptr<HeartbeatService> m_heartbeatService;
};
```

**注意**：
- Listener 接收组播请求，但只有主节点才处理
- 备节点收到请求后直接跳过，不调用broadcaster
- 这样可以避免备节点执行不必要的处理逻辑

#### 2.2.3 AlertReceiverServer

```cpp
class AlertReceiverServer {
public:
    AlertReceiverServer(
        std::shared_ptr<IChassisRepository> chassisRepo,
        std::shared_ptr<IStackRepository> stackRepo,
        std::shared_ptr<ResourceMonitorBroadcaster> broadcaster,
        std::shared_ptr<QywApiClient> apiClient,
        const std::string& clientIp,
        std::shared_ptr<HeartbeatService> heartbeatService,  // 注入心跳服务
        int port,
        const std::string& host,
        int heartbeatInterval = 10
    );
    
    void HandleBoardAlert(const BoardAlertRequest& request) {
        // 检查角色：只有主节点才处理告警
        if (!m_heartbeatService || !m_heartbeatService->IsPrimary()) {
            spdlog::debug("当前为备节点，不处理板卡告警");
            return;
        }
        // ... 处理告警逻辑 ...
    }
    
    void SendHeartbeat() {
        // 检查角色：只有主节点才发送心跳
        if (m_heartbeatService && !m_heartbeatService->IsPrimary()) {
            spdlog::debug("当前为备节点，不发送IP心跳检测");
            return;
        }
        // ... 发送心跳逻辑 ...
    }
    
private:
    std::shared_ptr<HeartbeatService> m_heartbeatService;
};
```

**注意**：
- `AlertReceiverServer` 负责发送IP心跳保活（通过API）
- 只有主节点才发送心跳，备节点不发送
- 告警处理也只有主节点执行

#### 2.2.4 BmcReceiver

```cpp
class BmcReceiver {
public:
    BmcReceiver(
        std::shared_ptr<IChassisRepository> chassisRepo,
        std::shared_ptr<HeartbeatService> heartbeatService,  // 注入心跳服务
        const std::string& multicastGroup,
        uint16_t port
    );
    
    void HandleReceivedPacket(const char* data, size_t length) {
        // 检查角色：只有主节点才处理BMC报文
        if (!m_heartbeatService || !m_heartbeatService->IsPrimary()) {
            spdlog::debug("当前为备节点，不处理BMC报文");
            return;
        }
        // ... 处理BMC报文逻辑 ...
    }
    
private:
    std::shared_ptr<HeartbeatService> m_heartbeatService;
};
```

#### 2.2.5 DataCollectorService

```cpp
class DataCollectorService {
    // 数据采集服务在主备节点都运行，不受角色影响
    // 不需要检查角色
};
```

**优势**：
- **简化架构**：无需ServiceManager，减少一层抽象
- **职责清晰**：各服务自己管理自己的行为
- **灵活性高**：不同服务可以有不同的角色检查逻辑
- **易于维护**：角色检查逻辑分散在各服务中，更容易理解和修改
- **切换更快**：无需启动/停止服务，只需角色状态变化

#### 2.2.6 在main.cpp中的初始化示例

```cpp
int main() {
    // ... 初始化仓储、API客户端等 ...
    
    // 1. 创建并启动心跳服务（最先启动）
    auto heartbeatService = std::make_shared<HeartbeatService>(
        ConfigManager::GetString("/ha/multicast_group", "224.100.200.16"),
        static_cast<uint16_t>(ConfigManager::GetInt("/ha/heartbeat/port", 9999)),
        ConfigManager::GetInt("/ha/priority", 0),
        ConfigManager::GetInt("/ha/heartbeat/interval_seconds", 3),
        ConfigManager::GetInt("/ha/heartbeat/timeout_seconds", 9)
    );
    
    // 等待角色协商完成（可选，也可以异步）
    std::this_thread::sleep_for(std::chrono::seconds(2));
    heartbeatService->Start(HeartbeatService::Role::Unknown);  // 从Unknown开始，自动协商
    
    // 2. 创建服务（注入心跳服务引用）
    auto broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
        chassisRepo, stackRepo, apiClient, heartbeatService,  // 注入心跳服务
        udpBroadcasterGroup, udpPort
    );
    
    auto listener = std::make_shared<ResourceMonitorListener>(
        broadcaster, udpListenerGroup, udpPort
    );
    
    auto alertServer = std::make_shared<AlertReceiverServer>(
        chassisRepo, stackRepo, broadcaster, heartbeatService,  // 注入心跳服务
        httpAlertPort, httpAlertHost
    );
    
    auto bmcReceiver = std::make_shared<BmcReceiver>(
        chassisRepo, heartbeatService,  // 注入心跳服务
        bmcMulticastGroup, bmcPort
    );
    
    // 3. 启动所有服务（主备节点都启动）
    broadcaster->Start();
    listener->Start();
    alertServer->Start();
    bmcReceiver->Start();
    
    // 数据采集服务不需要心跳服务（主备都运行）
    DataCollectorService collector(chassisRepo, stackRepo, apiClient, 
                                    clientIp, intervalSeconds, boardTimeoutSeconds);
    collector.Start();
    
    // ... 主循环 ...
}
```

### 2.3 组播实现细节

```cpp
// 组播消息发送示例
void HeartbeatService::SendMulticastMessage(const MulticastHAMessage& msg) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_heartbeatPort);
    inet_pton(AF_INET, m_multicastGroup.c_str(), &addr.sin_addr);
    
    // 设置组播TTL（限制在同一网段）
    int ttl = 1;
    setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    
    // 发送组播消息
    sendto(m_socket, &msg, sizeof(msg), 0, 
           (struct sockaddr*)&addr, sizeof(addr));
}

// 组播消息接收示例
void HeartbeatService::ReceiveLoop() {
    char buffer[1024];
    struct sockaddr_in fromAddr;
    socklen_t addrLen = sizeof(fromAddr);
    
    while (m_running) {
        ssize_t recvLen = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                                   (struct sockaddr*)&fromAddr, &addrLen);
        if (recvLen == sizeof(MulticastHAMessage)) {
            MulticastHAMessage* msg = reinterpret_cast<MulticastHAMessage*>(buffer);
            // 处理收到的消息
            HandleReceivedMessage(*msg, fromAddr);
        }
    }
}
```

## 3. 工作流程

### 3.1 系统启动

```
1. 两个节点同时启动
2. 创建并启动心跳服务（HeartbeatService）
3. 通过组播协商机制确定主备角色
4. 启动所有服务（主备节点都启动）：
   - 数据采集服务（主备都运行）
   - UDP广播器（主备都启动，但只有主节点发送响应）
   - UDP监听器（主备都启动，接收请求）
   - HTTP告警服务器（主备都启动，但只有主节点处理告警）
   - BMC接收器（主备都启动，但只有主节点处理BMC报文）
5. 各服务在运行时检查角色，决定是否执行操作
```

### 3.2 正常运行

```
主节点：
- 所有服务都在运行
- 检查角色为Primary，执行所有操作：
  - 处理UDP请求并发送响应
  - 接收并处理HTTP告警
  - 接收并处理BMC报文
  - 定期调用API获取最新数据并更新仓储
  - 发送心跳到组播组

备节点：
- 所有服务都在运行
- 检查角色为Standby，跳过业务处理：
  - 接收UDP请求但不处理（或处理但不发送响应）
  - 接收HTTP告警但不处理
  - 接收BMC报文但不处理
  - 定期调用API获取最新数据并更新仓储（保持数据最新）
  - 监听组播心跳，监控主节点健康状态
```

### 3.3 故障切换

```
1. 备节点检测到主节点心跳超时（连续3次未收到）
2. 备节点通过组播重新选举，切换为主节点
3. 新主节点：
   - 所有服务已在运行（无需启动）
   - 角色检查从Standby变为Primary
   - 开始执行所有业务操作：
     * 处理UDP请求并发送响应
     * 接收并处理HTTP告警
     * 接收并处理BMC报文
   - 数据采集服务已在运行，数据已是最新
   - 开始发送心跳到组播组
4. 原主节点恢复后：
   - 检测到已有主节点（收到心跳）
   - 自动切换为备节点
   - 角色检查从Primary变为Standby
   - 停止执行业务操作（但服务仍在运行）
   - 继续通过API获取最新数据
```

## 4. 数据获取机制

### 4.1 数据来源

主备节点都通过 `DataCollectorService` 定期调用外部API获取最新数据：
- **板卡信息**：通过 `QywApiClient::GetBoardInfo()` 获取
- **业务链路信息**：通过 `QywApiClient::GetStackInfo()` 获取
- **心跳保活**：通过 `QywApiClient::SendHeartbeat()` 发送

### 4.2 数据更新策略

**主节点**：
- 数据采集服务正常运行
- 定期（默认10秒）调用API更新数据
- 处理所有业务请求

**备节点**：
- 数据采集服务正常运行（保持数据最新）
- 定期（默认10秒）调用API更新数据
- 不处理业务请求（静默模式）

**优势**：
- 无需状态同步，简化架构
- 备节点切换后数据已是最新（无需等待同步）
- 减少网络开销（不需要主备之间的同步流量）
- 数据来源统一（都来自API，保证一致性）

## 5. 心跳机制

### 5.1 组播消息格式

```cpp
#pragma pack(push, 1)
struct MulticastHAMessage {
    uint16_t magic;         // 魔数 0xBEA7 (BEAT的ASCII编码)
    uint8_t msgType;        // 消息类型：1=选举公告, 2=心跳, 3=角色声明
    uint8_t role;           // 角色：1=Primary, 2=Standby, 0=Unknown
    int32_t priority;       // 优先级（网络字节序）
    uint32_t sequence;      // 序列号（网络字节序）
    uint64_t timestamp;     // 时间戳（网络字节序）
    char nodeId[32];        // 节点标识（IP地址）
};
#pragma pack(pop)
```

**消息类型**：
- **选举公告（1）**：节点启动时发送，用于发现其他节点和角色协商
- **心跳（2）**：主节点定期发送，用于健康检测
- **角色声明（3）**：角色切换时发送，通知其他节点

### 5.2 组播协商流程

#### 5.2.1 节点启动

```
1. 节点启动，加入组播组
2. 发送选举公告消息（msgType=1, role=Unknown）
3. 等待一段时间（如2秒）收集其他节点的响应
4. 根据收集到的节点信息进行角色选举：
   - 比较优先级（priority字段）
   - 如果优先级相同，比较nodeId（IP地址）
   - 优先级最高（或IP最小）的节点成为Primary
5. 确定角色后，发送角色声明消息（msgType=3）
```

#### 5.2.2 正常运行

```
主节点：
- 每3秒发送一次心跳消息（msgType=2, role=Primary）
- 通过UDP组播发送，所有节点都能收到

备节点：
- 持续监听组播消息
- 记录主节点的最后心跳时间
- 如果超过9秒（3次心跳间隔）未收到主节点心跳，判定主节点故障
- 触发角色重新选举
```

#### 5.2.3 故障切换

```
1. 备节点检测到主节点心跳超时
2. 备节点发送选举公告消息
3. 等待一段时间收集其他备节点的响应
4. 重新选举：剩余节点中优先级最高的成为新的Primary
5. 新主节点发送角色声明消息
6. 新主节点开始发送心跳
```

### 5.3 主节点选举机制（2节点场景）

#### 5.3.1 选举规则

对于2节点场景，选举规则非常简单：

1. **优先级比较**（如果配置了priority > 0）
   - 优先级数值**越大**的节点成为主节点
   - 例如：priority=100 的节点优先于 priority=50 的节点

2. **IP地址比较**（如果priority == 0或优先级相同）
   - IP地址数值**越小**的节点成为主节点
   - 例如：192.168.1.10 优先于 192.168.1.20

#### 5.3.2 选举算法（2节点简化版本）

```cpp
// 节点启动时的初始选举
void InitialElection() {
    // 1. 发送选举公告
    SendElectionAnnouncement();
    
    // 2. 等待一段时间（如2秒）收集响应
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 3. 检查是否收到主节点心跳
    auto now = std::chrono::system_clock::now();
    auto lastHeartbeat = m_lastPrimaryHeartbeat.load();
    
    if (lastHeartbeat != std::chrono::system_clock::time_point{} &&
        (now - lastHeartbeat) < std::chrono::seconds(5)) {
        // 收到主节点心跳，本节点是备节点
        SwitchToStandby();
    } else {
        // 未收到主节点心跳，本节点成为主节点
        SwitchToPrimary();
    }
}

// 运行时检查并切换角色
void CheckAndSwitchRole() {
    auto now = std::chrono::system_clock::now();
    auto lastHeartbeat = m_lastPrimaryHeartbeat.load();
    auto timeout = std::chrono::seconds(m_timeoutThreshold);
    
    if (m_currentRole.load() == Role::Standby) {
        // 备节点：检查是否超时未收到主节点心跳
        if (lastHeartbeat == std::chrono::system_clock::time_point{} ||
            (now - lastHeartbeat) > timeout) {
            // 主节点故障，本节点成为主节点
            spdlog::info("检测到主节点故障，切换为主节点");
            SwitchToPrimary();
        }
    } else if (m_currentRole.load() == Role::Primary) {
        // 主节点：如果收到其他主节点心跳，需要降级
        // （这种情况在2节点场景下很少见，可能是网络分区恢复）
        // 可以通过比较优先级决定是否降级
    }
}

// 接收组播消息
void ReceiveLoop() {
    while (m_running) {
        MulticastHAMessage msg;
        struct sockaddr_in fromAddr;
        socklen_t addrLen = sizeof(fromAddr);
        
        ssize_t recvLen = recvfrom(m_socket, &msg, sizeof(msg), 0,
                                   (struct sockaddr*)&fromAddr, &addrLen);
        
        if (recvLen == sizeof(MulticastHAMessage) && msg.magic == 0xBEAT) {
            if (msg.msgType == 2 && msg.role == 1) {  // 收到主节点心跳
                // 更新最后收到心跳的时间
                m_lastPrimaryHeartbeat = std::chrono::system_clock::now();
                
                // 如果本节点也是主节点，且收到其他主节点心跳，需要处理冲突
                if (m_currentRole.load() == Role::Primary && 
                    std::string(msg.nodeId) != m_localNodeId) {
                    // 比较优先级，决定是否降级
                    if (ShouldYieldToOtherPrimary(msg)) {
                        spdlog::warn("检测到其他主节点，降级为备节点");
                        SwitchToStandby();
                    }
                }
            } else if (msg.msgType == 1) {  // 收到选举公告
                // 如果本节点是主节点，发送角色声明
                if (m_currentRole.load() == Role::Primary) {
                    SendRoleDeclaration();
                }
            }
        }
        
        // 定期检查角色
        CheckAndSwitchRole();
    }
}

// 判断是否应该让位给其他主节点
bool ShouldYieldToOtherPrimary(const MulticastHAMessage& msg) {
    // 比较优先级
    if (msg.priority > m_priority) {
        return true;  // 其他节点优先级更高
    }
    
    if (msg.priority == m_priority) {
        // 优先级相同，比较IP地址
        std::string otherNodeId(msg.nodeId);
        if (CompareNodeId(otherNodeId, m_localNodeId) < 0) {
            return true;  // 其他节点IP更小
        }
    }
    
    return false;
}

// 比较节点ID（IP地址转换为数值比较）
int CompareNodeId(const std::string& id1, const std::string& id2) {
    struct in_addr addr1, addr2;
    inet_pton(AF_INET, id1.c_str(), &addr1);
    inet_pton(AF_INET, id2.c_str(), &addr2);
    return ntohl(addr1.s_addr) - ntohl(addr2.s_addr);
}
```

#### 5.3.3 选举流程（2节点场景）

**场景1：节点启动时的初始选举**

```
时间线：
T0: 节点A启动，加入组播组，发送选举公告（priority=0, nodeId=192.168.1.10）
T1: 节点B启动，加入组播组，发送选举公告（priority=0, nodeId=192.168.1.20）
T2: 节点A收到节点B的选举公告
T2: 节点B收到节点A的选举公告
T3: 等待2秒收集期结束
T4: 节点A检查：
    - 未收到主节点心跳
    - 比较IP：192.168.1.10 < 192.168.1.20
    - 结论：节点A成为Primary
T4: 节点B检查：
    - 未收到主节点心跳
    - 比较IP：192.168.1.10 < 192.168.1.20
    - 结论：节点B成为Standby
T5: 节点A切换为Primary，开始发送心跳
T5: 节点B切换为Standby，开始监听心跳
```

**场景2：主节点故障后的切换**

```
时间线：
T0: 节点A（Primary）正常运行，每3秒发送心跳
T1: 节点A故障（崩溃或网络断开）
T9: 节点B检测到9秒未收到心跳，判定主节点故障
T10: 节点B切换为Primary，开始发送心跳
T11: 节点A恢复后，收到节点B的心跳，切换为Standby
```

#### 5.3.4 选举示例

**示例1：优先级不同**
```
节点A: priority=100, IP=192.168.1.10
节点B: priority=50,  IP=192.168.1.20
结果: 节点A成为Primary（优先级更高）
```

**示例2：优先级相同，比较IP**
```
节点A: priority=0, IP=192.168.1.10
节点B: priority=0, IP=192.168.1.20
结果: 节点A成为Primary（IP更小）
```

### 5.4 优势

- **无需配置对端IP**：通过组播自动发现另一个节点
- **简化设计**：针对2节点场景优化，实现简单
- **自动故障恢复**：主节点故障后，备节点自动切换为主节点
- **避免单点故障**：不依赖预先配置的节点信息
- **快速切换**：检测到故障后立即切换，无需复杂选举

## 6. 配置设计

### 6.1 配置文件

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
    "priority": 0,  // 优先级（0表示使用IP地址比较，数值越大优先级越高）
    "role": "auto"  // auto | primary | standby（auto表示通过组播协商）
  }
}
```

**注意**：
- 不再需要 `local_ip` 和 `peer_ip` 配置，通过组播自动发现另一个节点
- 不再需要 `sync` 配置项，因为主备节点都通过API获取数据
- `priority` 为0时，使用IP地址比较（IP小的优先级高）

### 6.2 角色协商（2节点场景）

**组播协商模式**（`"role": "auto"`）：
1. 两个节点加入同一个组播组
2. 节点启动时发送选举公告
3. 等待2秒收集另一个节点的响应
4. 根据以下规则确定主备角色：
   - 如果配置了 `priority > 0`：优先级高的为主节点
   - 如果 `priority == 0`：比较IP地址，IP小的为主节点
5. 如果收到主节点心跳，自动成为备节点；否则成为主节点

**手动指定模式**（`"role": "primary"` 或 `"role": "standby"`）：
- 强制指定角色，不参与选举
- 适用于测试或特殊场景

## 7. 实现要点

### 7.1 避免脑裂（Split-Brain）

**问题**：网络分区时，两个节点都认为自己是主节点

**2节点场景下的处理**：

1. **优先级机制**（主要方案）：
   - 配置明确的优先级，确保优先级高的节点总是成为主节点
   - 如果优先级相同，使用IP地址比较（保证确定性）
   - 当网络恢复时，优先级低的节点会自动降级为备节点

2. **心跳检测**：
   - 主节点定期发送心跳
   - 如果节点收到其他主节点的心跳，比较优先级决定是否降级
   - 优先级低的节点自动降级为备节点

3. **组播TTL限制**（推荐）：
   - 设置组播TTL=1，限制在同一网段内
   - 避免跨网段的网络分区问题

4. **网络恢复后的自动恢复**：
   - 当网络分区恢复时，两个节点都能收到对方的心跳
   - 通过优先级比较，自动确定主备角色
   - 优先级低的节点自动降级为备节点

### 7.2 数据一致性

- 主备节点都通过相同的API获取数据，保证数据来源一致
- 备节点切换为主节点时，数据已是最新（因为备节点也在运行数据采集服务）
- 数据采集服务使用线程安全的仓储，保证并发安全

### 7.4 性能优化

**角色检查开销**：
- `IsPrimary()` 方法使用 `std::atomic<Role>`，读取开销极小
- 角色检查只在关键操作前进行，不会频繁调用
- 备节点虽然接收请求，但快速返回，不执行耗时操作

**优化建议**：
- 可以在服务内部缓存角色状态，减少对HeartbeatService的调用
- 使用角色变更回调更新缓存，避免频繁检查
- 备节点可以提前过滤请求，减少不必要的处理

```cpp
// 优化示例：缓存角色状态
class ResourceMonitorBroadcaster {
private:
    std::atomic<bool> m_isPrimary;  // 缓存角色状态
    
    void OnRoleChanged(HeartbeatService::Role newRole) {
        m_isPrimary = (newRole == HeartbeatService::Role::Primary);
    }
    
    bool SendResourceMonitorResponse(uint32_t requestId) {
        if (!m_isPrimary.load()) {  // 使用缓存，避免调用IsPrimary()
            return false;
        }
        // ... 发送逻辑 ...
    }
};
```

### 7.3 服务启动顺序

```
系统启动时（主备节点相同）：
1. 创建并启动心跳服务
2. 等待角色协商完成
3. 创建并启动所有服务（注入心跳服务引用）
4. 各服务在运行时检查角色，决定是否执行操作

备节点切换为主节点时：
1. 心跳服务检测到主节点故障，触发重新选举
2. 角色从Standby切换为Primary
3. 各服务在下次操作时检查角色，发现是Primary，开始执行操作
4. 无需启动/停止服务，只需角色状态变化

优势：
- 服务启动顺序简单，主备节点启动流程相同
- 切换时无需启动/停止服务，切换更快
- 服务状态稳定，减少启动/停止带来的问题
```

## 8. 监控和日志

### 8.1 关键事件日志

- 角色切换事件（HeartbeatService）
- 心跳超时事件（HeartbeatService）
- 角色检查日志（各服务在跳过操作时记录）
- 数据采集成功/失败（主备节点都有）
- 服务启动/停止（各服务）

### 8.2 监控指标

- 当前角色（Primary/Standby）
- 心跳延迟
- 最后心跳时间
- 切换次数统计
- 数据采集状态（主备节点都有）

## 9. 部署建议

### 9.1 网络配置

- 主备节点在同一网段（组播TTL=1，限制在同一网段）
- 配置组播地址（建议使用224.100.200.16，避免与其他服务冲突）
- 配置防火墙规则，允许组播心跳端口（UDP 9999）
- 主备节点都需要能够访问外部API（用于数据采集）
- 确保网络支持组播（IGMP协议）

### 9.2 资源要求

- 备节点需要与主节点相同的计算资源
- 网络带宽：主备节点都需要访问外部API，无需主备之间的同步流量
- 存储：无需额外的状态同步存储

## 10. 测试场景

1. **正常切换**：手动停止主节点，验证备节点自动切换为主节点
2. **组播协商**：两个节点同时启动，验证自动协商出主备角色
3. **网络分区**：模拟网络中断，验证不会出现脑裂（2节点场景下风险较低）
4. **数据完整性**：切换后验证备节点数据已是最新（通过API获取）
5. **性能测试**：验证切换时间（目标：<5秒）
6. **恢复测试**：原主节点恢复后自动降级为备节点
7. **数据采集测试**：验证备节点数据采集服务正常运行，数据保持最新
8. **组播消息测试**：验证两个节点都能收到组播消息
9. **优先级测试**：配置不同优先级，验证优先级高的成为主节点
10. **IP地址比较测试**：优先级相同时，验证IP地址小的成为主节点

