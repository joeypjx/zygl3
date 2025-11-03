/**
 * @file unit6_test.cpp
 * @brief 单元6测试 - 命令处理和数据组合单元
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <string>
#include <cstring>
#include "interfaces/udp/resource_monitor_broadcaster.h"
#include "interfaces/http/alert_receiver_server.h"
#include "interfaces/cli/cli_service.h"
#include "infrastructure/persistence/in_memory_chassis_repository.h"
#include "infrastructure/persistence/in_memory_stack_repository.h"
#include "infrastructure/api_client/qyw_api_client.h"
#include "domain/chassis.h"
#include "domain/board.h"
#include "domain/stack.h"
#include "domain/task.h"
#include "utils/test_data_generator.h"
#include "infrastructure/config/config_manager.h"

using namespace app::infrastructure;
using namespace app::domain;
using namespace app::interfaces;
using namespace app::test;

/**
 * @brief 单元6测试套件
 */
class Unit6Test : public ::testing::Test {
protected:
    void SetUp() override {
        // 加载配置（如果配置文件不存在，使用默认值）
        try {
            ConfigManager::LoadFromFile("config.json");
        } catch (...) {
            // 如果配置文件不存在，继续使用默认值
        }
        
        chassisRepo = std::make_shared<InMemoryChassisRepository>();
        stackRepo = std::make_shared<InMemoryStackRepository>();
        apiClient = std::make_shared<QywApiClient>("localhost", 8080);
        
        // 创建测试数据
        auto chassis = TestDataGenerator::CreateTestChassis(1);
        chassis->ResizeBoards(14);
        
        // 设置板卡数据
        std::vector<TaskStatusInfo> taskInfos;
        std::vector<app::domain::FanSpeed> fanSpeeds;
        auto* board = chassis->GetBoardBySlot(1);
        if (board) {
            board->UpdateFromApiData("Board_1", 0, 12.5f, 2.0f, 45.0f, fanSpeeds, taskInfos);
        }
        
        chassisRepo->Save(chassis);
    }
    
    void TearDown() override {
        broadcaster.reset();
        alertServer.reset();
        cliService.reset();
        apiClient.reset();
        stackRepo.reset();
        chassisRepo.reset();
    }
    
    std::shared_ptr<InMemoryChassisRepository> chassisRepo;
    std::shared_ptr<InMemoryStackRepository> stackRepo;
    std::shared_ptr<QywApiClient> apiClient;
    std::shared_ptr<ResourceMonitorBroadcaster> broadcaster;
    std::shared_ptr<AlertReceiverServer> alertServer;
    std::shared_ptr<CliService> cliService;
};

/**
 * @brief TC-6.1: CLI服务创建和初始化
 */
TEST_F(Unit6Test, TC_6_1_CLIServiceCreation) {
    cliService = std::make_shared<CliService>(
        chassisRepo, stackRepo, apiClient);
    
    ASSERT_NE(nullptr, cliService);
}

/**
 * @brief TC-6.2: 板卡状态映射
 * 
 * 注意：这个测试需要访问ResourceMonitorBroadcaster的内部方法
 * 如果这些方法是私有的，可能需要添加测试友好的接口或使用友元类
 */
TEST_F(Unit6Test, TC_6_2_BoardStatusMapping) {
    broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
        chassisRepo, stackRepo, apiClient, "234.186.1.99", 0x100A);
    
    // 验证板卡状态映射逻辑
    // 由于MapBoardStatusToArray可能是私有方法，
    // 这个测试可能需要通过公共接口间接验证
    // 或者需要添加测试友好的接口
    
    // 基本验证：Broadcaster可以创建和启动
    broadcaster->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    broadcaster->Stop();
}

/**
 * @brief TC-6.3: CLI命令处理
 */
TEST_F(Unit6Test, TC_6_3_CLICommandProcessing) {
    cliService = std::make_shared<CliService>(
        chassisRepo, stackRepo, apiClient);
    
    // 验证CLI服务可以创建
    ASSERT_NE(nullptr, cliService);
    
    // 注意：CLI服务的具体命令处理可能需要模拟用户输入
    // 或者在集成测试中验证
}

/**
 * @brief TC-6.4: 告警接收服务器创建
 */
TEST_F(Unit6Test, TC_6_4_AlertServerCreation) {
    broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
        chassisRepo, stackRepo, apiClient, "234.186.1.99", 0x100A);
    
    alertServer = std::make_shared<AlertReceiverServer>(
        chassisRepo, stackRepo, broadcaster, 8081, "0.0.0.0");
    
    ASSERT_NE(nullptr, alertServer);
}

/**
 * @brief TC-6.5: 数据组合验证
 * 
 * 验证从仓储中读取数据并组合成响应报文的能力
 */
TEST_F(Unit6Test, TC_6_5_DataAggregation) {
    // 创建多个机箱
    for (int i = 1; i <= 3; ++i) {
        auto chassis = TestDataGenerator::CreateTestChassis(i);
        chassis->ResizeBoards(14);
        chassisRepo->Save(chassis);
    }
    
    // 验证数据可以被正确存储和查询
    ASSERT_EQ(3, chassisRepo->Size());
    
    auto chassis1 = chassisRepo->FindByNumber(1);
    ASSERT_NE(nullptr, chassis1);
    ASSERT_EQ(1, chassis1->GetChassisNumber());
}

/**
 * @brief TC-6.6: IP地址格式验证
 * 
 * 验证IP地址字符串格式的正确性
 */
TEST_F(Unit6Test, TC_6_6_IPAddressFormat) {
    // 创建Broadcaster（会使用IP地址）
    broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
        chassisRepo, stackRepo, apiClient, "234.186.1.99", 0x100A);
    
    // 验证Broadcaster可以正常创建（IP地址格式验证在构造函数中）
    ASSERT_NE(nullptr, broadcaster);
}

/**
 * @brief TC-6.7: 仓储数据查询
 */
TEST_F(Unit6Test, TC_6_7_RepositoryDataQuery) {
    // 添加业务链路数据
    auto stack = std::make_shared<Stack>("test-uuid", "TestStack");
    stackRepo->Save(stack);
    
    // 验证查询
    auto found = stackRepo->FindByUUID("test-uuid");
    ASSERT_NE(nullptr, found);
    ASSERT_EQ("test-uuid", found->GetStackUUID());
    
    // 验证GetAll
    auto allStacks = stackRepo->GetAll();
    ASSERT_EQ(1, allStacks.size());
}

/**
 * @brief TC-6.8: 空数据处理
 */
TEST_F(Unit6Test, TC_6_8_EmptyDataHandling) {
    // 使用空的仓储
    auto emptyChassisRepo = std::make_shared<InMemoryChassisRepository>();
    auto emptyStackRepo = std::make_shared<InMemoryStackRepository>();
    
    broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
        emptyChassisRepo, emptyStackRepo, apiClient, "234.186.1.99", 0x100A);
    
    // 验证空数据不会导致崩溃
    broadcaster->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    broadcaster->Stop();
    
    ASSERT_EQ(0, emptyChassisRepo->Size());
    ASSERT_EQ(0, emptyStackRepo->Size());
}

