# 单元测试用例设计文档

本文档定义了资源管理系统(zygl3)六个核心单元的测试用例设计。

## 测试框架建议

建议使用 **Google Test (gtest)** 或 **Catch2** 作为测试框架：
- **Google Test**: 功能完整，文档丰富，广泛使用
- **Catch2**: 轻量级，单头文件，易于集成

---

## 单元1: 资源采集单元

### 测试目标
验证板卡信息采集、心跳发送、离线检测功能的正确性。

### 测试用例

#### TC-1.1: 板卡信息采集成功
**前置条件**: 
- API客户端可用，返回有效的JSON数据

**测试步骤**:
1. 创建DataCollectorService实例
2. 调用CollectBoardInfo()
3. 验证API客户端被调用
4. 验证数据被转换为领域对象
5. 验证仓储被更新

**预期结果**:
- QywApiClient::GetBoardInfo()被调用一次
- 返回的板卡信息被正确解析
- 仓储中对应的机箱和板卡数据被更新

**断言检查**:
```cpp
ASSERT_TRUE(apiClient->GetBoardInfoCalled());
ASSERT_EQ(1, chassisRepo->Size());
ASSERT_NOT_NULL(chassisRepo->FindByNumber(1));
```

---

#### TC-1.2: 板卡信息采集失败处理
**前置条件**:
- API客户端返回错误或网络异常

**测试步骤**:
1. 模拟API调用失败
2. 调用CollectBoardInfo()
3. 验证异常被捕获
4. 验证系统不崩溃

**预期结果**:
- 异常被正确捕获和处理
- 不影响后续采集循环
- 记录错误日志

---

#### TC-1.3: 心跳发送成功
**前置条件**:
- API客户端可用

**测试步骤**:
1. 创建DataCollectorService实例
2. 调用SendHeartbeat()
3. 验证心跳API被调用

**预期结果**:
- QywApiClient::SendHeartbeat()被调用
- 传递的IP地址正确

**断言检查**:
```cpp
ASSERT_TRUE(apiClient->SendHeartbeatCalled());
ASSERT_EQ("192.168.6.222", apiClient->GetLastHeartbeatIp());
```

---

#### TC-1.4: 板卡离线检测
**前置条件**:
- 仓储中有板卡数据
- 板卡最后更新时间超过阈值

**测试步骤**:
1. 创建板卡，设置最后更新时间为2分钟前（超过120秒阈值）
2. 调用CheckAndMarkOfflineBoards(120)
3. 验证板卡状态被标记为离线

**预期结果**:
- 超时的板卡状态变为Offline
- 板卡的任务列表被清空

**断言检查**:
```cpp
auto board = chassis->GetBoardBySlot(1);
ASSERT_EQ(BoardOperationalStatus::Offline, board->GetStatus());
ASSERT_TRUE(board->GetTasks().empty());
```

---

#### TC-1.5: 采集循环执行
**前置条件**:
- 采集服务已启动

**测试步骤**:
1. 启动DataCollectorService
2. 等待15秒（超过一个采集间隔）
3. 验证采集方法被调用多次

**预期结果**:
- CollectBoardInfo()和CollectStackInfo()至少被调用一次
- SendHeartbeat()至少被调用一次

---

## 单元2: 资源数据存取单元

### 测试目标
验证机箱和板卡数据的存储、查询、更新功能。

### 测试用例

#### TC-2.1: 机箱保存和查询
**前置条件**: 
- 空仓储

**测试步骤**:
1. 创建Chassis对象（机箱号=1）
2. 调用Save(chassis)
3. 调用FindByNumber(1)
4. 验证查询结果

**预期结果**:
- Save成功
- FindByNumber返回正确的机箱对象
- Size()返回1

**断言检查**:
```cpp
repo->Save(chassis);
auto found = repo->FindByNumber(1);
ASSERT_NOT_NULL(found);
ASSERT_EQ(1, found->GetChassisNumber());
ASSERT_EQ(1, repo->Size());
```

---

#### TC-2.2: 板卡更新
**前置条件**:
- 仓储中有机箱1，槽位1有板卡

**测试步骤**:
1. 创建新的Board对象（状态=Abnormal）
2. 调用UpdateBoard(1, 1, board)
3. 查询板卡验证更新

**预期结果**:
- 板卡状态被更新为Abnormal
- 其他属性正确保存

**断言检查**:
```cpp
auto chassis = repo->FindByNumber(1);
auto* board = chassis->GetBoardBySlot(1);
ASSERT_EQ(BoardOperationalStatus::Abnormal, board->GetStatus());
```

---

#### TC-2.3: 并发访问安全
**前置条件**:
- 仓储中有数据

**测试步骤**:
1. 启动多个线程同时进行Save和FindByNumber操作
2. 运行一段时间
3. 验证数据一致性

**预期结果**:
- 无数据竞争
- 所有操作成功
- 数据完整性保持

**断言检查**:
```cpp
// 启动10个线程，每个线程执行100次操作
std::vector<std::thread> threads;
for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&]() {
        for (int j = 0; j < 100; ++j) {
            repo->Save(chassis);
            auto found = repo->FindByNumber(1);
            ASSERT_NOT_NULL(found);
        }
    });
}
// 等待所有线程完成
for (auto& t : threads) t.join();
ASSERT_EQ(1, repo->Size()); // 确保只有1个机箱
```

---

#### TC-2.4: 获取所有机箱
**前置条件**:
- 仓储中有多个机箱

**测试步骤**:
1. 保存9个机箱（机箱号1-9）
2. 调用GetAll()
3. 验证返回结果

**预期结果**:
- 返回9个机箱
- 每个机箱的机箱号正确

**断言检查**:
```cpp
auto allChassis = repo->GetAll();
ASSERT_EQ(9, allChassis.size());
for (int i = 1; i <= 9; ++i) {
    ASSERT_EQ(i, allChassis[i-1]->GetChassisNumber());
}
```

---

#### TC-2.5: 查询不存在的机箱
**前置条件**:
- 空仓储

**测试步骤**:
1. 调用FindByNumber(999)

**预期结果**:
- 返回nullptr

**断言检查**:
```cpp
auto found = repo->FindByNumber(999);
ASSERT_NULL(found);
```

---

## 单元3: 业务采集单元

### 测试目标
验证业务链路信息采集的正确性。

### 测试用例

#### TC-3.1: 业务链路信息采集成功
**前置条件**:
- API客户端可用，返回有效的业务链路JSON

**测试步骤**:
1. 创建DataCollectorService实例
2. 调用CollectStackInfo()
3. 验证API被调用
4. 验证数据转换正确

**预期结果**:
- QywApiClient::GetStackInfo()被调用
- 返回的业务链路信息被正确解析
- 仓储被更新

**断言检查**:
```cpp
ASSERT_TRUE(apiClient->GetStackInfoCalled());
ASSERT_GT(stackRepo->Size(), 0);
```

---

#### TC-3.2: 业务链路数据解析
**前置条件**:
- API返回包含Stack、Service、Task的JSON

**测试步骤**:
1. 模拟API返回复杂业务链路数据
2. 调用CollectStackInfo()
3. 验证数据转换的完整性

**预期结果**:
- Stack对象创建正确（名称、UUID、状态）
- Service对象创建正确（包含在Stack中）
- Task对象创建正确（包含在Service中）
- ResourceUsage数据正确映射

**断言检查**:
```cpp
auto stacks = stackRepo->GetAll();
ASSERT_GT(stacks.size(), 0);
auto stack = stacks[0];
ASSERT_FALSE(stack->GetStackUUID().empty());
auto services = stack->GetAllServices();
ASSERT_GT(services.size(), 0);
```

---

#### TC-3.3: 业务链路采集失败处理
**前置条件**:
- API调用失败

**测试步骤**:
1. 模拟API异常
2. 调用CollectStackInfo()
3. 验证异常处理

**预期结果**:
- 异常被捕获
- 系统继续运行
- 记录错误日志

---

#### TC-3.4: 空业务链路处理
**前置条件**:
- API返回空业务链路列表

**测试步骤**:
1. 模拟返回空列表
2. 调用CollectStackInfo()
3. 验证仓储被清空

**预期结果**:
- 仓储被清空（调用Clear）
- Size()返回0

**断言检查**:
```cpp
ASSERT_EQ(0, stackRepo->Size());
```

---

## 单元4: 业务数据存取单元

### 测试目标
验证业务链路数据的存储、查询功能。

### 测试用例

#### TC-4.1: 业务链路保存和查询
**前置条件**:
- 空仓储

**测试步骤**:
1. 创建Stack对象（UUID="test-uuid"）
2. 调用Save(stack)
3. 调用FindByUUID("test-uuid")
4. 验证查询结果

**预期结果**:
- Save成功
- FindByUUID返回正确的Stack对象

**断言检查**:
```cpp
repo->Save(stack);
auto found = repo->FindByUUID("test-uuid");
ASSERT_NOT_NULL(found);
ASSERT_EQ("test-uuid", found->GetStackUUID());
```

---

#### TC-4.2: 业务链路更新
**前置条件**:
- 仓储中有业务链路

**测试步骤**:
1. 保存Stack（状态=已部署）
2. 修改Stack状态（状态=未部署）
3. 再次Save
4. 查询验证状态更新

**预期结果**:
- 状态被正确更新

**断言检查**:
```cpp
stack->UpdateDeployStatus(1);
repo->Save(stack);
auto found = repo->FindByUUID("test-uuid");
ASSERT_EQ(1, found->GetDeployStatus());
```

---

#### TC-4.3: 业务链路清空
**前置条件**:
- 仓储中有多个业务链路

**测试步骤**:
1. 保存多个Stack
2. 调用Clear()
3. 验证仓储为空

**预期结果**:
- Size()返回0
- GetAll()返回空列表

**断言检查**:
```cpp
repo->Clear();
ASSERT_EQ(0, repo->Size());
ASSERT_TRUE(repo->GetAll().empty());
```

---

#### TC-4.4: 服务添加和查询
**前置条件**:
- Stack对象已创建

**测试步骤**:
1. 创建Service对象
2. 调用stack->AddOrUpdateService(service)
3. 调用stack->GetAllServices()
4. 验证服务被添加

**预期结果**:
- 服务被添加到Stack中
- 可以通过UUID查询到服务

**断言检查**:
```cpp
stack->AddOrUpdateService(service);
auto services = stack->GetAllServices();
ASSERT_GT(services.size(), 0);
ASSERT_TRUE(services.count("service-uuid") > 0);
```

---

#### TC-4.5: 任务添加和资源更新
**前置条件**:
- Service对象已创建

**测试步骤**:
1. 创建Task对象
2. 设置ResourceUsage
3. 调用service->AddOrUpdateTask(taskId, task)
4. 查询任务验证资源数据

**预期结果**:
- 任务被添加
- 资源使用情况正确保存

**断言检查**:
```cpp
ResourceUsage resources;
resources.cpuUsage = 50.0f;
resources.memoryUsage = 60.0f;
task.UpdateResources(resources);
service.AddOrUpdateTask("task-1", task);
auto tasks = service.GetAllTasks();
ASSERT_TRUE(tasks.count("task-1") > 0);
auto& savedTask = tasks.at("task-1");
ASSERT_FLOAT_EQ(50.0f, savedTask.GetResources().cpuUsage);
```

---

## 单元5: 前端组播交互单元

### 测试目标
验证UDP组播通信基础设施的正确性。

### 测试用例

#### TC-5.1: UDP Socket创建和配置
**前置条件**:
- 系统支持UDP socket

**测试步骤**:
1. 创建ResourceMonitorBroadcaster实例
2. 验证socket被创建
3. 验证广播选项被设置

**预期结果**:
- Socket创建成功（fd >= 0）
- SO_BROADCAST选项被启用

**断言检查**:
```cpp
auto broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
    chassisRepo, stackRepo, apiClient, "234.186.1.99", 0x100A);
// 通过访问内部socket或添加getter方法验证
// 注意：需要为测试暴露socket或添加测试接口
```

---

#### TC-5.2: 启动和停止服务
**前置条件**:
- Broadcaster实例已创建

**测试步骤**:
1. 调用Start()
2. 验证运行状态
3. 调用Stop()
4. 验证停止状态

**预期结果**:
- Start()后IsRunning()或内部标志为true
- Stop()后运行标志为false

**断言检查**:
```cpp
broadcaster->Start();
// 需要添加IsRunning()方法或通过其他方式验证
broadcaster->Stop();
```

---

#### TC-5.3: 命令码配置
**前置条件**:
- Broadcaster实例已创建

**测试步骤**:
1. 调用SetCommand()设置命令码
2. 发送响应
3. 验证使用正确的命令码

**预期结果**:
- 命令码被正确设置
- 发送的报文中命令码正确

---

#### TC-5.4: 组播地址配置
**前置条件**:
- Broadcaster实例已创建

**测试步骤**:
1. 使用自定义组播地址创建
2. 发送数据
3. 验证使用正确的组播地址

**预期结果**:
- 组播地址被正确配置
- 数据发送到指定地址

---

## 单元6: 命令处理和数据组合单元

### 测试目标
验证命令处理、数据组合、格式转换的正确性。

### 测试用例

#### TC-6.1: UDP资源监控响应数据组合
**前置条件**:
- 仓储中有机箱和板卡数据
- Broadcaster已创建

**测试步骤**:
1. 准备测试数据（9个机箱，每个12块板卡）
2. 调用BuildResponseData()
3. 验证响应数据结构

**预期结果**:
- boardStatus数组长度为108（9×12）
- taskStatus数组长度为864（9×12×8）
- 数据映射正确

**断言检查**:
```cpp
ResourceMonitorResponse response;
broadcaster->BuildResponseData(response);
// 验证数组长度和内容
ASSERT_EQ(108, sizeof(response.boardStatus));
ASSERT_EQ(864, sizeof(response.taskStatus));
```

---

#### TC-6.2: 板卡状态映射
**前置条件**:
- 仓储中有板卡（状态=Normal）

**测试步骤**:
1. 调用MapBoardStatusToArray()
2. 验证映射结果

**预期结果**:
- Normal状态映射为1
- Abnormal/Offline状态映射为0

**断言检查**:
```cpp
uint8_t array[12] = {0};
auto chassis = repo->FindByNumber(1);
// 设置板卡状态为Normal
chassis->GetBoardBySlot(1)->UpdateFromApiData(...);
broadcaster->MapBoardStatusToArray(array, 1);
ASSERT_EQ(1, array[0]); // 第一块板卡为Normal
```

---

#### TC-6.3: 任务状态映射
**前置条件**:
- 仓储中有板卡和任务数据

**测试步骤**:
1. 准备任务数据（状态=运行中）
2. 调用MapTaskStatusToArray()
3. 验证映射结果

**预期结果**:
- 运行中任务（status=1）映射为1
- 其他状态映射为0

**断言检查**:
```cpp
uint8_t array[96] = {0}; // 12板卡×8任务
// 设置任务状态为运行中
broadcaster->MapTaskStatusToArray(array, 1);
// 验证第一个任务的状态
```

---

#### TC-6.4: 任务启动请求处理
**前置条件**:
- Broadcaster已创建
- API客户端可用

**测试步骤**:
1. 创建TaskStartRequest（工作模式=标签1）
2. 调用HandleTaskStartRequest()
3. 验证API被调用
4. 验证响应被发送

**预期结果**:
- WorkModeToLabel()转换正确
- DeployStacks()被调用
- 响应报文被组合和发送

**断言检查**:
```cpp
TaskStartRequest request;
request.workMode = 1; // 假设1对应"label1"
bool result = broadcaster->HandleTaskStartRequest(request);
ASSERT_TRUE(result);
ASSERT_TRUE(apiClient->DeployStacksCalled());
```

---

#### TC-6.5: HTTP告警处理-板卡异常
**前置条件**:
- AlertReceiverServer已创建

**测试步骤**:
1. 构造板卡异常上报JSON
2. 调用HandleBoardAlert()
3. 验证数据解析
4. 验证UDP故障上报被发送
5. 验证HTTP响应

**预期结果**:
- JSON被正确解析
- BoardAlertRequest结构填充正确
- 调用SendFaultReport()
- HTTP响应code=0, message="success"

**断言检查**:
```cpp
httplib::Request req;
req.body = R"({"chassisNumber":1,"boardNumber":1,...})";
httplib::Response res;
alertServer->HandleBoardAlert(req, res);
ASSERT_EQ(200, res.status);
json response = json::parse(res.body);
ASSERT_EQ(0, response["code"]);
ASSERT_TRUE(broadcaster->SendFaultReportCalled());
```

---

#### TC-6.6: HTTP告警处理-组件异常
**前置条件**:
- AlertReceiverServer已创建

**测试步骤**:
1. 构造组件异常上报JSON（包含taskAlertInfos）
2. 调用HandleServiceAlert()
3. 验证数据解析
4. 验证响应

**预期结果**:
- ServiceAlertRequest结构填充正确
- taskAlertInfos数组解析正确
- 发送故障上报

---

#### TC-6.7: API控制命令-部署业务链路
**前置条件**:
- API客户端可用

**测试步骤**:
1. 调用DeployStacks({"label1", "label2"}, "admin", "password", 0)
2. 验证请求JSON格式
3. 验证API被调用
4. 验证响应解析

**预期结果**:
- 请求JSON包含stackLabels、account、password、stop字段
- API端点正确
- DeployResponse解析正确（包含success和failure列表）

**断言检查**:
```cpp
auto response = apiClient->DeployStacks({"label1"}, "admin", "pwd", 0);
ASSERT_FALSE(response.successStackInfos.empty() || 
             response.failureStackInfos.empty());
```

---

#### TC-6.8: IP地址格式转换
**前置条件**:
- Broadcaster已创建

**测试步骤**:
1. 调用IpStringToUint32("192.168.1.1")
2. 验证转换结果

**预期结果**:
- IP字符串正确转换为uint32
- 字节序正确（网络字节序）

**断言检查**:
```cpp
uint32_t ip = broadcaster->IpStringToUint32("192.168.1.1");
// 验证转换结果
ASSERT_EQ(0xC0A80101, ntohl(ip)); // 注意字节序
```

---

#### TC-6.9: 工作模式到标签转换
**前置条件**:
- Broadcaster已创建

**测试步骤**:
1. 调用WorkModeToLabel(workMode)
2. 验证转换结果

**预期结果**:
- 工作模式正确映射为标签名

---

#### TC-6.10: 任务查询响应构建
**前置条件**:
- 仓储中有任务数据

**测试步骤**:
1. 创建TaskQueryRequest（机箱1，板卡1，任务索引0）
2. 调用BuildTaskQueryResponse()
3. 验证响应数据

**预期结果**:
- 任务ID正确
- 任务状态正确
- CPU使用率、内存使用率正确映射

**断言检查**:
```cpp
TaskQueryRequest request;
request.chassisNumber = 1;
request.boardNumber = 1;
request.taskIndex = 0;
TaskQueryResponse response;
broadcaster->BuildTaskQueryResponse(response, request);
ASSERT_FALSE(response.taskId == 0);
ASSERT_LT(response.cpuUsage, 1001); // 千分比
```

---

## 集成测试用例

### IT-1: 端到端数据采集流程
**测试步骤**:
1. 启动完整系统
2. 模拟API返回数据
3. 等待一个采集周期
4. 通过UDP查询数据
5. 验证数据一致性

**预期结果**:
- 数据从API采集 → 存储 → UDP响应流程完整

---

### IT-2: 命令处理完整流程
**测试步骤**:
1. 接收UDP任务启动请求
2. 验证请求被解析
3. 验证API部署命令被调用
4. 验证UDP响应被发送

**预期结果**:
- 命令处理流程完整，数据流转正确

---

## 测试覆盖率目标

| 单元 | 代码覆盖率目标 | 重点覆盖 |
|------|---------------|---------|
| 单元1 | ≥80% | API调用、异常处理、离线检测 |
| 单元2 | ≥90% | CRUD操作、并发安全 |
| 单元3 | ≥80% | 数据解析、转换逻辑 |
| 单元4 | ≥90% | CRUD操作、数据关联 |
| 单元5 | ≥70% | Socket操作、配置管理 |
| 单元6 | ≥85% | 数据组合、格式转换、命令处理 |

---

## 测试框架集成

### Google Test示例
```cpp
#include <gtest/gtest.h>

class Unit1Test : public ::testing::Test {
protected:
    void SetUp() override {
        // 测试前置设置
        chassisRepo = std::make_shared<InMemoryChassisRepository>();
        stackRepo = std::make_shared<InMemoryStackRepository>();
        // 创建Mock API Client
        apiClient = std::make_shared<MockQywApiClient>();
    }
    
    void TearDown() override {
        // 测试后清理
    }
    
    std::shared_ptr<IChassisRepository> chassisRepo;
    std::shared_ptr<IStackRepository> stackRepo;
    std::shared_ptr<MockQywApiClient> apiClient;
};

TEST_F(Unit1Test, TC_1_1_CollectBoardInfoSuccess) {
    // 实现TC-1.1
}
```

### Catch2示例
```cpp
#include <catch2/catch.hpp>

TEST_CASE("Unit1: CollectBoardInfoSuccess", "[unit1]") {
    // 实现TC-1.1
}
```

---

## Mock对象设计

### MockQywApiClient
```cpp
class MockQywApiClient : public QywApiClient {
public:
    MOCK_METHOD(std::vector<BoardInfoResponse>, GetBoardInfo, ());
    MOCK_METHOD(std::vector<StackInfoResponse>, GetStackInfo, ());
    MOCK_METHOD(bool, SendHeartbeat, (const std::string&));
    MOCK_METHOD(DeployResponse, DeployStacks, (...));
    
    // 测试辅助方法
    void SetMockBoardInfo(const std::vector<BoardInfoResponse>& data);
    void SetShouldFail(bool fail);
};
```

---

## 测试数据准备

### 测试数据生成器
建议创建测试辅助类：

```cpp
class TestDataGenerator {
public:
    static std::shared_ptr<Chassis> CreateTestChassis(int chassisNumber);
    static BoardInfoResponse CreateTestBoardInfo(int chassisNum, int boardNum);
    static StackInfoResponse CreateTestStackInfo(const std::string& uuid);
    static std::string CreateTestJson(const std::string& type);
};
```

---

## 性能测试用例

### PT-1: 并发采集性能
- 验证多线程采集不会导致数据竞争
- 验证采集性能满足要求（10秒间隔）

### PT-2: 大数据量存储
- 验证9个机箱×14块板卡的数据存储性能
- 验证查询响应时间

### PT-3: UDP响应性能
- 验证UDP响应构建和发送性能
- 验证并发请求处理能力

---

**文档版本**: v1.0  
**最后更新**: 2024

