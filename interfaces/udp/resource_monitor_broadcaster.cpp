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
    const std::string& multicastGroup,
    uint16_t port)
    : m_chassisRepo(chassisRepo)
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
            // 解析请求
            ResourceMonitorRequest* request = (ResourceMonitorRequest*)buffer;
            
            if (request->command == 0xF000) {
                std::cout << "收到资源监控请求: ID=" << request->requestId << std::endl;
                
                // 发送响应
                if (m_broadcaster) {
                    m_broadcaster->SendResponse(request->requestId);
                }
            }
        }
    }
}

}

