/**
 * @file unit5_test.cpp
 * @brief 单元5测试 - 前端组播交互单元
 * 
 * 注意：由于涉及UDP socket和网络通信，部分测试可能需要Mock或集成测试环境
 */

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include <cstring>
#include "interfaces/udp/resource_monitor_broadcaster.h"
#include "infrastructure/persistence/in_memory_chassis_repository.h"
#include "infrastructure/persistence/in_memory_stack_repository.h"
#include "infrastructure/api_client/qyw_api_client.h"
#include "domain/chassis.h"
#include "domain/board.h"
#include "utils/test_data_generator.h"

using namespace app::infrastructure;
using namespace app::domain;
using namespace app::interfaces;
using namespace app::test;

/**
 * @brief 单元5测试套件
 */
class Unit5Test : public ::testing::Test {
protected:
    void SetUp() override {
        chassisRepo = std::make_shared<InMemoryChassisRepository>();
        stackRepo = std::make_shared<InMemoryStackRepository>();
        
        // 创建API客户端（测试环境中可以使用真实客户端或Mock）
        apiClient = std::make_shared<QywApiClient>("localhost", 8080);
        
        // 创建机箱和板卡数据用于测试
        auto chassis = TestDataGenerator::CreateTestChassis(1);
        chassis->ResizeBoards(14);
        chassisRepo->Save(chassis);
    }
    
    void TearDown() override {
        if (broadcaster) {
            broadcaster->Stop();
        }
        broadcaster.reset();
        apiClient.reset();
        stackRepo.reset();
        chassisRepo.reset();
    }
    
    std::shared_ptr<InMemoryChassisRepository> chassisRepo;
    std::shared_ptr<InMemoryStackRepository> stackRepo;
    std::shared_ptr<QywApiClient> apiClient;
    std::shared_ptr<ResourceMonitorBroadcaster> broadcaster;
};

/**
 * @brief TC-5.1: Broadcaster创建和初始化
 * 
 * 注意：这是一个基础测试，验证对象可以正常创建
 */
TEST_F(Unit5Test, TC_5_1_BroadcasterCreation) {
    // 创建Broadcaster实例
    broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
        chassisRepo, stackRepo, apiClient, "234.186.1.99", 0x100A);
    
    ASSERT_NE(nullptr, broadcaster);
    
    // 验证对象已创建（无法直接访问socket，但可以验证对象存在）
}

/**
 * @brief TC-5.2: 启动和停止服务
 */
TEST_F(Unit5Test, TC_5_2_StartAndStop) {
    broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
        chassisRepo, stackRepo, apiClient, "234.186.1.99", 0x100A);
    
    // 启动服务
    broadcaster->Start();
    
    // 等待一小段时间确保服务启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 停止服务
    broadcaster->Stop();
    
    // 验证服务可以正常停止（不会崩溃）
    // 注意：如果ResourceMonitorBroadcaster没有IsRunning()方法，
    // 我们只能验证Stop()不会抛出异常
}

/**
 * @brief TC-5.3: 组播地址和端口配置
 */
TEST_F(Unit5Test, TC_5_3_MulticastConfiguration) {
    const std::string testMulticastIp = "234.186.1.99";
    const uint16_t testPort = 0x100A;
    
    broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
        chassisRepo, stackRepo, apiClient, testMulticastIp, testPort);
    
    // 验证对象创建成功（组播地址在构造函数中设置）
    ASSERT_NE(nullptr, broadcaster);
    
    // 注意：如果ResourceMonitorBroadcaster有getter方法，可以验证组播地址
    // 否则，这个测试主要验证对象创建不会失败
}

/**
 * @brief TC-5.4: 多次启动和停止
 */
TEST_F(Unit5Test, TC_5_4_MultipleStartStop) {
    broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
        chassisRepo, stackRepo, apiClient, "234.186.1.99", 0x100A);
    
    // 多次启动和停止
    for (int i = 0; i < 3; ++i) {
        broadcaster->Start();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        broadcaster->Stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // 验证多次操作不会导致问题
}

/**
 * @brief TC-5.5: 数据发送（需要Mock UDP socket或集成测试环境）
 * 
 * 注意：这个测试在实际的UDP环境中可能无法直接验证数据发送
 * 在集成测试中，可以使用网络抓包工具或UDP接收端验证
 */
TEST_F(Unit5Test, TC_5_5_DataBroadcast) {
    broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
        chassisRepo, stackRepo, apiClient, "234.186.1.99", 0x100A);
    
    // 启动服务
    broadcaster->Start();
    
    // 等待一段时间让数据发送
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    broadcaster->Stop();
    
    // 在集成测试中，这里应该验证UDP数据包被正确发送
    // 在单元测试中，我们只能验证服务运行期间不会崩溃
}

