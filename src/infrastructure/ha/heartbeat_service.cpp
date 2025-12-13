#include "heartbeat_service.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <ctime>

// 字节序转换函数（64位大端到主机字节序）
static inline uint64_t be64toh_custom(uint64_t value) {
    // 检查系统字节序
    union {
        uint32_t i;
        char c[4];
    } test = {0x01020304};
    
    if (test.c[0] == 1) {
        // 大端系统，不需要转换
        return value;
    } else {
        // 小端系统，需要转换
        return ((value & 0xFF00000000000000ULL) >> 56) |
               ((value & 0x00FF000000000000ULL) >> 40) |
               ((value & 0x0000FF0000000000ULL) >> 24) |
               ((value & 0x000000FF00000000ULL) >> 8)  |
               ((value & 0x00000000FF000000ULL) << 8)  |
               ((value & 0x0000000000FF0000ULL) << 24) |
               ((value & 0x000000000000FF00ULL) << 40) |
               ((value & 0x00000000000000FFULL) << 56);
    }
}

// 字节序转换函数（主机字节序到64位大端）
static inline uint64_t htobe64_custom(uint64_t value) {
    return be64toh_custom(value);  // 对称操作
}

namespace app::infrastructure {

HeartbeatService::HeartbeatService(
    const std::string& multicastGroup,
    uint16_t heartbeatPort,
    int priority,
    int heartbeatInterval,
    int timeoutThreshold)
    : m_multicastGroup(multicastGroup)
    , m_heartbeatPort(heartbeatPort)
    , m_priority(priority)
    , m_heartbeatInterval(heartbeatInterval)
    , m_timeoutThreshold(timeoutThreshold)
    , m_currentRole(Role::Unknown)
    , m_running(false)
    , m_lastPrimaryHeartbeat(std::chrono::system_clock::time_point{})
    , m_socket(-1)
    , m_sequence(0) {
    
    // 获取本地IP地址
    m_localNodeId = GetLocalIpAddress();
    if (m_localNodeId.empty()) {
        spdlog::warn("无法获取本地IP地址，使用默认值");
        m_localNodeId = "0.0.0.0";
    }
    
    // 创建socket
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        spdlog::error("创建UDP socket失败: {}", strerror(errno));
        return;
    }
    
    // 设置socket选项，允许端口复用
    int reuse = 1;
    if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        spdlog::error("设置SO_REUSEADDR失败: {}", strerror(errno));
        close(m_socket);
        m_socket = -1;
        return;
    }
    
    // 绑定地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_heartbeatPort);
    
    if (bind(m_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        spdlog::error("绑定地址失败: {}", strerror(errno));
        close(m_socket);
        m_socket = -1;
        return;
    }
    
    // 加入组播组
    struct ip_mreq mreq;
    if (inet_pton(AF_INET, m_multicastGroup.c_str(), &mreq.imr_multiaddr) <= 0) {
        spdlog::error("无效的组播地址: {}", m_multicastGroup);
        close(m_socket);
        m_socket = -1;
        return;
    }
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        spdlog::error("加入组播组失败: {}", strerror(errno));
        close(m_socket);
        m_socket = -1;
        return;
    }
    
    // 设置组播TTL（限制在同一网段）
    int ttl = 1;
    setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    
    spdlog::info("心跳服务初始化成功 (组播地址: {}:{}, 本地IP: {}, 优先级: {})",
                 m_multicastGroup, m_heartbeatPort, m_localNodeId, m_priority);
}

HeartbeatService::~HeartbeatService() {
    Stop();
    if (m_socket >= 0) {
        close(m_socket);
    }
}

void HeartbeatService::Start(Role initialRole) {
    if (m_running) {
        spdlog::warn("心跳服务已在运行");
        return;
    }
    
    if (m_socket < 0) {
        spdlog::error("socket无效，无法启动心跳服务");
        return;
    }
    
    m_running = true;
    
    // 如果指定了初始角色，直接设置；否则从Unknown开始，等待协商
    if (initialRole != Role::Unknown) {
        m_currentRole = initialRole;
        spdlog::info("心跳服务启动，初始角色: {}",
                     initialRole == Role::Primary ? "Primary" : "Standby");
    } else {
        m_currentRole = Role::Unknown;
        spdlog::info("心跳服务启动，等待角色协商...");
        
        // 发送选举公告
        SendElectionAnnouncement();
        
        // 等待一段时间收集响应
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // 检查是否收到主节点心跳
        auto now = std::chrono::system_clock::now();
        auto lastHeartbeat = m_lastPrimaryHeartbeat.load();
        
        if (lastHeartbeat != std::chrono::system_clock::time_point{} &&
            (now - lastHeartbeat) < std::chrono::seconds(5)) {
            // 收到主节点心跳，本节点是备节点
            SwitchToStandby();
        } else {
            // 未收到主节点心跳，本节点成为主节点
            SwitchToPrimary();
        }
    }
    
    // 启动接收线程
    m_receiveThread = std::thread(&HeartbeatService::ReceiveLoop, this);
    
    // 如果当前是主节点，且心跳线程未启动，启动心跳发送线程
    // 注意：如果通过SwitchToPrimary()切换角色，线程已经在SwitchToPrimary()中启动
    if (m_currentRole.load() == Role::Primary && !m_heartbeatThread.joinable()) {
        m_heartbeatThread = std::thread(&HeartbeatService::HeartbeatLoop, this);
    }
    
    spdlog::info("心跳服务已启动，当前角色: {}",
                 m_currentRole.load() == Role::Primary ? "Primary" : 
                 m_currentRole.load() == Role::Standby ? "Standby" : "Unknown");
}

void HeartbeatService::Stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.join();
    }
    
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
    
    spdlog::info("心跳服务已停止");
}

void HeartbeatService::HeartbeatLoop() {
    spdlog::info("心跳发送线程启动");
    
    while (m_running) {
        // 只有主节点才发送心跳
        if (m_currentRole.load() == Role::Primary) {
            SendHeartbeat();
        }
        
        // 等待心跳间隔
        for (int i = 0; i < m_heartbeatInterval && m_running; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    spdlog::info("心跳发送线程结束");
}

void HeartbeatService::ReceiveLoop() {
    spdlog::info("心跳接收线程启动");
    
    char buffer[1024];
    
    while (m_running) {
        struct sockaddr_in fromAddr;
        socklen_t addrLen = sizeof(fromAddr);
        
        // 设置接收超时（1秒）
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        
        ssize_t recvLen = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                                   (struct sockaddr*)&fromAddr, &addrLen);
        
        if (recvLen == sizeof(MulticastHAMessage)) {
            MulticastHAMessage* msg = reinterpret_cast<MulticastHAMessage*>(buffer);
            
            // 验证魔数 (0xBEA7 = 48807，使用"BEAT"的ASCII编码)
            // 0x42='B', 0x45='E', 0x41='A', 0x54='T' -> 0xBEA7
            uint16_t magicValue = 0xBEA7;
            if (msg->magic != magicValue) {
                continue;
            }
            
            // 转换字节序
            msg->priority = ntohl(msg->priority);
            msg->sequence = ntohl(msg->sequence);
            msg->timestamp = be64toh_custom(msg->timestamp);
            
            std::string senderNodeId(msg->nodeId);
            
            if (msg->msgType == 2 && msg->role == 1) {  // 收到主节点心跳
                // 更新最后收到心跳的时间
                m_lastPrimaryHeartbeat = std::chrono::system_clock::now();
                
                // 如果本节点也是主节点，且收到其他主节点心跳，需要处理冲突
                if (m_currentRole.load() == Role::Primary && 
                    senderNodeId != m_localNodeId) {
                    // 比较优先级，决定是否降级
                    if (ShouldYieldToOtherPrimary(*msg)) {
                        spdlog::warn("检测到其他主节点 ({}), 降级为备节点", senderNodeId);
                        SwitchToStandby();
                    }
                } else if (m_currentRole.load() == Role::Standby) {
                    // 备节点收到主节点心跳，正常情况
                    spdlog::debug("收到主节点心跳: {}", senderNodeId);
                }
            } else if (msg->msgType == 1) {  // 收到选举公告
                // 如果本节点是主节点，发送角色声明
                if (m_currentRole.load() == Role::Primary) {
                    spdlog::debug("收到选举公告，发送角色声明");
                    SendRoleDeclaration();
                }
            } else if (msg->msgType == 3) {  // 收到角色声明
                // 如果声明的是主节点，且本节点也是主节点，需要处理冲突
                if (msg->role == 1 && m_currentRole.load() == Role::Primary &&
                    senderNodeId != m_localNodeId) {
                    if (ShouldYieldToOtherPrimary(*msg)) {
                        spdlog::warn("收到其他主节点角色声明 ({}), 降级为备节点", senderNodeId);
                        SwitchToStandby();
                    }
                }
            }
        }
        
        // 定期检查角色
        CheckAndSwitchRole();
    }
    
    spdlog::info("心跳接收线程结束");
}

void HeartbeatService::CheckAndSwitchRole() {
    auto now = std::chrono::system_clock::now();
    auto lastHeartbeat = m_lastPrimaryHeartbeat.load();
    auto timeout = std::chrono::seconds(m_timeoutThreshold);
    
    if (m_currentRole.load() == Role::Standby) {
        // 备节点：检查是否超时未收到主节点心跳
        if (lastHeartbeat == std::chrono::system_clock::time_point{} ||
            (now - lastHeartbeat) > timeout) {
            // 主节点故障，本节点成为主节点
            spdlog::info("检测到主节点故障（超时{}秒），切换为主节点", m_timeoutThreshold);
            SwitchToPrimary();
        }
    }
}

void HeartbeatService::SwitchToPrimary() {
    Role oldRole = m_currentRole.load();
    if (oldRole == Role::Primary) {
        return;  // 已经是主节点
    }
    
    m_currentRole = Role::Primary;
    
    spdlog::info("角色切换: {} -> Primary",
                 oldRole == Role::Standby ? "Standby" : "Unknown");
    
    // 发送角色声明
    SendRoleDeclaration();
    
    // 如果心跳线程未启动，启动它
    if (!m_heartbeatThread.joinable()) {
        m_heartbeatThread = std::thread(&HeartbeatService::HeartbeatLoop, this);
    }
    
    // 通知角色变更
    NotifyRoleChange(oldRole, Role::Primary);
}

void HeartbeatService::SwitchToStandby() {
    Role oldRole = m_currentRole.load();
    if (oldRole == Role::Standby) {
        return;  // 已经是备节点
    }
    
    m_currentRole = Role::Standby;
    
    spdlog::info("角色切换: {} -> Standby",
                 oldRole == Role::Primary ? "Primary" : "Unknown");
    
    // 发送角色声明
    SendRoleDeclaration();
    
    // 停止心跳发送线程（如果正在运行）
    // 注意：线程会在下次检查角色时自然退出，这里不需要强制停止
    
    // 通知角色变更
    NotifyRoleChange(oldRole, Role::Standby);
}

void HeartbeatService::SendElectionAnnouncement() {
    if (m_socket < 0) {
        return;
    }
    
    MulticastHAMessage msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.magic = 0xBEA7;  // 魔数 0xBEA7 (BEAT的ASCII编码)
    msg.msgType = 1;  // 选举公告
    msg.role = 0;     // Unknown
    msg.priority = htonl(m_priority);
    msg.sequence = htonl(m_sequence.fetch_add(1));
    
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    msg.timestamp = htobe64_custom(seconds.count());
    
    strncpy(msg.nodeId, m_localNodeId.c_str(), sizeof(msg.nodeId) - 1);
    msg.nodeId[sizeof(msg.nodeId) - 1] = '\0';
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_heartbeatPort);
    inet_pton(AF_INET, m_multicastGroup.c_str(), &addr.sin_addr);
    
    sendto(m_socket, &msg, sizeof(msg), 0,
           (struct sockaddr*)&addr, sizeof(addr));
    
    spdlog::debug("发送选举公告");
}

void HeartbeatService::SendHeartbeat() {
    if (m_socket < 0 || m_currentRole.load() != Role::Primary) {
        return;
    }
    
    MulticastHAMessage msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.magic = 0xBEA7;  // 魔数 0xBEA7 (BEAT的ASCII编码)
    msg.msgType = 2;  // 心跳
    msg.role = 1;    // Primary
    msg.priority = htonl(m_priority);
    msg.sequence = htonl(m_sequence.fetch_add(1));
    
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    msg.timestamp = htobe64_custom(seconds.count());
    
    strncpy(msg.nodeId, m_localNodeId.c_str(), sizeof(msg.nodeId) - 1);
    msg.nodeId[sizeof(msg.nodeId) - 1] = '\0';
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_heartbeatPort);
    inet_pton(AF_INET, m_multicastGroup.c_str(), &addr.sin_addr);
    
    sendto(m_socket, &msg, sizeof(msg), 0,
           (struct sockaddr*)&addr, sizeof(addr));
    
    spdlog::debug("发送心跳 (序列号: {})", m_sequence.load() - 1);
}

void HeartbeatService::SendRoleDeclaration() {
    if (m_socket < 0) {
        return;
    }
    
    MulticastHAMessage msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.magic = 0xBEA7;  // 魔数 0xBEA7 (BEAT的ASCII编码)
    msg.msgType = 3;  // 角色声明
    msg.role = (m_currentRole.load() == Role::Primary) ? 1 : 2;
    msg.priority = htonl(m_priority);
    msg.sequence = htonl(m_sequence.fetch_add(1));
    
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    msg.timestamp = htobe64_custom(seconds.count());
    
    strncpy(msg.nodeId, m_localNodeId.c_str(), sizeof(msg.nodeId) - 1);
    msg.nodeId[sizeof(msg.nodeId) - 1] = '\0';
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_heartbeatPort);
    inet_pton(AF_INET, m_multicastGroup.c_str(), &addr.sin_addr);
    
    sendto(m_socket, &msg, sizeof(msg), 0,
           (struct sockaddr*)&addr, sizeof(addr));
    
    spdlog::debug("发送角色声明: {}", 
                  m_currentRole.load() == Role::Primary ? "Primary" : "Standby");
}

bool HeartbeatService::ShouldYieldToOtherPrimary(const MulticastHAMessage& msg) {
    // 比较优先级
    if (msg.priority > m_priority) {
        return true;  // 其他节点优先级更高
    }
    
    if (msg.priority == m_priority) {
        // 优先级相同，比较IP地址
        std::string otherNodeId(msg.nodeId);
        if (CompareNodeId(otherNodeId, m_localNodeId) < 0) {
            return true;  // 其他节点IP更小
        }
    }
    
    return false;
}

int HeartbeatService::CompareNodeId(const std::string& id1, const std::string& id2) {
    struct in_addr addr1, addr2;
    if (inet_pton(AF_INET, id1.c_str(), &addr1) != 1 ||
        inet_pton(AF_INET, id2.c_str(), &addr2) != 1) {
        // 如果IP地址解析失败，使用字符串比较
        return id1.compare(id2);
    }
    
    // 将IP地址转换为数值进行比较
    uint32_t ip1 = ntohl(addr1.s_addr);
    uint32_t ip2 = ntohl(addr2.s_addr);
    
    if (ip1 < ip2) {
        return -1;
    } else if (ip1 > ip2) {
        return 1;
    } else {
        return 0;
    }
}

std::string HeartbeatService::GetLocalIpAddress() {
    // 尝试从网络接口获取本地IP地址
    struct ifaddrs* ifaddr = nullptr;
    std::string localIp;
    
    if (getifaddrs(&ifaddr) == 0) {
        // 优先查找非回环的IPv4地址
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                struct sockaddr_in* sin = (struct sockaddr_in*)ifa->ifa_addr;
                
                // 跳过回环地址
                if (sin->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
                    continue;
                }
                
                // 转换为字符串
                char ipStr[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &sin->sin_addr, ipStr, INET_ADDRSTRLEN)) {
                    localIp = ipStr;
                    // 优先选择非127.0.0.1的地址
                    if (localIp != "127.0.0.1") {
                        break;
                    }
                }
            }
        }
        
        freeifaddrs(ifaddr);
    }
    
    // 如果没找到，尝试从socket获取
    if (localIp.empty() && m_socket >= 0) {
        struct sockaddr_in localAddr;
        socklen_t addrLen = sizeof(localAddr);
        if (getsockname(m_socket, (struct sockaddr*)&localAddr, &addrLen) == 0) {
            char ipStr[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &localAddr.sin_addr, ipStr, INET_ADDRSTRLEN)) {
                localIp = ipStr;
            }
        }
    }
    
    return localIp;
}

void HeartbeatService::NotifyRoleChange(Role oldRole, Role newRole) {
    if (m_roleChangeCallback) {
        try {
            m_roleChangeCallback(oldRole, newRole);
        } catch (const std::exception& e) {
            spdlog::error("角色变更回调执行失败: {}", e.what());
        }
    }
}

}