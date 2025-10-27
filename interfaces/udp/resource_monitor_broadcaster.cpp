#include "resource_monitor_broadcaster.h"
#include "domain/chassis.h"
#include "domain/board.h"
#include "domain/value_objects.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <cerrno>

namespace app::interfaces {

// ResourceMonitorBroadcaster 实现
ResourceMonitorBroadcaster::ResourceMonitorBroadcaster(
    std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
    std::shared_ptr<app::domain::IStackRepository> stackRepo,
    const std::string& multicastGroup,
    uint16_t port)
    : m_chassisRepo(chassisRepo)
    , m_stackRepo(stackRepo)
    , m_multicastGroup(multicastGroup)
    , m_port(port)
    , m_socket(-1)
    , m_running(false)
    , m_nextResponseId(0) {

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
    response.command = 0xF000;
    
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
                if (tasks[taskIdx].taskStatus == "running" || tasks[taskIdx].taskStatus == "正常") {
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
    response.command = 0xF105;

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

                if (command == 0xF000 && recvLen >= sizeof(ResourceMonitorRequest)) {
                    // 资源监控请求
                    ResourceMonitorRequest* request = (ResourceMonitorRequest*)buffer;
                    std::cout << "收到资源监控请求: ID=" << request->requestId << std::endl;

                    // 发送响应
                    if (m_broadcaster) {
                        m_broadcaster->SendResponse(request->requestId);
                    }
                }
                else if (command == 0xF005 && recvLen >= sizeof(TaskQueryRequest)) {
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
            }
        }
    }
}

}

