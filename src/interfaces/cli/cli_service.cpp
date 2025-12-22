#include "cli_service.h"
#include "src/domain/i_chassis_repository.h"
#include "src/domain/i_stack_repository.h"
#include "src/domain/value_objects.h"
#include "src/domain/chassis.h"
#include "src/domain/board.h"
#include "src/domain/stack.h"
#include "src/infrastructure/api_client/qyw_api_client.h"
#include "src/infrastructure/config/config_manager.h"
#include "src/infrastructure/controller/resource_controller.h"
#include <spdlog/spdlog.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <chrono>

namespace app::interfaces {

CliService::CliService(std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
                       std::shared_ptr<app::domain::IStackRepository> stackRepo,
                       std::shared_ptr<app::infrastructure::QywApiClient> apiClient)
    : m_chassisRepo(chassisRepo),
      m_stackRepo(stackRepo),
      m_apiClient(apiClient),
      m_chassisController(std::make_unique<ResourceController>()),
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
    spdlog::info("=== CLI服务已启动 ===");
    spdlog::info("输入 'help' 查看可用命令");
    
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
    
    spdlog::info("CLI服务已停止");
}

void CliService::ProcessCommand(const std::string& command) {
    if (command.empty()) {
        return;
    }
    
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "help" || cmd == "h" || cmd == "?") {
        PrintHelp();
    } else if (cmd == "quit" || cmd == "exit" || cmd == "q") {
        spdlog::info("退出CLI服务...");
        m_running = false;
    } else if (cmd == "chassis" || cmd == "c") {
        PrintAllChassisFullInfo();
    } else if (cmd == "stack" || cmd == "s") {
        PrintAllStacksFullInfo();
    } else if (cmd == "task" || cmd == "t") {
        // 解析 "task <机箱号> <槽位> <任务序号>" 命令
        int chassisNumber, slotNumber, taskIndex;
        iss >> chassisNumber >> slotNumber >> taskIndex;
        if (iss.fail() || chassisNumber <= 0 || slotNumber <= 0 || taskIndex <= 0) {
            spdlog::warn("命令格式错误，请使用: task <机箱号> <槽位> <任务序号>");
            spdlog::info("示例: task 1 3 1 或 t 1 3 1");
        } else {
            PrintTaskDetail(chassisNumber, slotNumber, taskIndex);
        }
    } else if (cmd == "deploy" || cmd == "d") {
        // 解析 "deploy <标签1> [标签2] ..." 命令
        std::vector<std::string> labels;
        std::string label;
        while (iss >> label) {
            labels.push_back(label);
        }
        if (labels.empty()) {
            spdlog::warn("请提供至少一个业务标签");
        } else {
            DeployStacks(labels);
        }
    } else if (cmd == "undeploy" || cmd == "u") {
        // 解析 "undeploy <标签1> [标签2] ..." 命令
        std::vector<std::string> labels;
        std::string label;
        while (iss >> label) {
            labels.push_back(label);
        }
        if (labels.empty()) {
            spdlog::warn("请提供至少一个业务标签");
        } else {
            UndeployStacks(labels);
        }
    } else if (cmd == "reset" || cmd == "resetall" || cmd == "r") {
        // 复位所有机箱的所有板卡
        ResetAllChassisBoards();
    } else if (cmd == "selfcheck" || cmd == "check" || cmd == "sc") {
        // 自检所有机箱的所有板卡
        SelfcheckAllChassisBoards();
    } else {
        spdlog::warn("未知命令: {}", command);
        spdlog::info("输入 'help' 或 'h' 查看可用命令");
    }
}

void CliService::PrintHelp() {
    std::cout << "\n=== 可用命令 ===" << std::endl;
    std::cout << "  chassis, c            - 显示所有机箱完整信息" << std::endl;
    std::cout << "  stack, s              - 显示所有业务链路完整信息" << std::endl;
    std::cout << "  task, t <机箱> <槽位> <序号>  - 显示指定任务的详细信息" << std::endl;
    std::cout << "  deploy, d <标签...>   - 启动指定标签的业务链路" << std::endl;
    std::cout << "  undeploy, u <标签...> - 停止指定标签的业务链路" << std::endl;
    std::cout << "  reset, resetall, r    - 复位所有机箱的所有板卡" << std::endl;
    std::cout << "  selfcheck, check, sc  - 自检所有机箱的所有板卡" << std::endl;
    std::cout << "  help, h, ?            - 显示此帮助信息" << std::endl;
    std::cout << "  quit, exit, q         - 退出CLI服务" << std::endl;
    std::cout << "\n示例:" << std::endl;
    std::cout << "  c                     - 显示所有机箱信息" << std::endl;
    std::cout << "  s                     - 显示所有业务链路信息" << std::endl;
    std::cout << "  t 1 3 1               - 显示机箱1槽位3的第1个任务" << std::endl;
    std::cout << "  d label1 label2       - 启动标签为label1和label2的业务链路" << std::endl;
    std::cout << "  u label1              - 停止标签为label1的业务链路" << std::endl;
    std::cout << "  reset                 - 复位所有机箱的所有板卡" << std::endl;
    std::cout << "  selfcheck             - 自检所有机箱的所有板卡（ping检查）" << std::endl;
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
        spdlog::warn("未找到机箱号: {}", chassisNumber);
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

void CliService::PrintTaskDetail(int chassisNumber, int slotNumber, int taskIndex) {
    // 查找机箱
    auto chassis = m_chassisRepo->FindByNumber(chassisNumber);
    if (!chassis) {
        spdlog::warn("未找到机箱号: {}", chassisNumber);
        return;
    }
    
    // 检查槽位号是否有效
    const auto& boards = chassis->GetAllBoards();
    if (slotNumber < 1 || static_cast<size_t>(slotNumber) > boards.size()) {
        spdlog::warn("槽位号 {} 无效，有效范围: 1-{}", slotNumber, boards.size());
        return;
    }
    
    // 获取板卡（槽位从1开始，数组索引从0开始）
    const auto& board = boards[slotNumber - 1];
    const auto& tasks = board.GetTasks();
    
    // 检查任务序号是否有效
    if (taskIndex < 1 || static_cast<size_t>(taskIndex) > tasks.size()) {
        spdlog::warn("任务序号 {} 无效，该板卡共有 {} 个任务", taskIndex, tasks.size());
        return;
    }
    
    // 获取任务（任务序号从1开始，数组索引从0开始）
    const auto& task = tasks[taskIndex - 1];
    
    std::cout << "\n=== 任务详细信息 ===" << std::endl;
    PrintSeparator();
    
    // 基本信息
    std::cout << "任务ID: " << task.taskID << std::endl;
    std::cout << "任务状态: " << TaskStatusToString(task.taskStatus) << std::endl;
    std::cout << "服务名称: " << task.serviceName << std::endl;
    std::cout << "服务UUID: " << task.serviceUUID << std::endl;
    std::cout << "业务链路名称: " << task.stackName << std::endl;
    std::cout << "业务链路UUID: " << task.stackUUID << std::endl;
    
    // 位置信息
    std::cout << "\n位置信息:" << std::endl;
    std::cout << "  机箱: " << chassis->GetChassisName() << " (机箱号: " << chassisNumber << ")" << std::endl;
    std::cout << "  板卡: " << board.GetBoardName() << " (槽位: " << slotNumber << ")" << std::endl;
    std::cout << "  板卡IP: " << board.GetAddress() << std::endl;
    std::cout << "  板卡类型: " << BoardTypeToString(board.GetBoardType()) << std::endl;
    
    // 从业务链路中查找任务以获取资源信息
    auto stack = m_stackRepo->FindByUUID(task.stackUUID);
    if (stack) {
        const auto& services = stack->GetAllServices();
        auto serviceIt = services.find(task.serviceUUID);
        if (serviceIt != services.end()) {
            const auto& service = serviceIt->second;
            const auto& serviceTasks = service.GetAllTasks();
            auto taskIt = serviceTasks.find(task.taskID);
            if (taskIt != serviceTasks.end()) {
                const auto& taskWithResources = taskIt->second;
                const auto& resources = taskWithResources.GetResources();
                
                std::cout << "\n资源使用情况:" << std::endl;
                
                // CPU信息
                if (resources.cpuCores > 0 || resources.cpuUsed > 0 || resources.cpuUsage > 0) {
                    std::cout << "  CPU:" << std::endl;
                    if (resources.cpuCores > 0) {
                        std::cout << "    总量: " << std::fixed << std::setprecision(2) 
                                  << resources.cpuCores << " 核" << std::endl;
                    }
                    if (resources.cpuUsed > 0) {
                        std::cout << "    使用量: " << std::fixed << std::setprecision(2) 
                                  << resources.cpuUsed << " 核" << std::endl;
                    }
                    if (resources.cpuUsage > 0) {
                        std::cout << "    使用率: " << std::fixed << std::setprecision(1) 
                                  << resources.cpuUsage << "%" << std::endl;
                    }
                }
                
                // 内存信息
                if (resources.memorySize > 0 || resources.memoryUsed > 0 || resources.memoryUsage > 0) {
                    std::cout << "  内存:" << std::endl;
                    if (resources.memorySize > 0) {
                        std::cout << "    总量: " << std::fixed << std::setprecision(2) 
                                  << resources.memorySize << " MB" << std::endl;
                    }
                    if (resources.memoryUsed > 0) {
                        std::cout << "    使用量: " << std::fixed << std::setprecision(2) 
                                  << resources.memoryUsed << " MB" << std::endl;
                    }
                    if (resources.memoryUsage > 0) {
                        std::cout << "    使用率: " << std::fixed << std::setprecision(1) 
                                  << resources.memoryUsage << "%" << std::endl;
                    }
                }
                
                // 网络信息
                if (resources.netReceive > 0 || resources.netSent > 0) {
                    std::cout << "  网络:" << std::endl;
                    if (resources.netReceive > 0) {
                        std::cout << "    接收流量: " << std::fixed << std::setprecision(2) 
                                  << resources.netReceive << " MB/s" << std::endl;
                    }
                    if (resources.netSent > 0) {
                        std::cout << "    发送流量: " << std::fixed << std::setprecision(2) 
                                  << resources.netSent << " MB/s" << std::endl;
                    }
                }
                
                // GPU信息
                if (resources.gpuMemUsed > 0) {
                    std::cout << "  GPU显存:" << std::endl;
                    std::cout << "    使用量: " << std::fixed << std::setprecision(2) 
                              << resources.gpuMemUsed << " GB" << std::endl;
                }
            } else {
                std::cout << "\n资源使用情况: 未找到资源信息" << std::endl;
            }
        } else {
            std::cout << "\n资源使用情况: 未找到服务信息" << std::endl;
        }
    } else {
        std::cout << "\n资源使用情况: 未找到业务链路信息" << std::endl;
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
    std::cout << "共 " << allChassis.size() << " 个机箱" << std::endl;
    PrintSeparator();
    
    // 表头
    std::cout << std::left 
              << std::setw(8) << "机箱号"
              << std::setw(20) << "机箱名称"
              << std::setw(6) << "槽位"
              << std::setw(18) << "IP地址"
              << std::setw(28) << "板卡类型"
              << std::setw(10) << "状态"
              << std::setw(8) << "任务数"
              << std::setw(12) << "电压(V)"
              << std::setw(10) << "电流(A)"
              << std::setw(10) << "温度(°C)"
              << std::endl;
    PrintSeparator();
    
    // 遍历所有机箱和板卡
    for (const auto& chassis : allChassis) {
        if (!chassis) continue;
        
        const auto& boards = chassis->GetAllBoards();
        for (size_t i = 0; i < boards.size(); ++i) {
            const auto& board = boards[i];
            int slotNumber = static_cast<int>(i + 1);
            
            std::cout << std::left 
                      << std::setw(8) << chassis->GetChassisNumber()
                      << std::setw(20) << chassis->GetChassisName()
                      << std::setw(6) << slotNumber
                      << std::setw(18) << board.GetAddress()
                      << std::setw(28) << BoardTypeToString(board.GetBoardType())
                      << std::setw(10) << BoardStatusToString(board.GetStatus())
                      << std::setw(8) << board.GetTasks().size();
            
            // 传感器信息
            if (board.GetVoltage() > 0) {
                std::cout << std::fixed << std::setprecision(2) << std::setw(12) << board.GetVoltage();
            } else {
                std::cout << std::setw(12) << "-";
            }
            
            if (board.GetCurrent() > 0) {
                std::cout << std::fixed << std::setprecision(2) << std::setw(10) << board.GetCurrent();
            } else {
                std::cout << std::setw(10) << "-";
            }
            
            if (board.GetTemperature() > 0) {
                std::cout << std::fixed << std::setprecision(1) << std::setw(10) << board.GetTemperature();
            } else {
                std::cout << std::setw(10) << "-";
            }
            
            std::cout << std::endl;
        }
    }
    
    PrintSeparator();
    
    // 显示任务详情（如果有）
    bool hasTasks = false;
    for (const auto& chassis : allChassis) {
        if (!chassis) continue;
        const auto& boards = chassis->GetAllBoards();
        for (size_t i = 0; i < boards.size(); ++i) {
            const auto& board = boards[i];
            if (!board.GetTasks().empty()) {
                if (!hasTasks) {
                    std::cout << "\n任务详情:" << std::endl;
                    PrintSeparator();
                    hasTasks = true;
                }
                int slotNumber = static_cast<int>(i + 1);
                for (const auto& task : board.GetTasks()) {
                    std::cout << "机箱" << chassis->GetChassisNumber() 
                              << " 槽位" << slotNumber 
                              << " | 任务ID: " << task.taskID 
                              << " | 状态: " << TaskStatusToString(task.taskStatus)
                              << " | 服务: " << task.serviceName
                              << " | 业务链路: " << task.stackName << std::endl;
                }
            }
        }
    }
    
    if (hasTasks) {
        PrintSeparator();
    }
}

std::string CliService::BoardTypeToString(app::domain::BoardType type) const {
    switch (type) {
        case app::domain::BoardType::Other:
            return "其他";
        case app::domain::BoardType::CPUGeneralComputingA:
            return "CPU通用计算模块A型";
        case app::domain::BoardType::CPUGeneralComputingB:
            return "CPU通用计算模块B型";
        case app::domain::BoardType::GPUIHighPerformanceComputing:
            return "GPU I型高性能计算模块";
        case app::domain::BoardType::GPUIIHighPerformanceComputing:
            return "GPU II型高性能计算模块";
        case app::domain::BoardType::IntegratedComputingA:
            return "综合计算模块A型";
        case app::domain::BoardType::IntegratedComputingB:
            return "综合计算模块B型";
        case app::domain::BoardType::SRIO:
            return "SRIO模块";
        case app::domain::BoardType::EthernetSwitch:
            return "以太网交换模块";
        case app::domain::BoardType::Cache:
            return "缓存模块";
        case app::domain::BoardType::Power:
            return "电源模块";
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
        spdlog::warn("未找到业务链路UUID: {}", stackUUID);
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
            std::cout << "  - " << label << std::endl;
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
    std::cout << "共 " << allStacks.size() << " 个业务链路" << std::endl;
    PrintSeparator();
    
    // 表头
    std::cout << std::left 
              << std::setw(12) << "UUID"
              << std::setw(20) << "名称"
              << std::setw(12) << "部署状态"
              << std::setw(12) << "运行状态"
              << std::setw(20) << "标签"
              << std::setw(10) << "组件数"
              << std::setw(10) << "任务数"
              << std::endl;
    PrintSeparator();
    
    // 遍历所有业务链路
    for (const auto& stack : allStacks) {
        if (!stack) continue;
        
        // UUID（缩短显示）
        std::string uuid_short = stack->GetStackUUID().length() > 12 
            ? stack->GetStackUUID().substr(0, 12) + "..." 
            : stack->GetStackUUID();
        
        // 部署状态
        std::string deploy_status = (stack->GetDeployStatus() == 0 ? "未部署" : "已部署");
        
        // 运行状态
        std::string running_status;
        if (stack->GetRunningStatus() == 1) {
            running_status = "正常运行";
        } else if (stack->GetRunningStatus() == 2) {
            running_status = "异常运行";
        } else if (stack->GetRunningStatus() == 3) {
            running_status = "启用中";
        } else {
            running_status = "未运行";
        }
        
        // 标签（可能有多个，用逗号分隔）
        std::string labels_str;
        const auto& labels = stack->GetLabels();
        if (!labels.empty()) {
            for (size_t i = 0; i < labels.size(); ++i) {
                if (i > 0) labels_str += ", ";
                labels_str += labels[i];
            }
        } else {
            labels_str = "-";
        }
        // 如果标签太长，截断
        if (labels_str.length() > 18) {
            labels_str = labels_str.substr(0, 15) + "...";
        }
        
        // 组件数和任务数
        const auto& services = stack->GetAllServices();
        size_t total_tasks = 0;
        for (const auto& pair : services) {
            total_tasks += pair.second.GetAllTasks().size();
        }
        
        std::cout << std::left 
                  << std::setw(12) << uuid_short
                  << std::setw(20) << stack->GetStackName()
                  << std::setw(12) << deploy_status
                  << std::setw(12) << running_status
                  << std::setw(20) << labels_str
                  << std::setw(10) << services.size()
                  << std::setw(10) << total_tasks
                  << std::endl;
    }
    
    PrintSeparator();
    
    // 显示组件和任务详情
    std::cout << "\n组件和任务详情:" << std::endl;
    PrintSeparator();
    
    for (const auto& stack : allStacks) {
        if (!stack) continue;
        
        std::cout << "\n业务链路: " << stack->GetStackName() 
                  << " (UUID: " << stack->GetStackUUID().substr(0, 8) << "...)" << std::endl;
        
        const auto& services = stack->GetAllServices();
        if (services.empty()) {
            std::cout << "  组件: 无" << std::endl;
        } else {
            // 组件表格
            std::cout << "  组件列表:" << std::endl;
            std::cout << "  " << std::left 
                      << std::setw(10) << "UUID"
                      << std::setw(20) << "名称"
                      << std::setw(12) << "状态"
                      << std::setw(10) << "类型"
                      << std::setw(10) << "任务数"
                      << std::endl;
            std::cout << "  " << std::string(62, '-') << std::endl;
            
            for (const auto& pair : services) {
                const auto& service = pair.second;
                const auto& tasks = service.GetAllTasks();
                
                std::cout << "  " << std::left 
                          << std::setw(10) << service.GetServiceUUID().substr(0, 8)
                          << std::setw(20) << service.GetServiceName()
                          << std::setw(12) << ServiceStatusToString(service.GetServiceStatus())
                          << std::setw(10) << (service.GetServiceType() == 0 ? "普通" : 
                                               service.GetServiceType() == 1 ? "公共组件" : "公共链路")
                          << std::setw(10) << tasks.size()
                          << std::endl;
                
                // 任务详情
                if (!tasks.empty()) {
                    for (const auto& taskPair : tasks) {
                        const auto& task = taskPair.second;
                        const auto& resources = task.GetResources();
                        
                        std::cout << "    └─ 任务ID: " << task.GetTaskID() 
                                  << ", 状态: " << TaskStatusToString(task.GetTaskStatus())
                                  << ", 板卡: " << task.GetBoardAddress() << std::endl;
                        
                        // 资源使用情况（详细显示）
                        bool hasResources = false;
                        
                        // CPU信息
                        if (resources.cpuCores > 0 || resources.cpuUsed > 0 || resources.cpuUsage > 0) {
                            std::cout << "       CPU: ";
                            if (resources.cpuCores > 0 && resources.cpuUsed > 0) {
                                std::cout << std::fixed << std::setprecision(2) 
                                          << resources.cpuUsed << "/" << resources.cpuCores << "核";
                                if (resources.cpuUsage > 0) {
                                    std::cout << ", ";
                                }
                            }
                            if (resources.cpuUsage > 0) {
                                std::cout << std::setprecision(1) << resources.cpuUsage*100.0f << "%";
                            }
                            std::cout << std::endl;
                            hasResources = true;
                        }
                        
                        // 内存信息
                        if (resources.memorySize > 0 || resources.memoryUsed > 0 || resources.memoryUsage > 0) {
                            std::cout << "       内存: ";
                            if (resources.memorySize > 0 && resources.memoryUsed > 0) {
                                std::cout << std::fixed << std::setprecision(2) 
                                          << resources.memoryUsed << "/" << resources.memorySize << "MB";
                                if (resources.memoryUsage > 0) {
                                    std::cout << ", ";
                                }
                            }
                            if (resources.memoryUsage > 0) {
                                std::cout << std::setprecision(1) << resources.memoryUsage*100.0f << "%";
                            }
                            std::cout << std::endl;
                            hasResources = true;
                        }
                        
                        // 网络信息
                        if (resources.netReceive > 0 || resources.netSent > 0) {
                            std::cout << "       网络: ";
                            if (resources.netReceive > 0) {
                                std::cout << "接收 " << std::fixed << std::setprecision(2) 
                                          << resources.netReceive << "MB/s";
                            }
                            if (resources.netSent > 0) {
                                if (resources.netReceive > 0) std::cout << ", ";
                                std::cout << "发送 " << std::setprecision(2) 
                                          << resources.netSent << "MB/s";
                            }
                            std::cout << std::endl;
                            hasResources = true;
                        }
                        
                        // GPU信息
                        if (resources.gpuMemUsed > 0) {
                            std::cout << "       GPU显存: " << std::fixed << std::setprecision(2) 
                                      << resources.gpuMemUsed << "GB" << std::endl;
                            hasResources = true;
                        }
                    }
                }
            }
        }
    }
    
    PrintSeparator();
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
        spdlog::error("错误: API客户端未初始化");
        return;
    }
    
    std::string labelsStr;
    for (size_t i = 0; i < labels.size(); ++i) {
        labelsStr += labels[i];
        if (i < labels.size() - 1) labelsStr += ", ";
    }
    spdlog::info("正在启动业务链路，标签: {}", labelsStr);
    
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
        spdlog::warn("未找到匹配的业务链路");
    }
    
    PrintSeparator();
}

void CliService::UndeployStacks(const std::vector<std::string>& labels) {
    if (!m_apiClient) {
        spdlog::error("错误: API客户端未初始化");
        return;
    }
    
    std::string labelsStr;
    for (size_t i = 0; i < labels.size(); ++i) {
        labelsStr += labels[i];
        if (i < labels.size() - 1) labelsStr += ", ";
    }
    spdlog::info("正在停止业务链路，标签: {}", labelsStr);
    
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
        spdlog::warn("未找到匹配的业务链路");
    }
    
    PrintSeparator();
}

void CliService::ResetAllChassisBoards() {
    if (!m_chassisController) {
        spdlog::error("错误: 机箱控制器未初始化");
        return;
    }

    spdlog::info("开始复位所有机箱的所有板卡...");
    
    // 获取所有机箱
    auto allChassis = m_chassisRepo->GetAll();
    
    if (allChassis.empty()) {
        spdlog::warn("未找到任何机箱");
        return;
    }
    
    std::cout << "\n复位结果:" << std::endl;
    PrintSeparator();
    
    int totalSuccess = 0;
    int totalFailed = 0;
    
    // 遍历所有机箱（最多9个）
    for (size_t chassisIdx = 0; chassisIdx < allChassis.size() && chassisIdx < 9; ++chassisIdx) {
        const auto& chassis = allChassis[chassisIdx];
        int chassisNumber = chassis->GetChassisNumber();
        
        // 获取机箱IP地址（使用该机箱中boardType是EthernetSwitch（即10）的第一个板卡作为机箱IP）
        std::string chassisIp;
        const auto& boards = chassis->GetAllBoards();
        
        // 查找第一个EthernetSwitch类型的板卡
        bool foundEthernetSwitch = false;
        for (const auto& board : boards) {
            if (board.GetBoardType() == app::domain::BoardType::EthernetSwitch && 
                !board.GetAddress().empty()) {
                chassisIp = board.GetAddress();
                foundEthernetSwitch = true;
                break;
            }
        }
        
        // 如果没有找到EthernetSwitch板卡，使用默认格式
        if (!foundEthernetSwitch) {
            chassisIp = "192.168." + std::to_string(chassisNumber * 2) + ".180";
        }
        
        // 收集该机箱所有板卡的槽位号（1-12）
        std::vector<int> slotNumbers;
        for (int boardIdx = 0; boardIdx < 12 && boardIdx < static_cast<int>(boards.size()); ++boardIdx) {
            slotNumbers.push_back(boardIdx + 1);  // 槽位号从1开始
        }
        
        if (slotNumbers.empty()) {
            spdlog::warn("机箱{} 没有板卡需要复位", chassisNumber);
            continue;
        }
        
        // 构建槽位字符串用于显示
        std::string slotStr;
        for (size_t i = 0; i < slotNumbers.size(); ++i) {
            slotStr += std::to_string(slotNumbers[i]);
            if (i < slotNumbers.size() - 1) {
                slotStr += ",";
            }
        }
        
        std::cout << "机箱" << chassisNumber << " (IP: " << chassisIp << ", 板卡: " << slotStr << "): ";
        std::cout.flush();
        
        // 调用复位接口
        uint32_t requestId = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
        auto resetResult = m_chassisController->resetBoard(chassisIp, slotNumbers, requestId);
        
        // 统计结果
        int successCount = 0;
        int failedCount = 0;
        
        if (resetResult.result == ResourceController::OperationResult::SUCCESS ||
            resetResult.result == ResourceController::OperationResult::PARTIAL_SUCCESS) {
            // 统计成功和失败的板卡数
            for (const auto& slotResult : resetResult.slot_results) {
                if (slotResult.status == ResourceController::SlotStatus::NO_OPERATION_OR_SUCCESS) {
                    successCount++;
                } else {
                    failedCount++;
                }
            }
            
            if (failedCount == 0) {
                std::cout << "✓ 全部成功 (" << successCount << "个板卡)" << std::endl;
                totalSuccess += successCount;
            } else {
                std::cout << "⚠ 部分成功 (成功: " << successCount << ", 失败: " << failedCount << ")" << std::endl;
                totalSuccess += successCount;
                totalFailed += failedCount;
            }
        } else {
            std::cout << "✗ 全部失败 (" << slotNumbers.size() << "个板卡)" << std::endl;
            std::cout << "  错误: " << resetResult.message << std::endl;
            totalFailed += slotNumbers.size();
        }
    }
    
    PrintSeparator();
    std::cout << "总计: 成功 " << totalSuccess << " 个板卡, 失败 " << totalFailed << " 个板卡" << std::endl;
    PrintSeparator();
    
    spdlog::info("复位操作完成: 成功 {} 个板卡, 失败 {} 个板卡", totalSuccess, totalFailed);
}

void CliService::SelfcheckAllChassisBoards() {
    spdlog::info("开始自检所有机箱的所有板卡...");
    
    // 获取所有机箱
    auto allChassis = m_chassisRepo->GetAll();
    
    if (allChassis.empty()) {
        spdlog::warn("未找到任何机箱");
        return;
    }
    
    std::cout << "\n自检结果:" << std::endl;
    PrintSeparator();
    
    int totalSuccess = 0;
    int totalFailed = 0;
    
    // 遍历所有机箱（最多9个）
    for (size_t chassisIdx = 0; chassisIdx < allChassis.size() && chassisIdx < 9; ++chassisIdx) {
        const auto& chassis = allChassis[chassisIdx];
        int chassisNumber = chassis->GetChassisNumber();
        
        const auto& boards = chassis->GetAllBoards();
        
        std::cout << "机箱" << chassisNumber << ":" << std::endl;
        
        int chassisSuccess = 0;
        int chassisFailed = 0;
        
        // 遍历12块板卡（或实际板卡数量）
        for (int boardIdx = 0; boardIdx < 12 && boardIdx < static_cast<int>(boards.size()); ++boardIdx) {
            const auto& board = boards[boardIdx];
            int slotNumber = boardIdx + 1;
            std::string boardIp = board.GetAddress();
            
            std::cout << "  板卡" << slotNumber << " (IP: " << boardIp << "): ";
            std::cout.flush();
            
            if (boardIp.empty()) {
                std::cout << "✗ IP地址为空" << std::endl;
                chassisFailed++;
                totalFailed++;
                continue;
            }
            
            // 调用自检接口（ping检查）
            bool checkResult = ResourceController::SelfcheckBoard(boardIp);
            
            if (checkResult) {
                std::cout << "✓ 自检成功（ping通）" << std::endl;
                chassisSuccess++;
                totalSuccess++;
            } else {
                std::cout << "✗ 自检失败（ping不通）" << std::endl;
                chassisFailed++;
                totalFailed++;
            }
        }
        
        // 显示机箱统计
        std::cout << "  机箱" << chassisNumber << " 总计: 成功 " << chassisSuccess 
                  << " 个板卡, 失败 " << chassisFailed << " 个板卡" << std::endl;
        std::cout << std::endl;
    }
    
    PrintSeparator();
    std::cout << "总计: 成功 " << totalSuccess << " 个板卡, 失败 " << totalFailed << " 个板卡" << std::endl;
    PrintSeparator();
    
    spdlog::info("自检操作完成: 成功 {} 个板卡, 失败 {} 个板卡", totalSuccess, totalFailed);
}

} // namespace app::interfaces

