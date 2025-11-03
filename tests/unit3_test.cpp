/**
 * @file unit3_test.cpp
 * @brief 单元3测试 - 业务采集单元
 */

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include "infrastructure/collectors/data_collector_service.h"
#include "infrastructure/persistence/in_memory_chassis_repository.h"
#include "infrastructure/persistence/in_memory_stack_repository.h"
#include "mocks/mock_api_client.h"
#include "utils/test_data_generator.h"

using namespace app::infrastructure;
using namespace app::domain;
using namespace app::infrastructure::test;

/**
 * @brief 单元3测试套件
 */
class Unit3Test : public ::testing::Test {
protected:
    void SetUp() override {
        chassisRepo = std::make_shared<InMemoryChassisRepository>();
        stackRepo = std::make_shared<InMemoryStackRepository>();
        mockApiClient = std::make_shared<MockQywApiClient>();
    }
    
    void TearDown() override {
        if (collector) {
            collector->Stop();
        }
        collector.reset();
        mockApiClient.reset();
        stackRepo.reset();
        chassisRepo.reset();
    }
    
    std::shared_ptr<InMemoryChassisRepository> chassisRepo;
    std::shared_ptr<InMemoryStackRepository> stackRepo;
    std::shared_ptr<MockQywApiClient> mockApiClient;
    std::shared_ptr<DataCollectorService> collector;
};

/**
 * @brief TC-3.1: 业务链路信息采集成功
 */
TEST_F(Unit3Test, TC_3_1_CollectStackInfoSuccess) {
    // 准备Mock数据
    std::vector<StackInfoResponse> stackInfos;
    stackInfos.push_back(TestDataGenerator::CreateTestStackInfo("stack-uuid-1", "TestStack"));
    
    mockApiClient->SetMockStackInfo(stackInfos);
    
    collector = std::make_shared<DataCollectorService>(
        chassisRepo, stackRepo, mockApiClient, "192.168.6.222", 10, 120);
    
    // 启动采集服务
    collector->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    collector->Stop();
    
    // 验证仓储被更新
    auto stack = stackRepo->FindByUUID("stack-uuid-1");
    ASSERT_NE(nullptr, stack);
    ASSERT_EQ("TestStack", stack->GetStackName());
    ASSERT_EQ("stack-uuid-1", stack->GetStackUUID());
}

/**
 * @brief TC-3.2: 业务链路数据解析
 */
TEST_F(Unit3Test, TC_3_2_StackDataParsing) {
    // 创建复杂的业务链路数据
    std::vector<StackInfoResponse> stackInfos;
    auto stackInfo = TestDataGenerator::CreateTestStackInfo("stack-uuid-1", "ComplexStack");
    
    // 添加多个服务
    ServiceInfo service2;
    service2.serviceUUID = "service-uuid-2";
    service2.serviceName = "Service2";
    service2.serviceStatus = 3; // 运行异常
    service2.serviceType = 1; // 公共组件
    stackInfo.serviceInfos.push_back(service2);
    
    stackInfos.push_back(stackInfo);
    
    mockApiClient->SetMockStackInfo(stackInfos);
    
    collector = std::make_shared<DataCollectorService>(
        chassisRepo, stackRepo, mockApiClient, "192.168.6.222", 10, 120);
    
    collector->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    collector->Stop();
    
    // 验证数据转换的完整性
    auto stack = stackRepo->FindByUUID("stack-uuid-1");
    ASSERT_NE(nullptr, stack);
    
    // 验证标签
    auto labels = stack->GetLabels();
    ASSERT_FALSE(labels.empty());
    ASSERT_EQ("label-uuid-1", labels[0].stackLabelUUID);
    
    // 验证服务
    auto services = stack->GetAllServices();
    ASSERT_GE(services.size(), 1);
    
    // 验证任务资源数据
    auto servicesMap = stack->GetAllServices();
    for (const auto& pair : servicesMap) {
        const auto& service = pair.second;
        auto tasks = service.GetAllTasks();
        if (!tasks.empty()) {
            const auto& task = tasks.begin()->second;
            ASSERT_GT(task.GetResources().cpuUsage, 0);
            ASSERT_GT(task.GetResources().memoryUsage, 0);
        }
    }
}

/**
 * @brief TC-3.3: 业务链路采集失败处理
 */
TEST_F(Unit3Test, TC_3_3_CollectStackInfoFailure) {
    // 模拟API调用失败
    mockApiClient->SetMockStackInfo(std::vector<StackInfoResponse>());
    
    collector = std::make_shared<DataCollectorService>(
        chassisRepo, stackRepo, mockApiClient, "192.168.6.222", 10, 120);
    
    collector->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    collector->Stop();
    
    // 验证系统继续运行，仓储被清空
    ASSERT_EQ(0, stackRepo->Size());
}

/**
 * @brief TC-3.4: 空业务链路处理
 */
TEST_F(Unit3Test, TC_3_4_EmptyStackList) {
    // 先添加一些数据
    auto stack = std::make_shared<Stack>("uuid-1", "TestStack");
    stackRepo->Save(stack);
    ASSERT_EQ(1, stackRepo->Size());
    
    // 模拟API返回空列表
    mockApiClient->SetMockStackInfo(std::vector<StackInfoResponse>());
    
    collector = std::make_shared<DataCollectorService>(
        chassisRepo, stackRepo, mockApiClient, "192.168.6.222", 10, 120);
    
    collector->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    collector->Stop();
    
    // 验证仓储被清空
    ASSERT_EQ(0, stackRepo->Size());
}

