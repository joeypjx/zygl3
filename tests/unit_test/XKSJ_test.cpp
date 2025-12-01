/**
 * @file XKSJ_test.cpp
 * @brief 数据存取单元测试 - 合并了机箱仓储和业务链路仓储的测试
 * 
 * 使用Google Test框架测试数据存取单元
 */

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include "src/infrastructure/persistence/in_memory_chassis_repository.h"
#include "src/infrastructure/persistence/in_memory_stack_repository.h"
#include "src/domain/chassis.h"
#include "src/domain/board.h"
#include "src/domain/stack.h"
#include "src/domain/service.h"
#include "src/domain/task.h"
#include "src/domain/value_objects.h"

using namespace app::infrastructure;
using namespace app::domain;

/**
 * @brief 数据存取单元测试套件
 */
class XKSJTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 测试前置设置
        chassisRepo = std::make_shared<InMemoryChassisRepository>();
        stackRepo = std::make_shared<InMemoryStackRepository>();
    }
    
    void TearDown() override {
        // 测试后清理
        chassisRepo.reset();
        stackRepo.reset();
    }
    
    std::shared_ptr<InMemoryChassisRepository> chassisRepo;
    std::shared_ptr<InMemoryStackRepository> stackRepo;
};

/**
 * @brief TC-Chassis-SaveSuccess: 机箱仓储Save方法存储成功测试
 */
TEST_F(XKSJTest, TC_Chassis_SaveSuccess) {
    // 创建有效的机箱对象
    auto chassis = std::make_shared<Chassis>(5, "TestChassis_5");
    
    // 验证初始状态
    ASSERT_EQ(0, chassisRepo->Size()) << "初始仓储应该为空";
    ASSERT_EQ(nullptr, chassisRepo->FindByNumber(5)) << "初始时不应该找到机箱";
    
    // 执行保存操作
    chassisRepo->Save(chassis);
    
    // 验证存储成功
    ASSERT_EQ(1, chassisRepo->Size()) << "仓储大小应该为1";
    auto found = chassisRepo->FindByNumber(5);
    ASSERT_NE(nullptr, found) << "应该能够找到保存的机箱";
    ASSERT_EQ(5, found->GetChassisNumber()) << "机箱号应该匹配";
    ASSERT_EQ("TestChassis_5", found->GetChassisName()) << "机箱名称应该匹配";
    
    // 验证是同一个对象（通过shared_ptr的引用计数或内容比较）
    ASSERT_EQ(chassis->GetChassisNumber(), found->GetChassisNumber()) << "应该是同一个机箱对象";
}

/**
 * @brief TC-Chassis-SaveFailure: 机箱仓储Save方法存储失败测试
 */
TEST_F(XKSJTest, TC_Chassis_SaveFailure) {
    // 验证初始状态
    ASSERT_EQ(0, chassisRepo->Size()) << "初始仓储应该为空";
    
    // 尝试保存空指针（失败场景）
    std::shared_ptr<Chassis> nullChassis = nullptr;
    chassisRepo->Save(nullChassis);
    
    // 验证存储失败：仓储大小应该保持不变
    ASSERT_EQ(0, chassisRepo->Size()) << "空指针不应该被保存，仓储大小应该保持为0";
    
    // 验证仓储中没有数据
    auto allChassis = chassisRepo->GetAll();
    ASSERT_EQ(0, allChassis.size()) << "仓储中不应该有任何机箱";
}

/**
 * @brief TC-Chassis-GetAllDataIntegrity: 机箱仓储GetAll方法数据完整性测试
 */
TEST_F(XKSJTest, TC_Chassis_GetAllDataIntegrity) {
    // 创建并保存多个不同机箱号的机箱
    std::vector<std::shared_ptr<Chassis>> savedChassis;
    savedChassis.push_back(std::make_shared<Chassis>(1, "Chassis_1"));
    savedChassis.push_back(std::make_shared<Chassis>(5, "Chassis_5"));
    savedChassis.push_back(std::make_shared<Chassis>(10, "Chassis_10"));
    savedChassis.push_back(std::make_shared<Chassis>(3, "Chassis_3"));
    
    // 保存所有机箱
    for (const auto& chassis : savedChassis) {
        chassisRepo->Save(chassis);
    }
    
    // 验证仓储大小
    ASSERT_EQ(4, chassisRepo->Size()) << "应该保存了4个机箱";
    
    // 获取所有机箱
    auto allChassis = chassisRepo->GetAll();
    
    // 验证返回的机箱数量
    ASSERT_EQ(4, allChassis.size()) << "GetAll应该返回4个机箱";
    
    // 验证每个保存的机箱都能在GetAll结果中找到，且数据正确
    for (const auto& saved : savedChassis) {
        bool found = false;
        for (const auto& retrieved : allChassis) {
            if (retrieved->GetChassisNumber() == saved->GetChassisNumber()) {
                found = true;
                // 验证机箱数据完整性
                ASSERT_EQ(saved->GetChassisNumber(), retrieved->GetChassisNumber())
                    << "机箱号应该匹配: " << saved->GetChassisNumber();
                ASSERT_EQ(saved->GetChassisName(), retrieved->GetChassisName())
                    << "机箱名称应该匹配: " << saved->GetChassisName();
                break;
            }
        }
        ASSERT_TRUE(found) << "应该能在GetAll结果中找到机箱: " << saved->GetChassisNumber();
    }
    
    // 验证GetAll返回的机箱数量与仓储大小一致
    ASSERT_EQ(chassisRepo->Size(), allChassis.size()) << "GetAll返回的数量应该与仓储大小一致";
}

/**
 * @brief TC-Stack-SaveSuccess: 业务链路仓储Save方法存储成功测试
 */
TEST_F(XKSJTest, TC_Stack_SaveSuccess) {
    // 创建有效的业务链路对象
    auto stack = std::make_shared<Stack>("test-uuid-5", "TestStack_5");
    stack->UpdateDeployStatus(1);
    stack->UpdateRunningStatus(1);
    
    // 验证初始状态
    ASSERT_EQ(0, stackRepo->Size()) << "初始仓储应该为空";
    ASSERT_EQ(nullptr, stackRepo->FindByUUID("test-uuid-5")) << "初始时不应该找到业务链路";
    
    // 执行保存操作
    stackRepo->Save(stack);
    
    // 验证存储成功
    ASSERT_EQ(1, stackRepo->Size()) << "仓储大小应该为1";
    auto found = stackRepo->FindByUUID("test-uuid-5");
    ASSERT_NE(nullptr, found) << "应该能够找到保存的业务链路";
    ASSERT_EQ("test-uuid-5", found->GetStackUUID()) << "业务链路UUID应该匹配";
    ASSERT_EQ("TestStack_5", found->GetStackName()) << "业务链路名称应该匹配";
    ASSERT_EQ(1, found->GetDeployStatus()) << "部署状态应该匹配";
    ASSERT_EQ(1, found->GetRunningStatus()) << "运行状态应该匹配";
    
    // 验证是同一个对象（通过内容比较）
    ASSERT_EQ(stack->GetStackUUID(), found->GetStackUUID()) << "应该是同一个业务链路对象";
}

/**
 * @brief TC-Stack-SaveFailure: 业务链路仓储Save方法存储失败测试
 */
TEST_F(XKSJTest, TC_Stack_SaveFailure) {
    // 验证初始状态
    ASSERT_EQ(0, stackRepo->Size()) << "初始仓储应该为空";
    
    // 尝试保存空指针（失败场景）
    std::shared_ptr<Stack> nullStack = nullptr;
    stackRepo->Save(nullStack);
    
    // 验证存储失败：仓储大小应该保持不变
    ASSERT_EQ(0, stackRepo->Size()) << "空指针不应该被保存，仓储大小应该保持为0";
    
    // 验证仓储中没有数据
    auto allStacks = stackRepo->GetAll();
    ASSERT_EQ(0, allStacks.size()) << "仓储中不应该有任何业务链路";
}

/**
 * @brief TC-Stack-GetAllDataIntegrity: 业务链路仓储GetAll方法数据完整性测试
 */
TEST_F(XKSJTest, TC_Stack_GetAllDataIntegrity) {
    // 创建并保存多个不同UUID的业务链路
    std::vector<std::shared_ptr<Stack>> savedStacks;
    savedStacks.push_back(std::make_shared<Stack>("uuid-1", "Stack_1"));
    savedStacks.push_back(std::make_shared<Stack>("uuid-5", "Stack_5"));
    savedStacks.push_back(std::make_shared<Stack>("uuid-10", "Stack_10"));
    savedStacks.push_back(std::make_shared<Stack>("uuid-3", "Stack_3"));
    
    // 设置不同的状态
    savedStacks[0]->UpdateDeployStatus(0);
    savedStacks[0]->UpdateRunningStatus(0);
    savedStacks[1]->UpdateDeployStatus(1);
    savedStacks[1]->UpdateRunningStatus(1);
    savedStacks[2]->UpdateDeployStatus(1);
    savedStacks[2]->UpdateRunningStatus(2);
    savedStacks[3]->UpdateDeployStatus(0);
    savedStacks[3]->UpdateRunningStatus(0);
    
    // 保存所有业务链路
    for (const auto& stack : savedStacks) {
        stackRepo->Save(stack);
    }
    
    // 验证仓储大小
    ASSERT_EQ(4, stackRepo->Size()) << "应该保存了4个业务链路";
    
    // 获取所有业务链路
    auto allStacks = stackRepo->GetAll();
    
    // 验证返回的业务链路数量
    ASSERT_EQ(4, allStacks.size()) << "GetAll应该返回4个业务链路";
    
    // 验证每个保存的业务链路都能在GetAll结果中找到，且数据正确
    for (const auto& saved : savedStacks) {
        bool found = false;
        for (const auto& retrieved : allStacks) {
            if (retrieved->GetStackUUID() == saved->GetStackUUID()) {
                found = true;
                // 验证业务链路数据完整性
                ASSERT_EQ(saved->GetStackUUID(), retrieved->GetStackUUID())
                    << "业务链路UUID应该匹配: " << saved->GetStackUUID();
                ASSERT_EQ(saved->GetStackName(), retrieved->GetStackName())
                    << "业务链路名称应该匹配: " << saved->GetStackName();
                ASSERT_EQ(saved->GetDeployStatus(), retrieved->GetDeployStatus())
                    << "部署状态应该匹配: " << saved->GetStackUUID();
                ASSERT_EQ(saved->GetRunningStatus(), retrieved->GetRunningStatus())
                    << "运行状态应该匹配: " << saved->GetStackUUID();
                break;
            }
        }
        ASSERT_TRUE(found) << "应该能在GetAll结果中找到业务链路: " << saved->GetStackUUID();
    }
    
    // 验证GetAll返回的业务链路数量与仓储大小一致
    ASSERT_EQ(stackRepo->Size(), allStacks.size()) << "GetAll返回的数量应该与仓储大小一致";
}
