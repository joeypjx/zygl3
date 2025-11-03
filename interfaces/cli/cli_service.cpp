#include "cli_service.h"
#include "domain/i_chassis_repository.h"
#include "domain/i_stack_repository.h"
#include "domain/value_objects.h"
#include "domain/chassis.h"
#include "domain/board.h"
#include "domain/stack.h"
#include "infrastructure/api_client/qyw_api_client.h"
#include "infrastructure/config/config_manager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

namespace app::interfaces {

CliService::CliService(std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
                       std::shared_ptr<app::domain::IStackRepository> stackRepo,
                       std::shared_ptr<app::infrastructure::QywApiClient> apiClient)
    : m_chassisRepo(chassisRepo),
      m_stackRepo(stackRepo),
      m_apiClient(apiClient),
      m_running(false) {
}

CliService::~CliService() {
    Stop();
}

void CliService::Start() {
    if (m_running) {
        return;
    }
    
    m_running = true;
    m_thread = std::thread(&CliService::Run, this);
}

void CliService::Stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool CliService::IsRunning() const {
    return m_running;
}

void CliService::Run() {
    std::cout << "\n=== CLI服务已启动 ===" << std::endl;
    std::cout << "输入 'help' 查看可用命令" << std::endl;
    
    std::string line;
    while (m_running) {
        std::cout << "\nCLI> ";
        std::cout.flush();
        
        if (!std::getline(std::cin, line)) {
            break;
        }
        
        // 去除首尾空格
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty()) {
            continue;
        }
        
        ProcessCommand(line);
    }
    
    std::cout << "\nCLI服务已停止" << std::endl;
}

void CliService::ProcessCommand(const std::string& command) {
    if (command == "help" || command == "h") {
        PrintHelp();
    } else if (command == "quit" || command == "exit" || command == "q") {
        std::cout << "退出CLI服务..." << std::endl;
        m_running = false;
    } else if (command == "list" || command == "ls") {
        PrintAllChassisOverview();
    } else if (command == "all" || command == "a") {
        PrintAllChassisFullInfo();
    } else if (command == "stack" || command == "stacks") {
        PrintAllStacksOverview();
    } else if (command == "stackall" || command == "stacksall") {
        PrintAllStacksFullInfo();
    } else if (command.length() >= 6 && command.substr(0, 6) == "deploy") {
        // 解析 "deploy <标签1> [标签2] ..." 命令
        std::vector<std::string> labels;
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;
        std::string label;
        while (iss >> label) {
            labels.push_back(label);
        }
        if (labels.empty()) {
            std::cout << "请提供至少一个业务标签" << std::endl;
        } else {
            DeployStacks(labels);
        }
    } else if (command.length() >= 8 && command.substr(0, 8) == "undeploy") {
        // 解析 "undeploy <标签1> [标签2] ..." 命令
        std::vector<std::string> labels;
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;
        std::string label;
        while (iss >> label) {
            labels.push_back(label);
        }
        if (labels.empty()) {
            std::cout << "请提供至少一个业务标签" << std::endl;
        } else {
            UndeployStacks(labels);
        }
    } else if (command.length() >= 5 && command.substr(0, 5) == "show ") {
        // 解析 "show <机箱号>" 或 "show stack <UUID>" 命令
        std::istringstream iss(command);
        std::string cmd, type;
        iss >> cmd >> type;
        
        if (type == "stack") {
            std::string stackUUID;
            iss >> stackUUID;
            if (!stackUUID.empty()) {
                PrintStackDetail(stackUUID);
            } else {
                std::cout << "请提供业务链路的UUID" << std::endl;
            }
        } else {
            // 尝试解析为机箱号
            iss.seekg(5);  // 回到"show "之后
            int chassisNumber;
            iss >> chassisNumber;
            PrintChassisDetail(chassisNumber);
        }
    } else {
        std::cout << "未知命令: " << command << std::endl;
        std::cout << "输入 'help' 查看可用命令" << std::endl;
    }
}

void CliService::PrintHelp() {
    std::cout << "\n=== 可用命令 ===" << std::endl;
    std::cout << "机箱相关:" << std::endl;
    std::cout << "  list, ls              - 显示所有机箱概览" << std::endl;
    std::cout << "  all, a                - 显示所有机箱完整信息" << std::endl;
    std::cout << "  show <机箱号>         - 显示指定机箱的详细信息" << std::endl;
    std::cout << "业务链路相关:" << std::endl;
    std::cout << "  stack, stacks         - 显示所有业务链路概览" << std::endl;
    std::cout << "  stackall, stacksall   - 显示所有业务链路完整信息" << std::endl;
    std::cout << "  show stack <UUID>     - 显示指定业务链路的详细信息" << std::endl;
    std::cout << "  deploy <标签...>      - 启动指定标签的业务链路" << std::endl;
    std::cout << "  undeploy <标签...>    - 停止指定标签的业务链路" << std::endl;
    std::cout << "其他:" << std::endl;
    std::cout << "  help, h               - 显示此帮助信息" << std::endl;
    std::cout << "  quit, exit, q         - 退出CLI服务" << std::endl;
}

void CliService::PrintAllChassisOverview() {
    auto allChassis = m_chassisRepo->GetAll();
    
    if (allChassis.empty()) {
        std::cout << "没有找到任何机箱" << std::endl;
        return;
    }
    
    std::cout << "\n=== 机箱概览 ===" << std::endl;
    std::cout << "共 " << allChassis.size() << " 个机箱" << std::endl;
    PrintSeparator();
    std::cout << std::left << std::setw(8) << "机箱号" 
              << std::setw(20) << "机箱名称"
              << std::setw(10) << "板卡数量" << std::endl;
    PrintSeparator();
    
    for (const auto& chassis : allChassis) {
        if (!chassis) continue;
        std::cout << std::left << std::setw(8) << chassis->GetChassisNumber()
                  << std::setw(20) << chassis->GetChassisName()
                  << std::setw(10) << chassis->GetBoardCount() << std::endl;
    }
}

void CliService::PrintChassisDetail(int chassisNumber) {
    auto chassis = m_chassisRepo->FindByNumber(chassisNumber);
    
    if (!chassis) {
        std::cout << "未找到机箱号: " << chassisNumber << std::endl;
        return;
    }
    
    std::cout << "\n=== 机箱详细信息: " << chassis->GetChassisName() << " (机箱号: " 
              << chassis->GetChassisNumber() << ") ===" << std::endl;
    PrintSeparator();
    
    const auto& boards = chassis->GetAllBoards();
    std::cout << std::left << std::setw(6) << "槽位" 
              << std::setw(18) << "IP地址"
              << std::setw(12) << "板卡类型"
              << std::setw(10) << "状态"
              << std::setw(8) << "任务数" << std::endl;
    PrintSeparator();
    
    for (size_t i = 0; i < boards.size(); ++i) {
        const auto& board = boards[i];
        int slotNumber = static_cast<int>(i + 1);
        
        std::cout << std::left << std::setw(6) << slotNumber
                  << std::setw(18) << board.GetAddress()
                  << std::setw(12) << BoardTypeToString(board.GetBoardType())
                  << std::setw(10) << BoardStatusToString(board.GetStatus())
                  << std::setw(8) << board.GetTasks().size() << std::endl;
        
        // 显示板卡传感器信息
        if (!board.GetBoardName().empty()) {
            std::cout << "    板卡名称: " << board.GetBoardName() << std::endl;
        }
        if (board.GetVoltage() > 0 || board.GetCurrent() > 0 || board.GetTemperature() > 0) {
            std::cout << "    传感器: ";
            bool hasSensor = false;
            if (board.GetVoltage() > 0) {
                std::cout << "电压 " << std::fixed << std::setprecision(2) << board.GetVoltage() << "V";
                hasSensor = true;
            }
            if (board.GetCurrent() > 0) {
                if (hasSensor) std::cout << ", ";
                std::cout << "电流 " << std::setprecision(2) << board.GetCurrent() << "A";
                hasSensor = true;
            }
            if (board.GetTemperature() > 0) {
                if (hasSensor) std::cout << ", ";
                std::cout << "温度 " << std::setprecision(1) << board.GetTemperature() << "°C";
                hasSensor = true;
            }
            std::cout << std::endl;
        }
        
        // 显示风扇信息
        const auto& fanSpeeds = board.GetFanSpeeds();
        if (!fanSpeeds.empty()) {
            std::cout << "    风扇: ";
            for (size_t j = 0; j < fanSpeeds.size(); ++j) {
                if (j > 0) std::cout << ", ";
                std::cout << fanSpeeds[j].fanName << " " 
                          << std::fixed << std::setprecision(0) << fanSpeeds[j].speed << " RPM";
            }
            std::cout << std::endl;
        }
        
        // 如果有任务，显示任务详情
        if (!board.GetTasks().empty()) {
            for (const auto& task : board.GetTasks()) {
                std::cout << "    任务: ID=" << task.taskID 
                          << ", 状态=" << TaskStatusToString(task.taskStatus)
                          << ", 服务=" << task.serviceName << std::endl;
            }
        }
    }
    PrintSeparator();
}

void CliService::PrintAllChassisFullInfo() {
    auto allChassis = m_chassisRepo->GetAll();
    
    if (allChassis.empty()) {
        std::cout << "没有找到任何机箱" << std::endl;
        return;
    }
    
    std::cout << "\n=== 所有机箱完整信息 ===" << std::endl;
    
    for (const auto& chassis : allChassis) {
        if (!chassis) continue;
        PrintChassisDetail(chassis->GetChassisNumber());
        std::cout << std::endl;
    }
}

std::string CliService::BoardTypeToString(app::domain::BoardType type) const {
    switch (type) {
        case app::domain::BoardType::Computing:
            return "计算板卡";
        case app::domain::BoardType::Switch:
            return "交换板卡";
        case app::domain::BoardType::Power:
            return "电源板卡";
        default:
            return "未知";
    }
}

std::string CliService::BoardStatusToString(app::domain::BoardOperationalStatus status) const {
    switch (status) {
        case app::domain::BoardOperationalStatus::Unknown:
            return "未知";
        case app::domain::BoardOperationalStatus::Normal:
            return "正常";
        case app::domain::BoardOperationalStatus::Abnormal:
            return "异常";
        case app::domain::BoardOperationalStatus::Offline:
            return "离线";
        default:
            return "未知";
    }
}

void CliService::PrintSeparator() const {
    std::cout << std::string(80, '-') << std::endl;
}

void CliService::PrintAllStacksOverview() {
    auto allStacks = m_stackRepo->GetAll();
    
    if (allStacks.empty()) {
        std::cout << "没有找到任何业务链路" << std::endl;
        return;
    }
    
    std::cout << "\n=== 业务链路概览 ===" << std::endl;
    std::cout << "共 " << allStacks.size() << " 个业务链路" << std::endl;
    PrintSeparator();
    std::cout << std::left << std::setw(10) << "UUID" 
              << std::setw(20) << "名称"
              << std::setw(12) << "部署状态"
              << std::setw(12) << "运行状态"
              << std::setw(10) << "组件数" << std::endl;
    PrintSeparator();
    
    for (const auto& stack : allStacks) {
        if (!stack) continue;
        std::cout << std::left << std::setw(10) << stack->GetStackUUID().substr(0, 8)
                  << std::setw(20) << stack->GetStackName()
                  << std::setw(12) << (stack->GetDeployStatus() == 0 ? "未部署" : "已部署")
                  << std::setw(12) << (stack->GetRunningStatus() == 1 ? "正常运行" : 
                                       stack->GetRunningStatus() == 2 ? "异常运行" : "未运行")
                  << std::setw(10) << stack->GetAllServices().size() << std::endl;
    }
}

void CliService::PrintStackDetail(const std::string& stackUUID) {
    auto stack = m_stackRepo->FindByUUID(stackUUID);
    
    if (!stack) {
        std::cout << "未找到业务链路UUID: " << stackUUID << std::endl;
        return;
    }
    
    std::cout << "\n=== 业务链路详细信息: " << stack->GetStackName() << " (UUID: " 
              << stack->GetStackUUID() << ") ===" << std::endl;
    
    // 基本信息
    std::cout << "\n基本信息:" << std::endl;
    std::cout << "  部署状态: " << (stack->GetDeployStatus() == 0 ? "未部署" : "已部署") << std::endl;
    std::cout << "  运行状态: ";
    if (stack->GetRunningStatus() == 1) {
        std::cout << "正常运行";
    } else if (stack->GetRunningStatus() == 2) {
        std::cout << "异常运行";
    } else {
        std::cout << "未运行";
    }
    std::cout << std::endl;
    
    // 标签信息
    const auto& labels = stack->GetLabels();
    if (!labels.empty()) {
        std::cout << "\n标签信息:" << std::endl;
        for (const auto& label : labels) {
            std::cout << "  - " << label.stackLabelName << " (UUID: " << label.stackLabelUUID << ")" << std::endl;
        }
    }
    
    // 组件信息
    const auto& services = stack->GetAllServices();
    if (services.empty()) {
        std::cout << "\n组件: 无" << std::endl;
    } else {
        std::cout << "\n组件 (" << services.size() << "):" << std::endl;
        PrintSeparator();
        std::cout << std::left << std::setw(10) << "UUID" 
                  << std::setw(20) << "名称"
                  << std::setw(12) << "状态"
                  << std::setw(10) << "类型"
                  << std::setw(10) << "任务数" << std::endl;
        PrintSeparator();
        
        for (const auto& pair : services) {
            const auto& service = pair.second;
            const auto& tasks = service.GetAllTasks();
            
            std::cout << std::left << std::setw(10) << service.GetServiceUUID().substr(0, 8)
                      << std::setw(20) << service.GetServiceName()
                      << std::setw(12) << ServiceStatusToString(service.GetServiceStatus())
                      << std::setw(10) << (service.GetServiceType() == 0 ? "普通" : 
                                           service.GetServiceType() == 1 ? "公共组件" : "公共链路")
                      << std::setw(10) << tasks.size() << std::endl;
            
            // 显示任务详情
            if (!tasks.empty()) {
                for (const auto& taskPair : tasks) {
                    const auto& task = taskPair.second;
                    std::cout << "    └─ 任务ID: " << task.GetTaskID() 
                              << ", 状态: " << TaskStatusToString(task.GetTaskStatus())
                              << ", 板卡: " << task.GetBoardAddress() << std::endl;
                    
                    // 显示资源使用情况
                    const auto& resources = task.GetResources();
                    
                    // CPU信息
                    if (resources.cpuCores > 0 || resources.cpuUsed > 0 || resources.cpuUsage > 0) {
                        std::cout << "       CPU: ";
                        if (resources.cpuCores > 0) {
                            std::cout << std::fixed << std::setprecision(2) << resources.cpuUsed 
                                      << "/" << resources.cpuCores << "核";
                        }
                        if (resources.cpuUsage > 0) {
                            if (resources.cpuCores > 0) std::cout << ", ";
                            std::cout << std::setprecision(1) << resources.cpuUsage << "%";
                        }
                        std::cout << std::endl;
                    }
                    
                    // 内存信息
                    if (resources.memorySize > 0 || resources.memoryUsed > 0 || resources.memoryUsage > 0) {
                        std::cout << "       内存: ";
                        if (resources.memorySize > 0) {
                            std::cout << std::fixed << std::setprecision(2) << resources.memoryUsed 
                                      << "/" << resources.memorySize << "MB";
                        }
                        if (resources.memoryUsage > 0) {
                            if (resources.memorySize > 0) std::cout << ", ";
                            std::cout << std::setprecision(1) << resources.memoryUsage << "%";
                        }
                        std::cout << std::endl;
                    }
                    
                    // 网络信息
                    if (resources.netReceive > 0 || resources.netSent > 0) {
                        std::cout << "       网络: ";
                        if (resources.netReceive > 0) {
                            std::cout << "接收 " << std::fixed << std::setprecision(2) << resources.netReceive << "MB";
                        }
                        if (resources.netSent > 0) {
                            if (resources.netReceive > 0) std::cout << ", ";
                            std::cout << "发送 " << std::setprecision(2) << resources.netSent << "MB";
                        }
                        std::cout << std::endl;
                    }
                    
                    // GPU信息
                    if (resources.gpuMemUsed > 0) {
                        std::cout << "       GPU显存: " << std::fixed << std::setprecision(2) 
                                  << resources.gpuMemUsed << "MB" << std::endl;
                    }
                }
            }
        }
        PrintSeparator();
    }
}

void CliService::PrintAllStacksFullInfo() {
    auto allStacks = m_stackRepo->GetAll();
    
    if (allStacks.empty()) {
        std::cout << "没有找到任何业务链路" << std::endl;
        return;
    }
    
    std::cout << "\n=== 所有业务链路完整信息 ===" << std::endl;
    
    for (const auto& stack : allStacks) {
        if (!stack) continue;
        PrintStackDetail(stack->GetStackUUID());
        std::cout << std::endl;
    }
}

std::string CliService::ServiceStatusToString(int status) const {
    switch (status) {
        case 0:
            return "已停用";
        case 1:
            return "已启用";
        case 2:
            return "运行正常";
        case 3:
            return "运行异常";
        default:
            return "未知";
    }
}

std::string CliService::TaskStatusToString(int status) const {
    switch (status) {
        case 1:
            return "运行中";
        case 2:
            return "已完成";
        case 3:
            return "异常";
        case 0:
            return "其他";
        default:
            return "未知";
    }
}

void CliService::DeployStacks(const std::vector<std::string>& labels) {
    if (!m_apiClient) {
        std::cout << "错误: API客户端未初始化" << std::endl;
        return;
    }
    
    std::cout << "\n正在启动业务链路，标签: ";
    for (size_t i = 0; i < labels.size(); ++i) {
        std::cout << labels[i];
        if (i < labels.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    
    // 从配置读取账号密码
    std::string account = app::infrastructure::ConfigManager::GetString("/api/account", "admin");
    std::string password = app::infrastructure::ConfigManager::GetString("/api/password", "12q12w12ee");
    
    auto response = m_apiClient->DeployStacks(labels, account, password);
    
    std::cout << "\n部署结果:" << std::endl;
    PrintSeparator();
    
    if (!response.successStackInfos.empty()) {
        std::cout << "成功 (" << response.successStackInfos.size() << "):" << std::endl;
        for (const auto& stackInfo : response.successStackInfos) {
            std::cout << "  ✓ " << stackInfo.stackName << " (UUID: " << stackInfo.stackUUID << ")" << std::endl;
            if (!stackInfo.message.empty()) {
                std::cout << "    " << stackInfo.message << std::endl;
            }
        }
    }
    
    if (!response.failureStackInfos.empty()) {
        std::cout << "\n失败 (" << response.failureStackInfos.size() << "):" << std::endl;
        for (const auto& stackInfo : response.failureStackInfos) {
            std::cout << "  ✗ " << stackInfo.stackName << " (UUID: " << stackInfo.stackUUID << ")" << std::endl;
            if (!stackInfo.message.empty()) {
                std::cout << "    " << stackInfo.message << std::endl;
            }
        }
    }
    
    if (response.successStackInfos.empty() && response.failureStackInfos.empty()) {
        std::cout << "未找到匹配的业务链路" << std::endl;
    }
    
    PrintSeparator();
}

void CliService::UndeployStacks(const std::vector<std::string>& labels) {
    if (!m_apiClient) {
        std::cout << "错误: API客户端未初始化" << std::endl;
        return;
    }
    
    std::cout << "\n正在停止业务链路，标签: ";
    for (size_t i = 0; i < labels.size(); ++i) {
        std::cout << labels[i];
        if (i < labels.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    
    auto response = m_apiClient->UndeployStacks(labels);
    
    std::cout << "\n停用结果:" << std::endl;
    PrintSeparator();
    
    if (!response.successStackInfos.empty()) {
        std::cout << "成功 (" << response.successStackInfos.size() << "):" << std::endl;
        for (const auto& stackInfo : response.successStackInfos) {
            std::cout << "  ✓ " << stackInfo.stackName << " (UUID: " << stackInfo.stackUUID << ")" << std::endl;
            if (!stackInfo.message.empty()) {
                std::cout << "    " << stackInfo.message << std::endl;
            }
        }
    }
    
    if (!response.failureStackInfos.empty()) {
        std::cout << "\n失败 (" << response.failureStackInfos.size() << "):" << std::endl;
        for (const auto& stackInfo : response.failureStackInfos) {
            std::cout << "  ✗ " << stackInfo.stackName << " (UUID: " << stackInfo.stackUUID << ")" << std::endl;
            if (!stackInfo.message.empty()) {
                std::cout << "    " << stackInfo.message << std::endl;
            }
        }
    }
    
    if (response.successStackInfos.empty() && response.failureStackInfos.empty()) {
        std::cout << "未找到匹配的业务链路" << std::endl;
    }
    
    PrintSeparator();
}

} // namespace app::interfaces

