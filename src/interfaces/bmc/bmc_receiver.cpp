#include "bmc_receiver.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <cerrno>

namespace app::interfaces {

BmcReceiver::BmcReceiver(
    std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
    const std::string& multicastGroup,
    uint16_t port)
    : m_chassisRepo(chassisRepo)
    , m_multicastGroup(multicastGroup)
    , m_port(port)
    , m_socket(-1)
    , m_running(false) {
    
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
    addr.sin_port = htons(m_port);
    
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
    
    spdlog::info("BMC接收器初始化成功 (组播地址: {}:{})", m_multicastGroup, m_port);
}

BmcReceiver::~BmcReceiver() {
    Stop();
    if (m_socket >= 0) {
        // 退出组播组
        struct ip_mreq mreq;
        if (inet_pton(AF_INET, m_multicastGroup.c_str(), &mreq.imr_multiaddr) > 0) {
            mreq.imr_interface.s_addr = INADDR_ANY;
            setsockopt(m_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
        }
        close(m_socket);
    }
}

void BmcReceiver::Start() {
    if (m_running) {
        spdlog::warn("BMC接收器已在运行");
        return;
    }
    
    if (m_socket < 0) {
        spdlog::error("socket无效，无法启动接收服务");
        return;
    }
    
    m_running = true;
    m_receiveThread = std::thread(&BmcReceiver::ReceiveLoop, this);
    
    spdlog::info("BMC接收器已启动 (组播地址: {}:{})", m_multicastGroup, m_port);
}

void BmcReceiver::Stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
    
    spdlog::info("BMC接收器已停止");
}

void BmcReceiver::ReceiveLoop() {
    spdlog::info("开始接收BMC组播数据...");
    
    // 缓冲区大小设置为UdpInfo结构体大小（约1600字节）
    char buffer[2048];
    
    while (m_running) {
        struct sockaddr_in senderAddr;
        socklen_t addrLen = sizeof(senderAddr);
        
        ssize_t recvLen = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                                    (struct sockaddr*)&senderAddr, &addrLen);
        
        if (recvLen < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            spdlog::error("接收UDP数据失败: {}", strerror(errno));
            continue;
        }
        
        if (recvLen > 0) {
            char senderIpStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &senderAddr.sin_addr, senderIpStr, INET_ADDRSTRLEN);
            spdlog::debug("收到UDP数据包: 来源 {}:{}, 长度 {} 字节", 
                         senderIpStr, ntohs(senderAddr.sin_port), recvLen);
            
            // 处理接收到的数据包
            HandleReceivedPacket(buffer, recvLen);
        }
    }
}

void BmcReceiver::HandleReceivedPacket(const char* data, size_t length) {
    // 检查最小长度（至少需要包含UdpInfo的基本字段）
    if (length < sizeof(UdpInfo)) {
        spdlog::warn("接收到的数据包长度不足: {} 字节，期望至少 {} 字节", 
                    length, sizeof(UdpInfo));
        return;
    }
    
    // 将接收到的数据转换为UdpInfo结构
    const UdpInfo* info = reinterpret_cast<const UdpInfo*>(data);
    
    // 验证报文格式
    if (!ValidatePacket(info)) {
        spdlog::warn("UDP报文验证失败");
        return;
    }
    
    // 打印接收到的BMC信息
    spdlog::debug("收到BMC报文: 机箱号={}, 报文编号={}, 时间戳={}, 报文长度={}", 
                info->boxid, info->seqnum, info->timestamp, info->msglenth);
    
    // 根据BMC报文更新机箱存储中的板卡状态
    if (m_chassisRepo) {
        // 构建槽位号到在位状态的映射
        // 注意：board[10] 对应槽位 1,2,3,4,6,7,9,10,11,12（没有槽位5和8）
        std::map<int, bool> presenceMap;
        std::vector<int> absentSlots;  // 记录不在位的槽位
        
        for (int i = 0; i < 10; ++i) {
            uint8_t slotNumber = info->board[i].ipmbaddr;
            if (slotNumber != 0) {  // 有效的槽位号
                // prst: 0表示不在位，1表示在位
                bool isPresent = (info->board[i].prst == 1);
                presenceMap[slotNumber] = isPresent;
                
                // 记录不在位的槽位
                if (!isPresent) {
                    absentSlots.push_back(slotNumber);
                }
            }
        }
        
        // 如果有板卡不在位，打印日志
        if (!absentSlots.empty()) {
            int chassisNumber = info->boxid;
            std::string absentSlotsStr;
            for (size_t j = 0; j < absentSlots.size(); ++j) {
                absentSlotsStr += std::to_string(absentSlots[j]);
                if (j < absentSlots.size() - 1) {
                    absentSlotsStr += ",";
                }
            }
            spdlog::warn("BMC报文检测到机箱 {} 的板卡不在位: 槽位 {}", 
                        chassisNumber, absentSlotsStr);
        }
        
        // 批量更新板卡状态
        if (!presenceMap.empty()) {
            int chassisNumber = info->boxid;
            size_t updatedCount = m_chassisRepo->UpdateAllBoardsStatus(chassisNumber, presenceMap);
            spdlog::info("根据BMC报文更新机箱 {} 的 {} 个板卡状态", chassisNumber, updatedCount);
        }
    } else {
        spdlog::warn("BMC接收器未设置机箱仓储，无法更新板卡状态");
    }
}

bool BmcReceiver::ValidatePacket(const UdpInfo* info) {
    // 验证报文头
    if (info->head != 0x5AA5) {
        spdlog::warn("无效的报文头: 0x{:04X}, 期望 0x5AA5", info->head);
        return false;
    }
    
    // 验证报文尾
    if (info->tail != 0xA55A) {
        spdlog::warn("无效的报文尾: 0x{:04X}, 期望 0xA55A", info->tail);
        return false;
    }
    
    // 验证报文类型
    if (info->msgtype != 0x0002) {
        spdlog::warn("无效的报文类型: 0x{:04X}, 期望 0x0002", info->msgtype);
        return false;
    }
    
    // 验证报文长度（应该等于UdpInfo结构体大小）
    if (info->msglenth != sizeof(UdpInfo)) {
        spdlog::warn("报文长度不匹配: {}, 期望 {}", info->msglenth, sizeof(UdpInfo));
        return false;
    }
    
    return true;
}

}

