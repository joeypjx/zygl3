/**
 * @file test_data_generator.h
 * @brief 测试数据生成器
 */

#pragma once

#include <memory>
#include <string>
#include "src/domain/chassis.h"
#include "src/domain/board.h"
#include "src/domain/stack.h"
#include "src/domain/service.h"
#include "src/domain/task.h"
#include "src/domain/value_objects.h"
#include "src/infrastructure/api_client/qyw_api_client.h"

namespace app::test {

/**
 * @brief 测试数据生成器
 */
class TestDataGenerator {
public:
    /**
     * @brief 创建测试用的机箱
     */
    static std::shared_ptr<app::domain::Chassis> CreateTestChassis(
        int chassisNumber, 
        const std::string& chassisName = "") {
        if (chassisName.empty()) {
            return std::make_shared<app::domain::Chassis>(
                chassisNumber, "Chassis_" + std::to_string(chassisNumber));
        }
        return std::make_shared<app::domain::Chassis>(chassisNumber, chassisName);
    }
    
    /**
     * @brief 创建测试用的板卡信息响应
     */
    static app::infrastructure::BoardInfoResponse CreateTestBoardInfo(
        int chassisNum, 
        int boardNum,
        const std::string& boardAddress = "") {
        app::infrastructure::BoardInfoResponse info;
        info.chassisNumber = chassisNum;
        info.chassisName = "Chassis_" + std::to_string(chassisNum);
        info.boardNumber = boardNum;
        info.boardName = "Board_" + std::to_string(boardNum);
        if (boardAddress.empty()) {
            info.boardAddress = "192.168.0." + std::to_string(chassisNum * 100 + boardNum);
        } else {
            info.boardAddress = boardAddress;
        }
        info.boardType = (boardNum == 6 || boardNum == 7) ? 1 : 
                        (boardNum == 13 || boardNum == 14) ? 2 : 0;
        info.boardStatus = 0; // 正常
        info.voltage = 12.5f;
        info.current = 2.0f;
        info.temperature = 45.0f;
        
        // 添加风扇信息
        app::infrastructure::FanSpeed fan1;
        fan1.fanName = "Fan1";
        fan1.speed = 3000.0f;
        info.fanSpeeds.push_back(fan1);
        
        return info;
    }
    
    /**
     * @brief 创建测试用的业务链路信息响应
     */
    static app::infrastructure::StackInfoResponse CreateTestStackInfo(
        const std::string& uuid,
        const std::string& name = "") {
        app::infrastructure::StackInfoResponse info;
        info.stackUUID = uuid;
        info.stackName = name.empty() ? "Stack_" + uuid.substr(0, 8) : name;
        info.stackDeployStatus = 1; // 已部署
        info.stackRunningStatus = 1; // 正常运行
        
        // 添加标签
        app::infrastructure::LabelInfo label;
        label.stackLabelUUID = "label-uuid-1";
        label.stackLabelName = "label1";
        info.stackLabelInfos.push_back(label);
        
        // 添加服务
        app::infrastructure::ServiceInfo service;
        service.serviceUUID = "service-uuid-1";
        service.serviceName = "Service1";
        service.serviceStatus = 2; // 运行正常
        service.serviceType = 0; // 普通组件
        
        // 添加任务
        app::infrastructure::ServiceTaskInfo task;
        task.taskID = "task-1";
        task.taskStatus = 1; // 运行中
        task.cpuCores = 4.0f;
        task.cpuUsed = 2.0f;
        task.cpuUsage = 50.0f;
        task.memorySize = 8192.0f;
        task.memoryUsed = 4096.0f;
        task.memoryUsage = 50.0f;
        task.netReceive = 100.0f;
        task.netSent = 50.0f;
        task.gpuMemUsed = 2048.0f;
        task.chassisNumber = 1;
        task.boardNumber = 1;
        task.boardAddress = "192.168.0.101";
        service.taskInfos.push_back(task);
        
        info.serviceInfos.push_back(service);
        
        return info;
    }
    
    /**
     * @brief 创建测试用的部署响应
     */
    static app::infrastructure::DeployResponse CreateTestDeployResponse(
        bool success,
        const std::string& stackName = "TestStack",
        const std::string& stackUUID = "test-uuid") {
        app::infrastructure::DeployResponse response;
        
        app::infrastructure::StackOperationInfo info;
        info.stackName = stackName;
        info.stackUUID = stackUUID;
        info.message = success ? "部署成功" : "部署失败";
        
        if (success) {
            response.successStackInfos.push_back(info);
        } else {
            response.failureStackInfos.push_back(info);
        }
        
        return response;
    }
};

} // namespace app::test

