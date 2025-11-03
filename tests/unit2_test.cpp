/**
 * @file unit2_test.cpp
 * @brief 单元2测试 - 资源数据存取单元
 * 
 * 使用Google Test框架测试单元2（资源数据存取单元）
 */

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include "infrastructure/persistence/in_memory_chassis_repository.h"
#include "domain/chassis.h"
#include "domain/board.h"
#include "domain/value_objects.h"

using namespace app::infrastructure;
using namespace app::domain;

/**
 * @brief 单元2测试套件
 */
class Unit2Test : public ::testing::Test {
protected:
    void SetUp() override {
        // 测试前置设置
        repo = std::make_shared<InMemoryChassisRepository>();
    }
    
    void TearDown() override {
        // 测试后清理
        repo.reset();
    }
    
    std::shared_ptr<InMemoryChassisRepository> repo;
};

/**
 * @brief TC-2.1: 机箱保存和查询测试
 */
TEST_F(Unit2Test, TC_2_1_ChassisSaveAndQuery) {
    // 创建Chassis对象
    auto chassis = std::make_shared<Chassis>(1, "TestChassis_1");
    
    // 保存机箱
    repo->Save(chassis);
    
    // 查询机箱
    auto found = repo->FindByNumber(1);
    
    // 验证结果
    ASSERT_NE(nullptr, found) << "应该找到机箱";
    ASSERT_EQ(1, found->GetChassisNumber()) << "机箱号应该为1";
    ASSERT_EQ("TestChassis_1", found->GetChassisName()) << "机箱名称应该匹配";
    ASSERT_EQ(1, repo->Size()) << "仓储大小应该为1";
}

/**
 * @brief TC-2.2: 板卡更新测试
 */
TEST_F(Unit2Test, TC_2_2_BoardUpdate) {
    // 创建机箱并保存
    auto chassis = std::make_shared<Chassis>(1, "TestChassis_1");
    chassis->ResizeBoards(14);
    repo->Save(chassis);
    
    // 创建板卡并更新
    Board newBoard("192.168.1.1", 1, BoardType::Computing);
    std::vector<TaskStatusInfo> taskInfos;
    std::vector<FanSpeed> fanSpeeds;
    newBoard.UpdateFromApiData("Board_1", 0, 12.5f, 2.0f, 45.0f, fanSpeeds, taskInfos);
    
    // 更新板卡
    repo->UpdateBoard(1, 1, newBoard);
    
    // 验证结果
    auto updatedChassis = repo->FindByNumber(1);
    ASSERT_NE(nullptr, updatedChassis) << "应该找到机箱";
    
    auto* board = updatedChassis->GetBoardBySlot(1);
    ASSERT_NE(nullptr, board) << "应该找到板卡";
    ASSERT_EQ("192.168.1.1", board->GetAddress()) << "板卡IP应该匹配";
    ASSERT_EQ("Board_1", board->GetBoardName()) << "板卡名称应该匹配";
    ASSERT_FLOAT_EQ(12.5f, board->GetVoltage()) << "电压应该匹配";
    ASSERT_FLOAT_EQ(2.0f, board->GetCurrent()) << "电流应该匹配";
    ASSERT_FLOAT_EQ(45.0f, board->GetTemperature()) << "温度应该匹配";
}

/**
 * @brief TC-2.3: 并发访问安全测试
 */
TEST_F(Unit2Test, TC_2_3_ConcurrentAccess) {
    // 创建并保存一个机箱
    auto chassis = std::make_shared<Chassis>(1, "TestChassis_1");
    repo->Save(chassis);
    
    // 启动多个线程同时访问
    const int THREAD_COUNT = 10;
    const int OPERATIONS_PER_THREAD = 100;
    std::atomic<int> successCount(0);
    std::vector<std::thread> threads;
    
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                // 读取操作
                auto found = repo->FindByNumber(1);
                if (found != nullptr) {
                    successCount++;
                }
                
                // 写入操作
                repo->Save(chassis);
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    // 验证结果
    ASSERT_EQ(THREAD_COUNT * OPERATIONS_PER_THREAD, successCount.load())
        << "所有读取操作应该成功";
    ASSERT_EQ(1, repo->Size()) << "仓储中应该只有1个机箱";
}

/**
 * @brief TC-2.4: 获取所有机箱测试
 */
TEST_F(Unit2Test, TC_2_4_GetAllChassis) {
    // 保存9个机箱
    for (int i = 1; i <= 9; ++i) {
        auto chassis = std::make_shared<Chassis>(i, "Chassis_" + std::to_string(i));
        repo->Save(chassis);
    }
    
    // 获取所有机箱
    auto allChassis = repo->GetAll();
    
    // 验证结果
    ASSERT_EQ(9, allChassis.size()) << "应该返回9个机箱";
    
    // 验证每个机箱的机箱号正确
    for (size_t i = 0; i < allChassis.size(); ++i) {
        int expectedNumber = static_cast<int>(i + 1);
        ASSERT_EQ(expectedNumber, allChassis[i]->GetChassisNumber())
            << "机箱号应该匹配: " << expectedNumber;
    }
}

/**
 * @brief TC-2.5: 查询不存在的机箱测试
 */
TEST_F(Unit2Test, TC_2_5_QueryNonexistentChassis) {
    // 查询不存在的机箱
    auto found = repo->FindByNumber(999);
    
    // 验证结果
    ASSERT_EQ(nullptr, found) << "应该返回nullptr";
}

/**
 * @brief TC-2.6: 保存空指针测试
 */
TEST_F(Unit2Test, TC_2_6_SaveNullPointer) {
    // 尝试保存空指针
    std::shared_ptr<Chassis> nullChassis = nullptr;
    repo->Save(nullChassis);
    
    // 验证仓储大小不变
    ASSERT_EQ(0, repo->Size()) << "空指针不应该被保存";
}

/**
 * @brief TC-2.7: 更新不存在的板卡测试
 */
TEST_F(Unit2Test, TC_2_7_UpdateNonexistentBoard) {
    // 创建机箱但不包含板卡
    auto chassis = std::make_shared<Chassis>(1, "TestChassis_1");
    repo->Save(chassis);
    
    // 尝试更新不存在的板卡
    Board board("192.168.1.1", 99, BoardType::Computing);
    repo->UpdateBoard(1, 99, board);
    
    // 验证操作不崩溃（实现可能会忽略无效槽位）
    ASSERT_NE(nullptr, repo->FindByNumber(1)) << "机箱应该仍然存在";
}

