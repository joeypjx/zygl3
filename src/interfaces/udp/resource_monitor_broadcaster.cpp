#include "resource_monitor_broadcaster.h"
#include "src/domain/chassis.h"
#include "src/domain/board.h"
#include "src/domain/value_objects.h"
#include "src/infrastructure/config/config_manager.h"
#include "src/infrastructure/utils/udp_data_printer.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <ifaddrs.h>
#include <chrono>

namespace app::interfaces {

// 工作模式标签前缀
static const std::string WORK_MODE_LABEL_PREFIX = "工作模式";

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
        spdlog::error("创建UDP socket失败");
        return;
    }

    // 启用广播
    int broadcast = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    
    // 初始化组播地址（避免在每次发送时重复设置）
    memset(&m_multicastAddr, 0, sizeof(m_multicastAddr));
    m_multicastAddr.sin_family = AF_INET;
    m_multicastAddr.sin_port = htons(m_port);
    inet_pton(AF_INET, m_multicastGroup.c_str(), &m_multicastAddr.sin_addr);
}

ResourceMonitorBroadcaster::~ResourceMonitorBroadcaster() {
    Stop();
    if (m_socket >= 0) {
        close(m_socket);
    }
}

void ResourceMonitorBroadcaster::Start() {
    m_running = true;
    spdlog::info("资源监控广播器已启动 (组播地址: {}:{})", m_multicastGroup, m_port);
}

void ResourceMonitorBroadcaster::SetCommand(uint16_t resourceMonitorResp, uint16_t taskQueryResp, 
                                             uint16_t taskStartResp, uint16_t taskStopResp, uint16_t chassisResetResp, uint16_t chassisSelfCheckResp, uint16_t faultReport, uint16_t bmcQueryResp) {
    m_cmdResourceMonitorResp = resourceMonitorResp;
    m_cmdTaskQueryResp = taskQueryResp;
    m_cmdTaskStartResp = taskStartResp;
    m_cmdTaskStopResp = taskStopResp;
    m_cmdChassisResetResp = chassisResetResp;
    m_cmdChassisSelfCheckResp = chassisSelfCheckResp;
    m_cmdFaultReport = faultReport;
    m_cmdBmcQueryResp = bmcQueryResp;
}

void ResourceMonitorBroadcaster::Stop() {
    m_running = false;
    spdlog::info("资源监控广播器已停止");
}

void ResourceMonitorBroadcaster::SetResponseHeader(char* header, uint16_t totalLength) {
    // 清空头部
    memset(header, 0, 22);
    
    // 0-1: total length (2字节，主机字节序)
    memcpy(header + 0, &totalLength, 2);
    
    // 2-3: 0000H (2字节)
    uint16_t zero = 0;
    memcpy(header + 2, &zero, 2);
    
    // 4-7: local IP (4字节，主机字节序)
    // 从配置文件获取 alert_server host
    std::string localIpStr = app::infrastructure::ConfigManager::GetString("/alert_server/host", "0.0.0.0");
    uint32_t localIp = 0;
    
    // 如果配置的是 "0.0.0.0"，尝试从 socket 或网络接口获取实际 IP
    if (localIpStr == "0.0.0.0" || localIpStr.empty()) {
        struct sockaddr_in localAddr;
        socklen_t addrLen = sizeof(localAddr);
        if (getsockname(m_socket, (struct sockaddr*)&localAddr, &addrLen) == 0) {
            localIp = ntohl(localAddr.sin_addr.s_addr);  // 转换为主机字节序
        } else {
            // 如果获取失败，尝试从网络接口获取
            struct ifaddrs* ifaddr;
            if (getifaddrs(&ifaddr) == 0) {
                for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
                    if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                        struct sockaddr_in* sin = (struct sockaddr_in*)ifa->ifa_addr;
                        // 跳过回环地址
                        if (sin->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
                            localIp = ntohl(sin->sin_addr.s_addr);  // 转换为主机字节序
                            break;
                        }
                    }
                }
                freeifaddrs(ifaddr);
            }
        }
    } else {
        // 将 IP 字符串转换为 uint32_t（主机字节序）
        struct in_addr addr;
        if (inet_pton(AF_INET, localIpStr.c_str(), &addr) == 1) {
            localIp = ntohl(addr.s_addr);  // 转换为主机字节序
        } else {
            spdlog::warn("无法解析 alert_server host IP: {}, 使用默认值 0", localIpStr);
            localIp = 0;
        }
    }
    memcpy(header + 4, &localIp, 4);
    
    // 8-11: target IP (4字节，组播地址，主机字节序)
    uint32_t targetIp = ntohl(m_multicastAddr.sin_addr.s_addr);  // 转换为主机字节序
    memcpy(header + 8, &targetIp, 4);
    
    // 12-15: timestamp from today 00:00:00 (4字节，主机字节序，毫秒)
    // 获取当前时间（毫秒精度）
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    // 计算今天 00:00:00 的时间戳（毫秒）
    time_t nowTime = std::chrono::system_clock::to_time_t(now);
    struct tm today;
    localtime_r(&nowTime, &today);  // 使用线程安全的 localtime_r
    today.tm_hour = 0;
    today.tm_min = 0;
    today.tm_sec = 0;
    time_t todayStart = mktime(&today);
    auto todayStartMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::from_time_t(todayStart).time_since_epoch()).count();
    
    // 计算从今天 00:00:00 开始的毫秒数
    uint32_t timestamp = static_cast<uint32_t>(nowMs - todayStartMs);
    memcpy(header + 12, &timestamp, 4);
    
    // 16: 01H (1字节)
    header[16] = 0x01;
    
    // 17: Flag B2H (1字节)
    header[17] = 0xB2;
    
    // 18-19: total length - 16 (2字节，主机字节序)
    uint16_t lengthMinus16 = totalLength - 16;
    memcpy(header + 18, &lengthMinus16, 2);
    
    // 20-21: FFFFH (2字节)
    uint16_t ffff = 0xFFFF;
    memcpy(header + 20, &ffff, 2);
}

bool ResourceMonitorBroadcaster::SendResourceMonitorResponse(uint32_t requestId) {
    if (m_socket < 0) {
        return false;
    }
    
    // 构建响应报文
    ResourceMonitorResponse response;
    memset(&response, 0, sizeof(response));
    
    // 设置命令码
    response.command = m_cmdResourceMonitorResp;
    
    // 设置头部（22字节）
    SetResponseHeader(response.header, sizeof(response));
    
    // 设置响应ID（与请求ID一致）
    response.responseId = requestId;
    
    // 构建板卡状态和任务状态数据
    BuildResponseData(response);
    
    // 打印要发送的UDP数据（可选，用于调试）
    app::infrastructure::utils::UdpDataPrinter::PrintSentDataSimple(
        &response, sizeof(response), m_multicastGroup, m_port);
    
    // 发送组播数据包（使用构造函数中初始化的组播地址）
    int result = sendto(m_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&m_multicastAddr, sizeof(m_multicastAddr));
    
    if (result > 0) {
        spdlog::info("发送资源监控响应: ID={}", response.responseId);
        return true;
    }
    
    spdlog::error("发送响应失败: {}", strerror(errno));
    return false;
}

void ResourceMonitorBroadcaster::BuildResponseData(ResourceMonitorResponse& response) {
    // 初始化所有板卡状态为2（不在位）
    memset(response.boardStatus, 2, sizeof(response.boardStatus));
    // 初始化所有任务状态为2（没有任务）
    memset(response.taskStatus, 2, sizeof(response.taskStatus));
    
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
        // 机箱不存在，所有板卡状态设为2（不在位）
        memset(array, 2, 12);
        return;
    }
    
    const auto& boards = chassis->GetAllBoards();
    
    // 协议要求12块板卡，我们取前12块（槽位1-12）
    for (size_t i = 0; i < 12; ++i) {
        if (i < boards.size()) {
            auto boardStatus = boards[i].GetStatus();
            
            // 根据协议：0=板卡正常，1=板卡异常，2=板卡不在槽位
            if (boardStatus == app::domain::BoardOperationalStatus::Normal) {
                array[i] = 0;  // 板卡正常
            } else if (boardStatus == app::domain::BoardOperationalStatus::Abnormal) {
                array[i] = 1;  // 板卡异常
            } else if (boardStatus == app::domain::BoardOperationalStatus::Offline) {
                array[i] = 2;  // 板卡不在槽位
            } else {
                // Unknown状态，默认设为2（不在位）
                array[i] = 2;
            }
        } else {
            // 板卡槽位不存在，设为2（不在位）
            array[i] = 2;
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
                // 0=任务正常，1=任务异常
                // taskStatus: 1-运行中，2-已完成，3-异常，0-其他
                if (tasks[taskIdx].taskStatus == 1) {  // 运行中表示正常
                    array[boardIdx * 8 + taskIdx] = 0;
                } else {
                    array[boardIdx * 8 + taskIdx] = 1;
                }
            } else {
                // 没有任务，设置为2
                array[boardIdx * 8 + taskIdx] = 2;
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

    // 设置命令码
    response.command = m_cmdTaskQueryResp;
    
    // 设置头部（22字节）
    SetResponseHeader(response.header, sizeof(response));

    // 设置响应ID（与请求ID一致）
    response.responseId = request.requestId;

    // 构建任务查询响应数据
    BuildTaskQueryResponse(response, request);

    // 打印要发送的UDP数据（可选，用于调试）
    app::infrastructure::utils::UdpDataPrinter::PrintSentDataSimple(
        &response, sizeof(response), m_multicastGroup, m_port);

    // 发送组播数据包（使用构造函数中初始化的组播地址）
    int result = sendto(m_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&m_multicastAddr, sizeof(m_multicastAddr));

    if (result > 0) {
        spdlog::info("发送任务查询响应: 机箱{} 板卡{} 任务{} (响应ID={})", 
                     request.chassisNumber, request.boardNumber, request.taskIndex, response.responseId);
        return true;
    }

    spdlog::error("发送任务查询响应失败: {}", strerror(errno));
    return false;
}

void ResourceMonitorBroadcaster::BuildTaskQueryResponse(TaskQueryResponse& response, const TaskQueryRequest& request) {
    // 1. 根据机箱号和板卡号获取板卡信息
    auto chassis = m_chassisRepo->FindByNumber(request.chassisNumber);
    if (!chassis) {
        spdlog::error("未找到机箱: {}", request.chassisNumber);
        response.taskStatus = 1;  // 异常
        return;
    }

    auto* board = chassis->GetBoardBySlot(request.boardNumber);
    if (!board) {
        spdlog::error("未找到板卡: 机箱{} 槽位{}", request.chassisNumber, request.boardNumber);
        response.taskStatus = 1;  // 异常
        return;
    }

    // 2. 从板卡获取任务列表，根据任务序号获取taskID
    // request.taskIndex 从1开始，所以需要减1
    const auto& tasks = board->GetTasks();
    if (request.taskIndex < 1 || request.taskIndex > tasks.size()) {
        spdlog::error("任务序号超出范围: taskIndex={}, tasks.size()={}", request.taskIndex, tasks.size());
        response.taskStatus = 1;  // 异常
        return;
    }

    const auto& taskInfo = tasks[request.taskIndex - 1];
    std::string taskID = taskInfo.taskID;

    // 3. 通过stackRepo查找任务的资源使用情况
    auto resourceUsage = m_stackRepo->GetTaskResources(taskID);
    if (!resourceUsage) {
        spdlog::error("未找到任务资源信息: taskID={}", taskID);
        response.taskStatus = 1;  // 异常
        return;
    }

    // 4. 填充响应数据
    // 参考BuildResponseData的逻辑：taskStatus == 1（运行中）表示正常，其他表示异常
    // TaskQueryResponse.taskStatus: 0=正常，1=异常
    if (taskInfo.taskStatus == 1) {  // 运行中表示正常
        response.taskStatus = 0;  // 正常
    } else {
        response.taskStatus = 1;  // 异常
    }

    // 将taskID字符串转换为uint32 (这里简化处理，实际可能需要更复杂的转换)
    try {
        response.taskId = std::stoul(taskID);
    } catch (const std::exception&) {
        // 如果taskID不是纯数字或超出范围，使用哈希值
        response.taskId = std::hash<std::string>{}(taskID);
    }

    // 从当前运行标签获取工作模式
    std::string currentLabel;
    {
        std::lock_guard<std::mutex> lock(m_labelMutex);
        currentLabel = m_currentRunningLabel;
    }
    response.workMode = LabelToWorkMode(currentLabel);

    // 板卡IP转换
    response.boardIp = IpStringToUint32(board->GetAddress());

    // CPU使用率转换为千分比 (0-1000)
    if (resourceUsage->cpuUsage > 1) {
        response.cpuUsage = 1000;
    } else {
        response.cpuUsage = static_cast<uint16_t>(resourceUsage->cpuUsage * 1000);
    }

    // 内存使用率（浮点类型，直接赋值）
    if (resourceUsage->memoryUsage > 1) {
        response.memoryUsage = 1.0f;
    } else {
        response.memoryUsage = resourceUsage->memoryUsage;
    }

    spdlog::info("任务查询成功: taskID={} CPU={:.1f}% MEM={:.1f}%", 
                 taskID, response.cpuUsage / 10.0f, response.memoryUsage * 100.0f);
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

    // 根据协议：启动策略 0：先停止当前任务，再启动任务。如果不是0，则不处理，也不返回响应
    if (request.startStrategy != 0) {
        spdlog::info("任务启动请求被忽略: 工作模式={} 启动策略={} (非0，不处理也不返回响应)", 
                     request.workMode, request.startStrategy);
        return false;  // 不处理，也不返回响应
    }

    // 构建响应报文
    TaskStartResponse response;
    memset(&response, 0, sizeof(response));

    // 设置命令码
    response.command = m_cmdTaskStartResp;
    
    // 设置头部（22字节）
    SetResponseHeader(response.header, sizeof(response));

    // 设置响应ID（与请求ID一致）
    response.responseId = request.requestId;

    // 构建启动响应数据
    BuildTaskStartResponse(response, request);

    // 打印要发送的UDP数据（可选，用于调试）
    app::infrastructure::utils::UdpDataPrinter::PrintSentDataSimple(
            &response, sizeof(response), m_multicastGroup, m_port);

    // 发送组播数据包（使用构造函数中初始化的组播地址）
    int result = sendto(m_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&m_multicastAddr, sizeof(m_multicastAddr));

    if (result > 0) {
        spdlog::info("发送任务启动响应: 工作模式={} (响应ID={})", request.workMode, response.responseId);
        return true;
    }

    spdlog::error("发送任务启动响应失败: {}", strerror(errno));
    return false;
}

bool ResourceMonitorBroadcaster::HandleTaskStopRequest(const TaskStopRequest& request) {
    if (m_socket < 0) {
        return false;
    }

    // 构建响应报文
    TaskStopResponse response;
    memset(&response, 0, sizeof(response));

    // 设置命令码
    response.command = m_cmdTaskStopResp;
    
    // 设置头部（22字节）
    SetResponseHeader(response.header, sizeof(response));

    // 设置响应ID（与请求ID一致）
    response.responseId = request.requestId;

    // 构建停止响应数据
    BuildTaskStopResponse(response, request);

    // 打印要发送的UDP数据（可选，用于调试）
    app::infrastructure::utils::UdpDataPrinter::PrintSentDataSimple(
        &response, sizeof(response), m_multicastGroup, m_port);

    // 发送组播数据包（使用构造函数中初始化的组播地址）
    int result = sendto(m_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&m_multicastAddr, sizeof(m_multicastAddr));

    if (result > 0) {
        spdlog::info("发送任务停止响应: (响应ID={})", response.responseId);
        return true;
    }

    spdlog::error("发送任务停止响应失败: {}", strerror(errno));
    return false;
}

void ResourceMonitorBroadcaster::BuildTaskStartResponse(TaskStartResponse& response, const TaskStartRequest& request) {
    // 将工作模式转换为标签名称
    std::string label = WorkModeToLabel(request.workMode);

    // 获取当前正在运行的任务标签
    std::string currentLabel;
    {
        std::lock_guard<std::mutex> lock(m_labelMutex);
        currentLabel = m_currentRunningLabel;
    }

    // 根据协议：启动策略 0：先停止当前任务，再启动任务
    // 这里已经确保 startStrategy == 0（在HandleTaskStartRequest中已检查）
    // 通过 stop 参数传递给 DeployStacks，stop=1 表示先停止当前任务
    int stop = 1;
    
    std::vector<std::string> labels;
    labels.push_back(label);
    
    // 从配置读取账号密码
    std::string account = app::infrastructure::ConfigManager::GetString("/api/account", "admin");
    std::string password = app::infrastructure::ConfigManager::GetString("/api/password", "12q12w12ee");
    
    auto result = m_apiClient->DeployStacks(labels, account, password, stop);

    if (result.failureStackInfos.empty() && !result.successStackInfos.empty()) {
        // 启动成功
        response.startResult = 0;
        strncpy(response.resultDesc, "任务启动成功", 63);
        
        // 更新当前运行标签
        {
            std::lock_guard<std::mutex> lock(m_labelMutex);
            m_currentRunningLabel = label;
        }
        
        spdlog::info("任务启动成功: {}", label);
    } else {
        // 启动失败
        response.startResult = 1;
        if (!result.failureStackInfos.empty()) {
            std::string desc = "任务启动失败: " + result.failureStackInfos[0].message;
            strncpy(response.resultDesc, desc.c_str(), 63);
        } else {
            strncpy(response.resultDesc, "任务启动失败", 63);
        }
        spdlog::error("任务启动失败: {}", response.resultDesc);
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
        // 没有正在运行的任务，调用ResetStacks方法
        spdlog::info("无正在运行的任务，调用ResetStacks方法");
        bool resetResult = m_apiClient->ResetStacks();
        
        if (resetResult) {
            // 复位成功
            response.stopResult = 0;
            strncpy(response.resultDesc, "无正在运行的任务，业务链路复位成功", 63);
            spdlog::info("业务链路复位成功");
        } else {
            // 复位失败
            response.stopResult = 1;
            strncpy(response.resultDesc, "无正在运行的任务，业务链路复位失败", 63);
            spdlog::error("业务链路复位失败");
        }
        return;
    }

    // 停止当前任务
    spdlog::info("开始停止任务: {}", currentLabel);
    
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
        
        spdlog::info("任务停止成功: {}", currentLabel);
    } else {
        // 停止失败
        response.stopResult = 1;
        if (!result.failureStackInfos.empty()) {
            std::string desc = "任务停止失败: " + result.failureStackInfos[0].message;
            strncpy(response.resultDesc, desc.c_str(), 63);
        } else {
            strncpy(response.resultDesc, "任务停止失败", 63);
        }
        spdlog::error("任务停止失败: {}", response.resultDesc);
    }
}

std::string ResourceMonitorBroadcaster::WorkModeToLabel(uint16_t workMode) {
    // 将工作模式编号转换为标签名称
    // 例如：1 -> "工作模式1", 2 -> "工作模式2", 3 -> "工作模式3"
    return WORK_MODE_LABEL_PREFIX + std::to_string(workMode);
}

uint16_t ResourceMonitorBroadcaster::LabelToWorkMode(const std::string& label) {
    // 将标签名称转换为工作模式编号
    // 例如："工作模式1" -> 1, "工作模式2" -> 2, "工作模式3" -> 3
    if (label.empty()) {
        return 0;  // 没有运行任务时返回0
    }
    
    // 检查是否以"工作模式"开头
    if (label.length() >= WORK_MODE_LABEL_PREFIX.length() && 
        label.substr(0, WORK_MODE_LABEL_PREFIX.length()) == WORK_MODE_LABEL_PREFIX) {
        try {
            // 提取"工作模式"后面的数字
            std::string numStr = label.substr(WORK_MODE_LABEL_PREFIX.length());
            return static_cast<uint16_t>(std::stoul(numStr));
        } catch (const std::exception&) {
            // 如果转换失败，返回0
            return 0;
        }
    }
    
    // 如果不是"工作模式X"格式，返回0
    return 0;
}

bool ResourceMonitorBroadcaster::HandleChassisResetRequest(const ChassisResetRequest& request) {
    if (m_socket < 0) {
        return false;
    }

    // 构建响应报文
    ChassisResetResponse response;
    memset(&response, 0, sizeof(response));

    // 设置命令码
    response.command = m_cmdChassisResetResp;
    
    // 设置头部（22字节）
    SetResponseHeader(response.header, sizeof(response));

    // 设置响应ID（与请求ID一致）
    response.responseId = request.requestId;

    // 构建复位响应数据
    BuildChassisResetResponse(response, request);

    // 打印要发送的UDP数据（可选，用于调试）
    app::infrastructure::utils::UdpDataPrinter::PrintSentDataSimple(
            &response, sizeof(response), m_multicastGroup, m_port);

    // 发送组播数据包（使用构造函数中初始化的组播地址）
    int result = sendto(m_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&m_multicastAddr, sizeof(m_multicastAddr));

    if (result > 0) {
        spdlog::info("发送机箱复位响应: (响应ID={})", response.responseId);
        return true;
    }

    spdlog::error("发送机箱复位响应失败: {}", strerror(errno));
    return false;
}

bool ResourceMonitorBroadcaster::HandleChassisSelfCheckRequest(const ChassisSelfCheckRequest& request) {
    if (m_socket < 0) {
        return false;
    }

    // 构建响应报文
    ChassisSelfCheckResponse response;
    memset(&response, 0, sizeof(response));

    // 设置命令码
    response.command = m_cmdChassisSelfCheckResp;
    
    // 设置头部（22字节）
    SetResponseHeader(response.header, sizeof(response));

    // 设置响应ID（与请求ID一致）
    response.responseId = request.requestId;

    // 设置机箱号
    response.chassisNumber = request.chassisNumber;

    // 构建自检响应数据
    BuildChassisSelfCheckResponse(response, request);

    // 打印要发送的UDP数据（可选，用于调试）
    app::infrastructure::utils::UdpDataPrinter::PrintSentDataSimple(
            &response, sizeof(response), m_multicastGroup, m_port);

    // 发送组播数据包（使用构造函数中初始化的组播地址）
    int result = sendto(m_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&m_multicastAddr, sizeof(m_multicastAddr));

    if (result > 0) {
        spdlog::info("发送机箱自检响应: 机箱{} (响应ID={})", response.chassisNumber, response.responseId);
        return true;
    }

    spdlog::error("发送机箱自检响应失败: {}", strerror(errno));
    return false;
}

void ResourceMonitorBroadcaster::BuildChassisSelfCheckResponse(ChassisSelfCheckResponse& response, const ChassisSelfCheckRequest& request) {
    // 初始化所有结果为失败（1）
    memset(response.checkResults, 1, sizeof(response.checkResults));

    // 根据机箱号获取机箱
    auto chassis = m_chassisRepo->FindByNumber(request.chassisNumber);
    if (!chassis) {
        spdlog::error("未找到机箱: {}", request.chassisNumber);
        return;
    }

    const auto& boards = chassis->GetAllBoards();

    // 遍历12块板卡
    for (int boardIdx = 0; boardIdx < 12; ++boardIdx) {
        // 检查是否需要自检该板卡
        // 协议：0：自检 1：不需自检，其他数字：不自检
        if (request.checkFlags[boardIdx] == 0) {
            // 需要自检
            if (boardIdx < static_cast<int>(boards.size())) {
                const auto& board = boards[boardIdx];
                std::string boardIp = board.GetAddress();
                
                if (!boardIp.empty()) {
                    spdlog::info("自检机箱{} 板卡{} (IP: {})", request.chassisNumber, boardIdx + 1, boardIp);
                    
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
            // 不需自检（1或其他数字），保持为1（没有自检或自检失败）
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
        
        // 获取机箱IP地址（使用该机箱中boardType是EthernetSwitch（即10）的第一个板卡作为机箱IP）
        std::string chassisIp;
        const auto& boards = chassis->GetAllBoards();
        
        // 查找第一个EthernetSwitch类型的板卡
        bool foundEthernetSwitch = false;
        for (const auto& board : boards) {
            if (board.GetBoardType() == app::domain::BoardType::EthernetSwitch && 
                !board.GetAddress().empty()) {
                // 直接使用EthernetSwitch板卡的IP地址作为机箱IP
                chassisIp = board.GetAddress();
                foundEthernetSwitch = true;
                break;
            }
        }
        
        // 如果没有找到EthernetSwitch板卡，使用默认格式
        if (!foundEthernetSwitch) {
            chassisIp = "192.168." + std::to_string(chassisNumber * 2) + ".180";
        }
        
        // 先收集该机箱所有需要复位的板卡槽位号
        std::vector<int> slotNumbers;
        std::vector<size_t> flagIndices;  // 记录对应的flagIndex，用于后续设置结果
        
        for (int boardIdx = 0; boardIdx < 12; ++boardIdx) {
            size_t flagIndex = chassisIdx * 12 + boardIdx;
            
            // 检查是否需要复位该板卡
            // 协议：0：不复位 1：需要复位，其他数字：不复位
            if (request.resetFlags[flagIndex] == 1) {
                slotNumbers.push_back(boardIdx + 1);  // 槽位号从1开始
                flagIndices.push_back(flagIndex);
            } else {
                // 不需要复位（0或其他数字），保持为1（没有复位或复位失败）
                response.resetResults[flagIndex] = 1;
            }
        }
        
        // 如果有需要复位的板卡，一次性调用resetBoard
        if (!slotNumbers.empty()) {
            std::string slotStr;
            for (size_t i = 0; i < slotNumbers.size(); ++i) {
                slotStr += std::to_string(slotNumbers[i]);
                if (i < slotNumbers.size() - 1) {
                    slotStr += ",";
                }
            }
            spdlog::info("复位机箱{} 板卡{} (IP: {})", chassisNumber, slotStr, chassisIp);
            
            auto resetResult = m_chassisController->resetBoard(
                chassisIp, slotNumbers, request.requestId);
            
            // 根据复位结果设置每个板卡的响应
            // 0：复位成功，1：没有复位或复位失败
            if (resetResult.result == ResourceController::OperationResult::SUCCESS ||
                resetResult.result == ResourceController::OperationResult::PARTIAL_SUCCESS) {
                // 先初始化所有板卡为失败，然后根据slot_results更新
                for (size_t flagIndex : flagIndices) {
                    response.resetResults[flagIndex] = 1;  // 默认失败
                }
                
                // 根据slot_results设置每个槽位的结果
                for (const auto& slotResult : resetResult.slot_results) {
                    // 找到对应的flagIndex
                    for (size_t i = 0; i < slotNumbers.size(); ++i) {
                        if (slotNumbers[i] == slotResult.slot_number) {
                            size_t flagIndex = flagIndices[i];
                            if (slotResult.status == ResourceController::SlotStatus::NO_OPERATION_OR_SUCCESS) {
                                response.resetResults[flagIndex] = 0;  // 复位成功
                            } else {
                                response.resetResults[flagIndex] = 1;  // 复位失败
                            }
                            break;
                        }
                    }
                }
            } else {
                // 整体操作失败，所有板卡都标记为失败
                for (size_t flagIndex : flagIndices) {
                    response.resetResults[flagIndex] = 1;  // 复位失败
                }
            }
        }
    }
}

bool ResourceMonitorBroadcaster::SendFaultReport(const std::string& faultDescription, uint16_t problemCode) {
    if (m_socket < 0) {
        spdlog::error("发送故障上报失败: socket未初始化");
        return false;
    }
    
    // 构建故障上报报文
    FaultReportPacket packet;
    memset(&packet, 0, sizeof(packet));
    
    // 设置命令码
    packet.command = m_cmdFaultReport;
    
    // 设置头部（22字节）
    SetResponseHeader(packet.header, sizeof(packet));

    // 设置问题代码
    packet.problemCode = problemCode;
    
    // 设置故障描述（最多256字符）
    size_t descLen = faultDescription.length();
    if (descLen > 256) {
        descLen = 256;
        spdlog::warn("警告: 故障描述过长，已截断为256字符");
    }
    strncpy(packet.faultDescription, faultDescription.c_str(), descLen);
    packet.faultDescription[descLen] = '\0';
    
    // 打印要发送的UDP数据（可选，用于调试）
    app::infrastructure::utils::UdpDataPrinter::PrintSentDataSimple(
            &packet, sizeof(packet), m_multicastGroup, m_port);

    // 发送组播数据包（使用构造函数中初始化的组播地址）
    int result = sendto(m_socket, &packet, sizeof(packet), 0,
                        (struct sockaddr*)&m_multicastAddr, sizeof(m_multicastAddr));
    
    if (result > 0) {
        spdlog::info("发送故障上报成功: {}...", faultDescription.substr(0, 50));
        return true;
    }
    
    spdlog::error("发送故障上报失败: {}", strerror(errno));
    return false;
}

bool ResourceMonitorBroadcaster::HandleBmcQueryRequest(const BmcQueryRequest& request) {
    if (m_socket < 0) {
        return false;
    }

    // 构建响应报文
    BmcQueryResponse response;
    memset(&response, 0, sizeof(response));

    // 设置命令码
    response.command = m_cmdBmcQueryResp;
    
    // 设置头部（22字节）
    SetResponseHeader(response.header, sizeof(response));

    // 设置响应ID（与请求ID一致）
    response.responseId = request.requestId;

    // 构建BMC查询响应数据
    BuildBmcQueryResponse(response, request);

    // 发送组播数据包（使用构造函数中初始化的组播地址）
    int result = sendto(m_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&m_multicastAddr, sizeof(m_multicastAddr));

    if (result > 0) {
        spdlog::info("发送BMC查询响应: (响应ID={})", response.responseId);
        return true;
    }

    spdlog::error("发送BMC查询响应失败: {}", strerror(errno));
    return false;
}

void ResourceMonitorBroadcaster::BuildBmcQueryResponse(BmcQueryResponse& response, const BmcQueryRequest& request) {
    // 初始化所有数据为0
    memset(response.temperature, 0, sizeof(response.temperature));
    memset(response.voltage, 0, sizeof(response.voltage));
    memset(response.current, 0, sizeof(response.current));

    // 获取所有机箱
    auto allChassis = m_chassisRepo->GetAll();

    // 遍历9个机箱（协议要求9个机箱×12块板卡）
    for (size_t chassisIdx = 0; chassisIdx < allChassis.size() && chassisIdx < 9; ++chassisIdx) {
        const auto& chassis = allChassis[chassisIdx];
        const auto& boards = chassis->GetAllBoards();

        // 遍历12块板卡（协议要求12块板卡）
        for (size_t boardIdx = 0; boardIdx < 12 && boardIdx < boards.size(); ++boardIdx) {
            const auto& board = boards[boardIdx];
            
            // 计算在数组中的索引：机箱索引 * 12 + 板卡索引
            size_t arrayIndex = chassisIdx * 12 + boardIdx;

            // 从板卡获取监控数据
            response.temperature[arrayIndex] = board.GetTemperature();
            response.voltage[arrayIndex] = board.GetVoltage();
            response.current[arrayIndex] = board.GetCurrent();
        }
    }

    spdlog::info("BMC查询响应构建完成");
}

// ResourceMonitorListener 实现
ResourceMonitorListener::ResourceMonitorListener(
    std::shared_ptr<ResourceMonitorBroadcaster> broadcaster,
    std::shared_ptr<app::infrastructure::HeartbeatService> heartbeatService,
    const std::string& multicastGroup,
    uint16_t port)
    : m_broadcaster(broadcaster)
    , m_heartbeatService(heartbeatService)
    , m_multicastGroup(multicastGroup)
    , m_port(port)
    , m_socket(-1)
    , m_running(false) {
    
    // 创建socket
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        spdlog::error("创建UDP socket失败");
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
        spdlog::error("绑定地址失败: {}", strerror(errno));
        close(m_socket);
        m_socket = -1;
        return;
    }
    
    // 加入组播组
    struct ip_mreq mreq;
    inet_pton(AF_INET, m_multicastGroup.c_str(), &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        spdlog::error("加入组播组失败: {}", strerror(errno));
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
        spdlog::error("socket无效，无法启动监听");
        return;
    }
    
    m_running = true;
    m_listenThread = std::thread(&ResourceMonitorListener::ListenLoop, this);
    
    spdlog::info("资源监控监听器已启动 (组播地址: {}:{})", m_multicastGroup, m_port);
}

void ResourceMonitorListener::Stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    if (m_listenThread.joinable()) {
        m_listenThread.join();
    }
    
    spdlog::info("资源监控监听器已停止");
}

void ResourceMonitorListener::SetCommand(uint16_t resourceMonitor, uint16_t taskQuery, 
                                          uint16_t taskStart, uint16_t taskStop, uint16_t chassisReset, uint16_t chassisSelfCheck, uint16_t bmcQuery) {
    m_cmdResourceMonitor = resourceMonitor;
    m_cmdTaskQuery = taskQuery;
    m_cmdTaskStart = taskStart;
    m_cmdTaskStop = taskStop;
    m_cmdChassisReset = chassisReset;
    m_cmdChassisSelfCheck = chassisSelfCheck;
    m_cmdBmcQuery = bmcQuery;
}

void ResourceMonitorListener::ListenLoop() {
    spdlog::info("开始监听组播请求...");

    char buffer[1024];

    while (m_running) {
        struct sockaddr_in senderAddr;
        socklen_t addrLen = sizeof(senderAddr);

        ssize_t recvLen = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                                    (struct sockaddr*)&senderAddr, &addrLen);

        // 检查角色：只有主节点才处理请求
        if (m_heartbeatService && !m_heartbeatService->IsPrimary()) {
            // 备节点：接收请求但不处理，继续监听
            if (recvLen > 0) {
                spdlog::debug("当前为备节点，收到组播请求但不处理");
            }
            continue;
        }

        // 至少需要24字节才能读取命令码（header[22] + command[2]）
        if (recvLen > 0 && recvLen >= 24) {
            // 打印接收到的UDP数据（可选，用于调试）
            char senderIpStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &senderAddr.sin_addr, senderIpStr, INET_ADDRSTRLEN);
            app::infrastructure::utils::UdpDataPrinter::PrintReceivedDataSimple(
                buffer, recvLen, senderIpStr, ntohs(senderAddr.sin_port));
            // 先解析命令码（位于22-23字节）
            uint16_t command;
            memcpy(&command, buffer + 22, 2);

            // 根据命令码检查对应的请求大小
            if (command == m_cmdResourceMonitor && recvLen >= sizeof(ResourceMonitorRequest)) {
                // 资源监控请求
                ResourceMonitorRequest* request = (ResourceMonitorRequest*)buffer;
                spdlog::info("收到资源监控请求: ID={}", request->requestId);

                    // 发送响应
                    if (m_broadcaster) {
                        m_broadcaster->SendResourceMonitorResponse(request->requestId);
                    }
            }
            else if (command == m_cmdTaskQuery && recvLen >= sizeof(TaskQueryRequest)) {
                // 任务查看请求
                TaskQueryRequest* request = (TaskQueryRequest*)buffer;
                spdlog::info("收到任务查看请求: 机箱{} 板卡{} 任务序号{} (请求ID={})", 
                             request->chassisNumber, request->boardNumber, request->taskIndex, request->requestId);

                // 发送任务查询响应
                if (m_broadcaster) {
                    m_broadcaster->SendTaskQueryResponse(*request);
                }
            }
            else if (command == m_cmdTaskStart && recvLen >= sizeof(TaskStartRequest)) {
                // 任务启动请求
                TaskStartRequest* request = (TaskStartRequest*)buffer;
                spdlog::info("收到任务启动请求: 工作模式={} 启动策略={} (请求ID={})", 
                             request->workMode, request->startStrategy, request->requestId);

                // 发送任务启动响应
                if (m_broadcaster) {
                    m_broadcaster->HandleTaskStartRequest(*request);
                }
            }
            else if (command == m_cmdTaskStop && recvLen >= sizeof(TaskStopRequest)) {
                // 任务停止请求
                TaskStopRequest* request = (TaskStopRequest*)buffer;
                spdlog::info("收到任务停止请求: (请求ID={})", request->requestId);

                // 发送任务停止响应
                if (m_broadcaster) {
                    m_broadcaster->HandleTaskStopRequest(*request);
                }
            }
            else if (command == m_cmdChassisReset && recvLen >= sizeof(ChassisResetRequest)) {
                // 机箱复位请求
                ChassisResetRequest* request = (ChassisResetRequest*)buffer;
                spdlog::info("收到机箱复位请求: (请求ID={})", request->requestId);

                // 发送机箱复位响应
                if (m_broadcaster) {
                    m_broadcaster->HandleChassisResetRequest(*request);
                }
            }
            else if (command == m_cmdChassisSelfCheck && recvLen >= sizeof(ChassisSelfCheckRequest)) {
                // 机箱自检请求
                ChassisSelfCheckRequest* request = (ChassisSelfCheckRequest*)buffer;
                spdlog::info("收到机箱自检请求: 机箱{} (请求ID={})", request->chassisNumber, request->requestId);

                // 发送机箱自检响应
                if (m_broadcaster) {
                    m_broadcaster->HandleChassisSelfCheckRequest(*request);
                }
            }
            else if (command == m_cmdBmcQuery && recvLen >= sizeof(BmcQueryRequest)) {
                // BMC查询请求
                BmcQueryRequest* request = (BmcQueryRequest*)buffer;
                spdlog::info("收到BMC查询请求: (请求ID={})", request->requestId);

                // 发送BMC查询响应
                if (m_broadcaster) {
                    m_broadcaster->HandleBmcQueryRequest(*request);
                }
            }
        }
    }
}

}

