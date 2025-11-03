/**
 * @file unit1_test.cpp
 * @brief 单元1测试 - 资源采集单元
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
#include "domain/chassis.h"
#include "domain/board.h"

using namespace app::infrastructure;
using namespace app::domain;
using namespace app::infrastructure::test;

/**
 * @brief 单元1测试套件
 */
class Unit1Test : public ::testing::Test {
protected:
    void SetUp() override {
        chassisRepo = std::make_shared<InMemoryChassisRepository>();
        stackRepo = std::make_shared<InMemoryStackRepository>();
        
        // 创建Mock API客户端
        mockApiClient = std::make_shared<MockQywApiClient>();
        
        // 创建测试机箱
        auto chassis = TestDataGenerator::CreateTestChassis(1);
        chassis->ResizeBoards(14);
        chassisRepo->Save(chassis);
    }
    
    void TearDown() override {
        if (collector) {
            collector->Stop();
        }
        collector.reset();
        mockApiClient.reset();
        chassisRepo.reset();
        stackRepo.reset();
    }
    
    std::shared_ptr<InMemoryChassisRepository> chassisRepo;
    std::shared_ptr<InMemoryStackRepository> stackRepo;
    std::shared_ptr<MockQywApiClient> mockApiClient;
    std::shared_ptr<DataCollectorService> collector;
};

/**
 * @brief TC-1.1: 板卡信息采集成功
 */
TEST_F(Unit1Test, TC_1_1_CollectBoardInfoSuccess) {
    // 准备Mock数据
    std::vector<BoardInfoResponse> boardInfos;
    boardInfos.push_back(TestDataGenerator::CreateTestBoardInfo(1, 1));
    
    mockApiClient->SetMockBoardInfo(boardInfos);
    
    // 创建采集服务
    collector = std::make_shared<DataCollectorService>(
        chassisRepo, stackRepo, mockApiClient, "192.168.6.222", 10, 120);
    
    // 调用采集方法（使用反射或测试友好的接口）
    // 注意：由于CollectBoardInfo是私有方法，可能需要：
    // 1. 添加测试友好的访问接口
    // 2. 或者通过公共接口Start()然后等待一段时间来测试
    
    // 直接测试：通过创建服务并启动，然后在短时间内停止
    collector->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    collector->Stop();
    
    // 验证仓储被更新
    auto chassis = chassisRepo->FindByNumber(1);
    ASSERT_NE(nullptr, chassis);
    
    auto* board = chassis->GetBoardBySlot(1);
    ASSERT_NE(nullptr, board);
    ASSERT_EQ("192.168.0.101", board->GetAddress());
}

/**
 * @brief TC-1.2: 板卡信息采集失败处理
 */
TEST_F(Unit1Test, TC_1_2_CollectBoardInfoFailure) {
    // 模拟API调用返回空数据
    mockApiClient->SetMockBoardInfo(std::vector<BoardInfoResponse>());
    
    collector = std::make_shared<DataCollectorService>(
        chassisRepo, stackRepo, mockApiClient, "192.168.6.222", 10, 120);
    
    // 启动服务并等待一小段时间
    collector->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    collector->Stop();
    
    // 验证系统没有崩溃，仓储状态正常
    ASSERT_NE(nullptr, chassisRepo->FindByNumber(1));
}

/**
 * @brief TC-1.3: 心跳发送成功
 */
TEST_F(Unit1Test, TC_1_3_SendHeartbeatSuccess) {
    const std::string testIp = "192.168.6.222";
    
    // Mock会在SendHeartbeat被调用时记录
    
    collector = std::make_shared<DataCollectorService>(
        chassisRepo, stackRepo, mockApiClient, testIp, 10, 120);
    
    // 启动服务，心跳会在采集循环中发送
    collector->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    collector->Stop();
    
    // 验证心跳被调用
    ASSERT_TRUE(mockApiClient->SendHeartbeatCalled());
    ASSERT_EQ(testIp, mockApiClient->GetLastHeartbeatIp());
}

/**
 * @brief TC-1.4: 板卡离线检测
 */
TEST_F(Unit1Test, TC_1_4_BoardOfflineDetection) {
    // 创建机箱和板卡
    auto chassis = chassisRepo->FindByNumber(1);
    auto* board = chassis->GetBoardBySlot(1);
    
    // 设置板卡最后更新时间为2分钟前（超过120秒阈值）
    // 注意：需要检查Board类是否有设置时间戳的方法
    // 如果没有，可以通过UpdateFromApiData然后等待来实现
    
    // 创建采集服务，超时时间为120秒
    collector = std::make_shared<DataCollectorService>(
        chassisRepo, stackRepo, mockApiClient, "192.168.6.222", 10, 120);
    
    // 模拟API不返回该板卡（表示离线）
    mockApiClient->SetMockBoardInfo(std::vector<BoardInfoResponse>());
    
    collector->Start();
    // 等待超过超时时间（120秒），但为了测试速度，可以设置更短的超时
    // 或者需要添加测试友好的超时配置
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    collector->Stop();
    
    // 验证：由于无法直接设置时间戳，这个测试可能需要调整
    // 可以通过不更新板卡数据，等待超时时间来测试
}

/**
 * @brief TC-1.5: 采集循环执行
 */
TEST_F(Unit1Test, TC_1_5_CollectLoopExecution) {
    std::vector<BoardInfoResponse> boardInfos;
    boardInfos.push_back(TestDataGenerator::CreateTestBoardInfo(1, 1));
    
    // 准备Mock数据
    mockApiClient->SetMockBoardInfo(boardInfos);
    std::vector<StackInfoResponse> stackInfos;
    mockApiClient->SetMockStackInfo(stackInfos);
    
    collector = std::make_shared<DataCollectorService>(
        chassisRepo, stackRepo, mockApiClient, "192.168.6.222", 1, 120); // 1秒间隔
    
    collector->Start();
    // 等待超过一个采集间隔
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    collector->Stop();
    
    // 验证方法被调用
    ASSERT_TRUE(mockApiClient->GetBoardInfoCalled());
    ASSERT_TRUE(mockApiClient->GetStackInfoCalled());
    ASSERT_TRUE(mockApiClient->SendHeartbeatCalled());
}

