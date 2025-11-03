/**
 * @file unit4_test.cpp
 * @brief 单元4测试 - 业务数据存取单元
 */

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include "infrastructure/persistence/in_memory_stack_repository.h"
#include "domain/stack.h"
#include "domain/service.h"
#include "domain/task.h"
#include "domain/value_objects.h"

using namespace app::infrastructure;
using namespace app::domain;

/**
 * @brief 单元4测试套件
 */
class Unit4Test : public ::testing::Test {
protected:
    void SetUp() override {
        repo = std::make_shared<InMemoryStackRepository>();
    }
    
    void TearDown() override {
        repo.reset();
    }
    
    std::shared_ptr<InMemoryStackRepository> repo;
};

/**
 * @brief TC-4.1: 业务链路保存和查询
 */
TEST_F(Unit4Test, TC_4_1_StackSaveAndQuery) {
    auto stack = std::make_shared<Stack>("test-uuid", "TestStack");
    
    repo->Save(stack);
    
    auto found = repo->FindByUUID("test-uuid");
    
    ASSERT_NE(nullptr, found);
    ASSERT_EQ("test-uuid", found->GetStackUUID());
    ASSERT_EQ("TestStack", found->GetStackName());
}

/**
 * @brief TC-4.2: 业务链路更新
 */
TEST_F(Unit4Test, TC_4_2_StackUpdate) {
    auto stack = std::make_shared<Stack>("test-uuid", "TestStack");
    stack->UpdateDeployStatus(0); // 未部署
    repo->Save(stack);
    
    // 更新状态
    stack->UpdateDeployStatus(1); // 已部署
    repo->Save(stack);
    
    auto found = repo->FindByUUID("test-uuid");
    ASSERT_NE(nullptr, found);
    ASSERT_EQ(1, found->GetDeployStatus());
}

/**
 * @brief TC-4.3: 业务链路清空
 */
TEST_F(Unit4Test, TC_4_3_StackClear) {
    // 保存多个业务链路
    for (int i = 1; i <= 5; ++i) {
        auto stack = std::make_shared<Stack>("uuid-" + std::to_string(i), 
                                             "Stack" + std::to_string(i));
        repo->Save(stack);
    }
    
    ASSERT_EQ(5, repo->Size());
    
    repo->Clear();
    
    ASSERT_EQ(0, repo->Size());
    ASSERT_TRUE(repo->GetAll().empty());
}

/**
 * @brief TC-4.4: 服务添加和查询
 */
TEST_F(Unit4Test, TC_4_4_ServiceAddAndQuery) {
    auto stack = std::make_shared<Stack>("test-uuid", "TestStack");
    
    Service service("service-uuid", "Service1", 0);
    service.UpdateStatus(2); // 运行正常
    stack->AddOrUpdateService(service);
    
    repo->Save(stack);
    
    auto found = repo->FindByUUID("test-uuid");
    ASSERT_NE(nullptr, found);
    
    auto services = found->GetAllServices();
    ASSERT_GT(services.size(), 0);
    ASSERT_TRUE(services.count("service-uuid") > 0);
    
    const auto& savedService = services.at("service-uuid");
    ASSERT_EQ("Service1", savedService.GetServiceName());
    ASSERT_EQ(2, savedService.GetServiceStatus());
}

/**
 * @brief TC-4.5: 任务添加和资源更新
 */
TEST_F(Unit4Test, TC_4_5_TaskAddAndResourceUpdate) {
    auto stack = std::make_shared<Stack>("test-uuid", "TestStack");
    Service service("service-uuid", "Service1", 0);
    
    Task task("task-1", 1); // 运行中
    task.SetBoardAddress("192.168.0.101");
    
    ResourceUsage resources;
    resources.cpuCores = 4.0f;
    resources.cpuUsed = 2.0f;
    resources.cpuUsage = 50.0f;
    resources.memorySize = 8192.0f;
    resources.memoryUsed = 4096.0f;
    resources.memoryUsage = 50.0f;
    resources.netReceive = 100.0f;
    resources.netSent = 50.0f;
    resources.gpuMemUsed = 2048.0f;
    task.UpdateResources(resources);
    
    service.AddOrUpdateTask("task-1", task);
    stack->AddOrUpdateService(service);
    repo->Save(stack);
    
    auto found = repo->FindByUUID("test-uuid");
    ASSERT_NE(nullptr, found);
    
    auto services = found->GetAllServices();
    const auto& savedService = services.at("service-uuid");
    auto tasks = savedService.GetAllTasks();
    
    ASSERT_TRUE(tasks.count("task-1") > 0);
    
    const auto& savedTask = tasks.at("task-1");
    ASSERT_EQ("task-1", savedTask.GetTaskID());
    ASSERT_EQ(1, savedTask.GetTaskStatus());
    ASSERT_FLOAT_EQ(50.0f, savedTask.GetResources().cpuUsage);
    ASSERT_FLOAT_EQ(50.0f, savedTask.GetResources().memoryUsage);
    ASSERT_FLOAT_EQ(100.0f, savedTask.GetResources().netReceive);
    ASSERT_FLOAT_EQ(2048.0f, savedTask.GetResources().gpuMemUsed);
}

/**
 * @brief TC-4.6: 根据标签查找业务链路
 */
TEST_F(Unit4Test, TC_4_6_FindByLabel) {
    auto stack1 = std::make_shared<Stack>("uuid-1", "Stack1");
    auto stack2 = std::make_shared<Stack>("uuid-2", "Stack2");
    
    // 添加标签
    std::vector<StackLabelInfo> labels1;
    StackLabelInfo label1;
    label1.stackLabelUUID = "label-1";
    label1.stackLabelName = "label1";
    labels1.push_back(label1);
    stack1->SetLabels(labels1);
    
    std::vector<StackLabelInfo> labels2;
    StackLabelInfo label2;
    label2.stackLabelUUID = "label-1"; // 相同标签
    label2.stackLabelName = "label1";
    labels2.push_back(label2);
    stack2->SetLabels(labels2);
    
    repo->Save(stack1);
    repo->Save(stack2);
    
    // 根据标签查找
    auto found = repo->FindByLabel("label-1");
    
    ASSERT_EQ(2, found.size());
    
    // 验证找到的业务链路
    std::set<std::string> foundUUIDs;
    for (const auto& s : found) {
        foundUUIDs.insert(s->GetStackUUID());
    }
    ASSERT_TRUE(foundUUIDs.count("uuid-1") > 0);
    ASSERT_TRUE(foundUUIDs.count("uuid-2") > 0);
}

/**
 * @brief TC-4.7: 获取任务资源
 */
TEST_F(Unit4Test, TC_4_7_GetTaskResources) {
    auto stack = std::make_shared<Stack>("test-uuid", "TestStack");
    Service service("service-uuid", "Service1", 0);
    
    Task task("task-1", 1);
    ResourceUsage resources;
    resources.cpuUsage = 75.5f;
    resources.memoryUsage = 80.0f;
    task.UpdateResources(resources);
    
    service.AddOrUpdateTask("task-1", task);
    stack->AddOrUpdateService(service);
    repo->Save(stack);
    
    // 通过taskID查找资源
    auto taskResources = repo->GetTaskResources("task-1");
    
    ASSERT_TRUE(taskResources.has_value());
    ASSERT_FLOAT_EQ(75.5f, taskResources->cpuUsage);
    ASSERT_FLOAT_EQ(80.0f, taskResources->memoryUsage);
}

/**
 * @brief TC-4.8: 查询不存在的任务资源
 */
TEST_F(Unit4Test, TC_4_8_GetNonexistentTaskResources) {
    auto taskResources = repo->GetTaskResources("nonexistent-task");
    
    ASSERT_FALSE(taskResources.has_value());
}

