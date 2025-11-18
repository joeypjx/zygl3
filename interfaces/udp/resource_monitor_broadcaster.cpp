#include "resource_monitor_broadcaster.h"
#include "domain/chassis.h"
#include "domain/board.h"
#include "domain/value_objects.h"
#include "../../infrastructure/config/config_manager.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>

namespace app::interfaces {

// ResourceMonitorBroadcaster 实现
ResourceMonitorBroadcaster::ResourceMonitorBroadcaster(
    std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
    std::shared_ptr<app::domain::IStackRepository> stackRepo,
    std::shared_ptr<app::infrastructure::QywApiClient> apiClient,
    const std::string& multicastGroup,
    uint16_t port)
    : m_chassisRepo(chassisRepo)
    , m_stackRepo(stackRepo)
    , m_apiClient(apiClient)
    , m_multicastGroup(multicastGroup)
    , m_port(port)
    , m_socket(-1)
    , m_running(false)
    , m_nextResponseId(0)
    , m_chassisController(std::make_unique<ResourceController>()) {

    // 初始化socket
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        std::cerr << "创建UDP socket失败" << std::endl;
        return;
    }

    // 启用广播
    int broadcast = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
}

ResourceMonitorBroadcaster::~ResourceMonitorBroadcaster() {
    Stop();
    if (m_socket >= 0) {
        close(m_socket);
    }
}

void ResourceMonitorBroadcaster::Start() {
    m_running = true;
    std::cout << "资源监控广播器已启动 (组播地址: " << m_multicastGroup 
              << ":" << m_port << ")" << std::endl;
}

void ResourceMonitorBroadcaster::SetCommand(uint16_t resourceMonitorResp, uint16_t taskQueryResp, 
                                             uint16_t taskStartResp, uint16_t taskStopResp, uint16_t chassisResetResp, uint16_t chassisSelfCheckResp, uint16_t faultReport) {
    m_cmdResourceMonitorResp = resourceMonitorResp;
    m_cmdTaskQueryResp = taskQueryResp;
    m_cmdTaskStartResp = taskStartResp;
    m_cmdTaskStopResp = taskStopResp;
    m_cmdChassisResetResp = chassisResetResp;
    m_cmdChassisSelfCheckResp = chassisSelfCheckResp;
    m_cmdFaultReport = faultReport;
}

void ResourceMonitorBroadcaster::Stop() {
    m_running = false;
    std::cout << "资源监控广播器已停止" << std::endl;
}

bool ResourceMonitorBroadcaster::SendResponse(uint32_t requestId) {
    if (m_socket < 0) {
        return false;
    }
    
    // 构建响应报文
    ResourceMonitorResponse response;
    memset(&response, 0, sizeof(response));
    
    // 设置头部（22字节）
    memset(response.header, 0, 22);
    
    // 设置命令码
    response.command = m_cmdResourceMonitorResp;
    
    // 设置响应ID
    response.responseId = m_nextResponseId++;
    
    // 构建板卡状态和任务状态数据
    BuildResponseData(response);
    
    // 设置发送地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    inet_pton(AF_INET, m_multicastGroup.c_str(), &addr.sin_addr);
    
    // 发送组播数据包
    int result = sendto(m_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&addr, sizeof(addr));
    
    if (result > 0) {
        std::cout << "发送资源监控响应: ID=" << response.responseId 
                  << " (原始请求ID=" << requestId << ")" << std::endl;
        return true;
    }
    
    std::cerr << "发送响应失败: " << strerror(errno) << std::endl;
    return false;
}

void ResourceMonitorBroadcaster::BuildResponseData(ResourceMonitorResponse& response) {
    // 获取所有机箱
    auto allChassis = m_chassisRepo->GetAll();
    
    // 遍历9个机箱（协议要求12块板卡，但我们有14块，取前12块）
    for (size_t chassisIdx = 0; chassisIdx < allChassis.size() && chassisIdx < 9; ++chassisIdx) {
        const auto& chassis = allChassis[chassisIdx];
        int chassisNumber = chassis->GetChassisNumber();
        
        // 映射板卡状态
        MapBoardStatusToArray(response.boardStatus + chassisIdx * 12, chassisNumber);
        
        // 映射任务状态
        MapTaskStatusToArray(response.taskStatus + chassisIdx * 12 * 8, chassisNumber);
    }
}

void ResourceMonitorBroadcaster::MapBoardStatusToArray(uint8_t* array, int chassisNumber) {
    auto chassis = m_chassisRepo->FindByNumber(chassisNumber);
    if (!chassis) {
        return;
    }
    
    const auto& boards = chassis->GetAllBoards();
    
    // 协议要求12块板卡，我们取前12块（槽位1-12）
    for (size_t i = 0; i < 12 && i < boards.size(); ++i) {
        auto boardStatus = boards[i].GetStatus();
        
        // 1=板卡正常，0=板卡异常
        if (boardStatus == app::domain::BoardOperationalStatus::Normal) {
            array[i] = 1;
        } else {
            array[i] = 0;
        }
    }
}

void ResourceMonitorBroadcaster::MapTaskStatusToArray(uint8_t* array, int chassisNumber) {
    auto chassis = m_chassisRepo->FindByNumber(chassisNumber);
    if (!chassis) {
        return;
    }
    
    const auto& boards = chassis->GetAllBoards();
    
    // 9个机箱×12块板卡×8个任务 = 864字节
    for (size_t boardIdx = 0; boardIdx < 12 && boardIdx < boards.size(); ++boardIdx) {
        const auto& board = boards[boardIdx];
        const auto& tasks = board.GetTasks();
        
        for (size_t taskIdx = 0; taskIdx < 8; ++taskIdx) {
            if (taskIdx < tasks.size()) {
                // 1=任务正常，2=任务异常
                // taskStatus: 1-运行中，2-已完成，3-异常，0-其他
                if (tasks[taskIdx].taskStatus == 1) {  // 运行中表示正常
                    array[boardIdx * 8 + taskIdx] = 1;
                } else {
                    array[boardIdx * 8 + taskIdx] = 2;
                }
            } else {
                // 没有任务，设置为0
                array[boardIdx * 8 + taskIdx] = 0;
            }
        }
    }
}

bool ResourceMonitorBroadcaster::SendTaskQueryResponse(const TaskQueryRequest& request) {
    if (m_socket < 0) {
        return false;
    }

    // 构建响应报文
    TaskQueryResponse response;
    memset(&response, 0, sizeof(response));

    // 设置头部（22字节）
    memset(response.header, 0, 22);

    // 设置命令码
    response.command = m_cmdTaskQueryResp;

    // 设置响应ID
    response.responseId = m_nextResponseId++;

    // 构建任务查询响应数据
    BuildTaskQueryResponse(response, request);

    // 设置发送地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    inet_pton(AF_INET, m_multicastGroup.c_str(), &addr.sin_addr);

    // 发送组播数据包
    int result = sendto(m_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&addr, sizeof(addr));

    if (result > 0) {
        std::cout << "发送任务查询响应: 机箱" << request.chassisNumber
                  << " 板卡" << request.boardNumber
                  << " 任务" << request.taskIndex
                  << " (响应ID=" << response.responseId << ")" << std::endl;
        return true;
    }

    std::cerr << "发送任务查询响应失败: " << strerror(errno) << std::endl;
    return false;
}

void ResourceMonitorBroadcaster::BuildTaskQueryResponse(TaskQueryResponse& response, const TaskQueryRequest& request) {
    // 1. 根据机箱号和板卡号获取板卡信息
    auto chassis = m_chassisRepo->FindByNumber(request.chassisNumber);
    if (!chassis) {
        std::cerr << "未找到机箱: " << request.chassisNumber << std::endl;
        response.taskStatus = 1;  // 异常
        return;
    }

    auto* board = chassis->GetBoardBySlot(request.boardNumber);
    if (!board) {
        std::cerr << "未找到板卡: 机箱" << request.chassisNumber
                  << " 槽位" << request.boardNumber << std::endl;
        response.taskStatus = 1;  // 异常
        return;
    }

    // 2. 从板卡获取任务列表，根据任务序号获取taskID
    const auto& tasks = board->GetTasks();
    if (request.taskIndex >= tasks.size()) {
        std::cerr << "任务序号超出范围: " << request.taskIndex << std::endl;
        response.taskStatus = 1;  // 异常
        return;
    }

    const auto& taskInfo = tasks[request.taskIndex];
    std::string taskID = taskInfo.taskID;

    // 3. 通过stackRepo查找任务的资源使用情况
    auto resourceUsage = m_stackRepo->GetTaskResources(taskID);
    if (!resourceUsage) {
        std::cerr << "未找到任务资源信息: taskID=" << taskID << std::endl;
        response.taskStatus = 1;  // 异常
        return;
    }

    // 4. 填充响应数据
    response.taskStatus = 0;  // 正常

    // 将taskID字符串转换为uint32 (这里简化处理，实际可能需要更复杂的转换)
    try {
        response.taskId = std::stoul(taskID);
    } catch (...) {
        // 如果taskID不是纯数字，使用哈希值
        response.taskId = std::hash<std::string>{}(taskID);
    }

    response.workMode = 0;  // 工作模式（根据实际情况设置）

    // 板卡IP转换
    response.boardIp = IpStringToUint32(board->GetAddress());

    // CPU使用率转换为千分比 (0-1000)
    response.cpuUsage = static_cast<uint16_t>(resourceUsage->cpuUsage * 1000);

    // 内存使用率转换为千分比
    response.memoryUsage = static_cast<uint32_t>(resourceUsage->memoryUsage * 1000);

    std::cout << "任务查询成功: taskID=" << taskID
              << " CPU=" << response.cpuUsage / 10.0 << "%"
              << " MEM=" << response.memoryUsage / 10.0 << "%" << std::endl;
}

uint32_t ResourceMonitorBroadcaster::IpStringToUint32(const std::string& ipStr) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ipStr.c_str(), &addr) == 1) {
        return ntohl(addr.s_addr);
    }
    return 0;
}

bool ResourceMonitorBroadcaster::HandleTaskStartRequest(const TaskStartRequest& request) {
    if (m_socket < 0) {
        return false;
    }

    // 构建响应报文
    TaskStartResponse response;
    memset(&response, 0, sizeof(response));

    // 设置头部
    memset(response.header, 0, 22);

    // 设置命令码
    response.command = m_cmdTaskStartResp;

    // 设置响应ID
    response.responseId = m_nextResponseId++;

    // 构建启动响应数据
    BuildTaskStartResponse(response, request);

    // 设置发送地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    inet_pton(AF_INET, m_multicastGroup.c_str(), &addr.sin_addr);

    // 发送组播数据包
    int result = sendto(m_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&addr, sizeof(addr));

    if (result > 0) {
        std::cout << "发送任务启动响应: 工作模式=" << request.workMode
                  << " (响应ID=" << response.responseId << ")" << std::endl;
        return true;
    }

    std::cerr << "发送任务启动响应失败: " << strerror(errno) << std::endl;
    return false;
}

bool ResourceMonitorBroadcaster::HandleTaskStopRequest(const TaskStopRequest& request) {
    if (m_socket < 0) {
        return false;
    }

    // 构建响应报文
    TaskStopResponse response;
    memset(&response, 0, sizeof(response));

    // 设置头部
    memset(response.header, 0, 22);

    // 设置命令码
    response.command = m_cmdTaskStopResp;

    // 设置响应ID
    response.responseId = m_nextResponseId++;

    // 构建停止响应数据
    BuildTaskStopResponse(response, request);

    // 设置发送地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    inet_pton(AF_INET, m_multicastGroup.c_str(), &addr.sin_addr);

    // 发送组播数据包
    int result = sendto(m_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&addr, sizeof(addr));

    if (result > 0) {
        std::cout << "发送任务停止响应: (响应ID=" << response.responseId << ")" << std::endl;
        return true;
    }

    std::cerr << "发送任务停止响应失败: " << strerror(errno) << std::endl;
    return false;
}

void ResourceMonitorBroadcaster::BuildTaskStartResponse(TaskStartResponse& response, const TaskStartRequest& request) {
    // 将工作模式转换为标签名称
    std::string label = WorkModeToLabel(request.workMode);

    // 检查是否需要先停止当前任务
    bool shouldStopCurrent = (request.startStrategy == 0);

    // 获取当前正在运行的任务标签
    std::string currentLabel;
    {
        std::lock_guard<std::mutex> lock(m_labelMutex);
        currentLabel = m_currentRunningLabel;
    }

    // 如果策略是先停止当前任务，检查是否有正在运行的任务
    if (shouldStopCurrent && !currentLabel.empty()) {
        std::cout << "开始停止当前任务: " << currentLabel << std::endl;
        
        // 调用API停止当前任务
        std::vector<std::string> labels;
        labels.push_back(currentLabel);
        auto result = m_apiClient->UndeployStacks(labels);

        // 更新当前运行标签
        {
            std::lock_guard<std::mutex> lock(m_labelMutex);
            m_currentRunningLabel = "";
        }

        if (!result.failureStackInfos.empty()) {
            std::cerr << "停止当前任务失败" << std::endl;
            response.startResult = 1;  // 失败
            strncpy(response.resultDesc, "停止当前任务失败", 63);
            return;
        }
    }

    // 如果请求的任务已经在运行，直接返回成功
    if (!currentLabel.empty() && currentLabel == label) {
        std::cout << "任务已在运行: " << label << std::endl;
        response.startResult = 0;  // 成功
        strncpy(response.resultDesc, "任务已在运行", 63);
        return;
    }

    // 调用API启动新任务
    std::cout << "开始启动新任务: " << label << std::endl;
    
    std::vector<std::string> labels;
    labels.push_back(label);
    
    // 从配置读取账号密码
    std::string account = app::infrastructure::ConfigManager::GetString("/api/account", "admin");
    std::string password = app::infrastructure::ConfigManager::GetString("/api/password", "12q12w12ee");
    
    auto result = m_apiClient->DeployStacks(labels, account, password);

    if (result.failureStackInfos.empty() && !result.successStackInfos.empty()) {
        // 启动成功
        response.startResult = 0;
        strncpy(response.resultDesc, "任务启动成功", 63);
        
        // 更新当前运行标签
        {
            std::lock_guard<std::mutex> lock(m_labelMutex);
            m_currentRunningLabel = label;
        }
        
        std::cout << "任务启动成功: " << label << std::endl;
    } else {
        // 启动失败
        response.startResult = 1;
        if (!result.failureStackInfos.empty()) {
            std::string desc = "任务启动失败: " + result.failureStackInfos[0].message;
            strncpy(response.resultDesc, desc.c_str(), 63);
        } else {
            strncpy(response.resultDesc, "任务启动失败", 63);
        }
        std::cerr << "任务启动失败: " << response.resultDesc << std::endl;
    }
}

void ResourceMonitorBroadcaster::BuildTaskStopResponse(TaskStopResponse& response, const TaskStopRequest& request) {
    // 获取当前正在运行的任务标签
    std::string currentLabel;
    {
        std::lock_guard<std::mutex> lock(m_labelMutex);
        currentLabel = m_currentRunningLabel;
    }

    if (currentLabel.empty()) {
        // 没有正在运行的任务
        response.stopResult = 0;
        strncpy(response.resultDesc, "无正在运行的任务", 63);
        std::cout << "无正在运行的任务" << std::endl;
        return;
    }

    // 停止当前任务
    std::cout << "开始停止任务: " << currentLabel << std::endl;
    
    std::vector<std::string> labels;
    labels.push_back(currentLabel);
    auto result = m_apiClient->UndeployStacks(labels);

    if (result.failureStackInfos.empty() && !result.successStackInfos.empty()) {
        // 停止成功
        response.stopResult = 0;
        strncpy(response.resultDesc, "任务停止成功", 63);
        
        // 清空当前运行标签
        {
            std::lock_guard<std::mutex> lock(m_labelMutex);
            m_currentRunningLabel = "";
        }
        
        std::cout << "任务停止成功: " << currentLabel << std::endl;
    } else {
        // 停止失败
        response.stopResult = 1;
        if (!result.failureStackInfos.empty()) {
            std::string desc = "任务停止失败: " + result.failureStackInfos[0].message;
            strncpy(response.resultDesc, desc.c_str(), 63);
        } else {
            strncpy(response.resultDesc, "任务停止失败", 63);
        }
        std::cerr << "任务停止失败: " << response.resultDesc << std::endl;
    }
}

std::string ResourceMonitorBroadcaster::WorkModeToLabel(uint16_t workMode) {
    // 将工作模式编号转换为标签名称
    // 例如：1 -> "模式1", 2 -> "模式2", 3 -> "模式3"
    return "模式" + std::to_string(workMode);
}

bool ResourceMonitorBroadcaster::HandleChassisResetRequest(const ChassisResetRequest& request) {
    if (m_socket < 0) {
        return false;
    }

    // 构建响应报文
    ChassisResetResponse response;
    memset(&response, 0, sizeof(response));

    // 设置头部
    memset(response.header, 0, 22);

    // 设置命令码
    response.command = m_cmdChassisResetResp;

    // 设置响应ID
    response.responseId = m_nextResponseId++;

    // 构建复位响应数据
    BuildChassisResetResponse(response, request);

    // 设置发送地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    inet_pton(AF_INET, m_multicastGroup.c_str(), &addr.sin_addr);

    // 发送组播数据包
    int result = sendto(m_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&addr, sizeof(addr));

    if (result > 0) {
        std::cout << "发送机箱复位响应: (响应ID=" << response.responseId << ")" << std::endl;
        return true;
    }

    std::cerr << "发送机箱复位响应失败: " << strerror(errno) << std::endl;
    return false;
}

bool ResourceMonitorBroadcaster::HandleChassisSelfCheckRequest(const ChassisSelfCheckRequest& request) {
    if (m_socket < 0) {
        return false;
    }

    // 构建响应报文
    ChassisSelfCheckResponse response;
    memset(&response, 0, sizeof(response));

    // 设置头部
    memset(response.header, 0, 22);

    // 设置命令码
    response.command = m_cmdChassisSelfCheckResp;

    // 设置响应ID
    response.responseId = m_nextResponseId++;

    // 设置机箱号
    response.chassisNumber = request.chassisNumber;

    // 构建自检响应数据
    BuildChassisSelfCheckResponse(response, request);

    // 设置发送地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    inet_pton(AF_INET, m_multicastGroup.c_str(), &addr.sin_addr);

    // 发送组播数据包
    int result = sendto(m_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&addr, sizeof(addr));

    if (result > 0) {
        std::cout << "发送机箱自检响应: 机箱" << response.chassisNumber 
                  << " (响应ID=" << response.responseId << ")" << std::endl;
        return true;
    }

    std::cerr << "发送机箱自检响应失败: " << strerror(errno) << std::endl;
    return false;
}

void ResourceMonitorBroadcaster::BuildChassisSelfCheckResponse(ChassisSelfCheckResponse& response, const ChassisSelfCheckRequest& request) {
    // 初始化所有结果为失败（1）
    memset(response.checkResults, 1, sizeof(response.checkResults));

    // 根据机箱号获取机箱
    auto chassis = m_chassisRepo->FindByNumber(request.chassisNumber);
    if (!chassis) {
        std::cerr << "未找到机箱: " << request.chassisNumber << std::endl;
        return;
    }

    const auto& boards = chassis->GetAllBoards();

    // 遍历12块板卡
    for (int boardIdx = 0; boardIdx < 12; ++boardIdx) {
        // 检查是否需要自检该板卡（0：自检，1：不需自检）
        if (request.checkFlags[boardIdx] == 0) {
            // 需要自检
            if (boardIdx < static_cast<int>(boards.size())) {
                const auto& board = boards[boardIdx];
                std::string boardIp = board.GetAddress();
                
                if (!boardIp.empty()) {
                    std::cout << "自检机箱" << request.chassisNumber << " 板卡" << (boardIdx + 1) 
                              << " (IP: " << boardIp << ")" << std::endl;
                    
                    // Ping板卡IP
                    if (m_chassisController->SelfcheckBoard(boardIp)) {
                        response.checkResults[boardIdx] = 0;  // 自检成功
                    } else {
                        response.checkResults[boardIdx] = 1;  // 自检失败
                    }
                } else {
                    response.checkResults[boardIdx] = 1;  // IP地址为空，自检失败
                }
            } else {
                response.checkResults[boardIdx] = 1;  // 板卡不存在，自检失败
            }
        } else {
            // 不需自检，保持为1（没有自检）
            response.checkResults[boardIdx] = 1;
        }
    }
}

void ResourceMonitorBroadcaster::BuildChassisResetResponse(ChassisResetResponse& response, const ChassisResetRequest& request) {
    // 初始化所有结果为失败（1）
    memset(response.resetResults, 1, sizeof(response.resetResults));

    // 获取所有机箱
    auto allChassis = m_chassisRepo->GetAll();

    // 遍历9个机箱
    for (size_t chassisIdx = 0; chassisIdx < allChassis.size() && chassisIdx < 9; ++chassisIdx) {
        const auto& chassis = allChassis[chassisIdx];
        int chassisNumber = chassis->GetChassisNumber();
        
        // 获取机箱IP地址（使用第7块板卡的IP地址作为机箱IP）
        // 如果第7块板卡不存在，则使用默认格式
        std::string chassisIp;
        const auto& boards = chassis->GetAllBoards();
        if (boards.size() >= 7 && !boards[6].GetAddress().empty()) {
            // 从第7块板卡的IP地址提取机箱IP（假设格式为 192.168.x.y，取前三个段）
            std::string seventhBoardIp = boards[6].GetAddress();
            size_t lastDot = seventhBoardIp.find_last_of('.');
            if (lastDot != std::string::npos) {
                chassisIp = seventhBoardIp.substr(0, lastDot) + ".1";
            } else {
                chassisIp = "192.168." + std::to_string(chassisNumber) + ".1";
            }
        } else {
            chassisIp = "192.168." + std::to_string(chassisNumber) + ".1";
        }
        
        // 遍历12块板卡
        for (int boardIdx = 0; boardIdx < 12; ++boardIdx) {
            size_t flagIndex = chassisIdx * 12 + boardIdx;
            
            // 检查是否需要复位该板卡
            if (request.resetFlags[flagIndex] == 1) {
                // 需要复位，调用 ChassisController
                std::vector<int> slotNumbers;
                slotNumbers.push_back(boardIdx + 1);  // 槽位号从1开始
                
                std::cout << "复位机箱" << chassisNumber << " 板卡" << (boardIdx + 1) << " (IP: " << chassisIp << ")" << std::endl;
                auto resetResult = m_chassisController->resetBoard(
                    chassisIp, slotNumbers, request.requestId);
                
                // 根据复位结果设置响应
                // 0：复位成功，1：没有复位或复位失败
                if (resetResult.result == ResourceController::OperationResult::SUCCESS) {
                    response.resetResults[flagIndex] = 0;  // 复位成功
                } else {
                    response.resetResults[flagIndex] = 1;  // 复位失败
                }
            } else {
                // 不需要复位，保持为1（没有复位）
                response.resetResults[flagIndex] = 1;
            }
        }
    }
}

bool ResourceMonitorBroadcaster::SendFaultReport(const std::string& faultDescription) {
    if (m_socket < 0) {
        std::cerr << "发送故障上报失败: socket未初始化" << std::endl;
        return false;
    }
    
    // 构建故障上报报文
    FaultReportPacket packet;
    memset(&packet, 0, sizeof(packet));
    
    // 设置头部（22字节）
    memset(packet.header, 0, 22);
    
    // 设置命令码
    packet.command = m_cmdFaultReport;
    
    // 设置故障描述（最多256字符）
    size_t descLen = faultDescription.length();
    if (descLen > 256) {
        descLen = 256;
        std::cerr << "警告: 故障描述过长，已截断为256字符" << std::endl;
    }
    strncpy(packet.faultDescription, faultDescription.c_str(), descLen);
    packet.faultDescription[descLen] = '\0';
    
    // 设置发送地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    inet_pton(AF_INET, m_multicastGroup.c_str(), &addr.sin_addr);
    
    // 发送组播数据包
    int result = sendto(m_socket, &packet, sizeof(packet), 0,
                        (struct sockaddr*)&addr, sizeof(addr));
    
    if (result > 0) {
        std::cout << "发送故障上报成功: " << faultDescription.substr(0, 50) << "..." << std::endl;
        return true;
    }
    
    std::cerr << "发送故障上报失败: " << strerror(errno) << std::endl;
    return false;
}

// ResourceMonitorListener 实现
ResourceMonitorListener::ResourceMonitorListener(
    std::shared_ptr<ResourceMonitorBroadcaster> broadcaster,
    const std::string& multicastGroup,
    uint16_t port)
    : m_broadcaster(broadcaster)
    , m_multicastGroup(multicastGroup)
    , m_port(port)
    , m_socket(-1)
    , m_running(false) {
    
    // 创建socket
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        std::cerr << "创建UDP socket失败" << std::endl;
        return;
    }
    
    // 设置socket选项，允许端口复用
    int reuse = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    // 绑定地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);
    
    if (bind(m_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "绑定地址失败: " << strerror(errno) << std::endl;
        close(m_socket);
        m_socket = -1;
        return;
    }
    
    // 加入组播组
    struct ip_mreq mreq;
    inet_pton(AF_INET, m_multicastGroup.c_str(), &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        std::cerr << "加入组播组失败: " << strerror(errno) << std::endl;
    }
}

ResourceMonitorListener::~ResourceMonitorListener() {
    Stop();
    if (m_socket >= 0) {
        close(m_socket);
    }
}

void ResourceMonitorListener::Start() {
    if (m_running) {
        return;
    }
    
    if (m_socket < 0) {
        std::cerr << "socket无效，无法启动监听" << std::endl;
        return;
    }
    
    m_running = true;
    m_listenThread = std::thread(&ResourceMonitorListener::ListenLoop, this);
    
    std::cout << "资源监控监听器已启动 (组播地址: " << m_multicastGroup 
              << ":" << m_port << ")" << std::endl;
}

void ResourceMonitorListener::Stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    if (m_listenThread.joinable()) {
        m_listenThread.join();
    }
    
    std::cout << "资源监控监听器已停止" << std::endl;
}

void ResourceMonitorListener::SetCommand(uint16_t resourceMonitor, uint16_t taskQuery, 
                                          uint16_t taskStart, uint16_t taskStop, uint16_t chassisReset, uint16_t chassisSelfCheck) {
    m_cmdResourceMonitor = resourceMonitor;
    m_cmdTaskQuery = taskQuery;
    m_cmdTaskStart = taskStart;
    m_cmdTaskStop = taskStop;
    m_cmdChassisReset = chassisReset;
    m_cmdChassisSelfCheck = chassisSelfCheck;
}

void ResourceMonitorListener::ListenLoop() {
    std::cout << "开始监听组播请求..." << std::endl;

    char buffer[1024];

    while (m_running) {
        struct sockaddr_in senderAddr;
        socklen_t addrLen = sizeof(senderAddr);

        ssize_t recvLen = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                                    (struct sockaddr*)&senderAddr, &addrLen);

        if (recvLen > 0 && recvLen >= sizeof(ResourceMonitorRequest)) {
            // 先尝试解析命令码（位于22-23字节）
            if (recvLen >= 24) {
                uint16_t command;
                memcpy(&command, buffer + 22, 2);

                if (command == m_cmdResourceMonitor && recvLen >= sizeof(ResourceMonitorRequest)) {
                    // 资源监控请求
                    ResourceMonitorRequest* request = (ResourceMonitorRequest*)buffer;
                    std::cout << "收到资源监控请求: ID=" << request->requestId << std::endl;

                    // 发送响应
                    if (m_broadcaster) {
                        m_broadcaster->SendResponse(request->requestId);
                    }
                }
                else if (command == m_cmdTaskQuery && recvLen >= sizeof(TaskQueryRequest)) {
                    // 任务查看请求
                    TaskQueryRequest* request = (TaskQueryRequest*)buffer;
                    std::cout << "收到任务查看请求: 机箱" << request->chassisNumber
                              << " 板卡" << request->boardNumber
                              << " 任务序号" << request->taskIndex
                              << " (请求ID=" << request->requestId << ")" << std::endl;

                    // 发送任务查询响应
                    if (m_broadcaster) {
                        m_broadcaster->SendTaskQueryResponse(*request);
                    }
                }
                else if (command == m_cmdTaskStart && recvLen >= sizeof(TaskStartRequest)) {
                    // 任务启动请求
                    TaskStartRequest* request = (TaskStartRequest*)buffer;
                    std::cout << "收到任务启动请求: 工作模式=" << request->workMode
                              << " 启动策略=" << request->startStrategy
                              << " (请求ID=" << request->requestId << ")" << std::endl;

                    // 发送任务启动响应
                    if (m_broadcaster) {
                        m_broadcaster->HandleTaskStartRequest(*request);
                    }
                }
                else if (command == m_cmdTaskStop && recvLen >= sizeof(TaskStopRequest)) {
                    // 任务停止请求
                    TaskStopRequest* request = (TaskStopRequest*)buffer;
                    std::cout << "收到任务停止请求: (请求ID=" << request->requestId << ")" << std::endl;

                    // 发送任务停止响应
                    if (m_broadcaster) {
                        m_broadcaster->HandleTaskStopRequest(*request);
                    }
                }
                else if (command == m_cmdChassisReset && recvLen >= sizeof(ChassisResetRequest)) {
                    // 机箱复位请求
                    ChassisResetRequest* request = (ChassisResetRequest*)buffer;
                    std::cout << "收到机箱复位请求: (请求ID=" << request->requestId << ")" << std::endl;

                    // 发送机箱复位响应
                    if (m_broadcaster) {
                        m_broadcaster->HandleChassisResetRequest(*request);
                    }
                }
                else if (command == m_cmdChassisSelfCheck && recvLen >= sizeof(ChassisSelfCheckRequest)) {
                    // 机箱自检请求
                    ChassisSelfCheckRequest* request = (ChassisSelfCheckRequest*)buffer;
                    std::cout << "收到机箱自检请求: 机箱" << request->chassisNumber 
                              << " (请求ID=" << request->requestId << ")" << std::endl;

                    // 发送机箱自检响应
                    if (m_broadcaster) {
                        m_broadcaster->HandleChassisSelfCheckRequest(*request);
                    }
                }
            }
        }
    }
}

}

