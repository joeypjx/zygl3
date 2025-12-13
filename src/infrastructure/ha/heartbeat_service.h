#pragma once

#include <string>
#include <atomic>
#include <cstdint>
#include <thread>
#include <chrono>
#include <functional>
#include <memory>

namespace app::infrastructure {

/**
 * @brief 心跳和角色协商服务（2节点主备场景）
 * @detail 负责主备节点的角色协商和心跳发送
 */
class HeartbeatService {
public:
    enum class Role {
        Unknown = 0,
        Primary = 1,
        Standby = 2
    };

    using RoleChangeCallback = std::function<void(Role oldRole, Role newRole)>;

    /**
     * @brief 构造函数
     * @param multicastGroup 组播地址
     * @param heartbeatPort 心跳端口
     * @param priority 优先级（数值越大优先级越高，0表示使用IP地址比较）
     * @param heartbeatInterval 心跳间隔（秒）
     * @param timeoutThreshold 超时阈值（秒，3次心跳）
     */
    HeartbeatService(
        const std::string& multicastGroup,
        uint16_t heartbeatPort,
        int priority = 0,
        int heartbeatInterval = 3,
        int timeoutThreshold = 9
    );

    ~HeartbeatService();

    /**
     * @brief 启动心跳服务
     * @param initialRole 初始角色（Unknown表示自动协商）
     */
    void Start(Role initialRole = Role::Unknown);

    /**
     * @brief 停止心跳服务
     */
    void Stop();

    /**
     * @brief 获取当前角色
     */
    Role GetCurrentRole() const { return m_currentRole.load(); }

    /**
     * @brief 是否为主节点
     */
    bool IsPrimary() const { return GetCurrentRole() == Role::Primary; }

    /**
     * @brief 设置角色变更回调
     */
    void SetRoleChangeCallback(RoleChangeCallback callback) {
        m_roleChangeCallback = callback;
    }

private:
    /**
     * @brief 心跳发送循环（主节点）
     */
    void HeartbeatLoop();

    /**
     * @brief 接收组播消息循环
     */
    void ReceiveLoop();

    /**
     * @brief 检查并切换角色
     */
    void CheckAndSwitchRole();

    /**
     * @brief 切换到主节点
     */
    void SwitchToPrimary();

    /**
     * @brief 切换到备节点
     */
    void SwitchToStandby();

    /**
     * @brief 发送选举公告
     */
    void SendElectionAnnouncement();

    /**
     * @brief 发送心跳消息
     */
    void SendHeartbeat();

    /**
     * @brief 发送角色声明
     */
    void SendRoleDeclaration();

    /**
     * @brief 判断是否应该让位给其他主节点
     */
    bool ShouldYieldToOtherPrimary(const struct MulticastHAMessage& msg);

    /**
     * @brief 比较节点ID（IP地址）
     */
    int CompareNodeId(const std::string& id1, const std::string& id2);

    /**
     * @brief 获取本地IP地址
     */
    std::string GetLocalIpAddress();

    /**
     * @brief 通知角色变更
     */
    void NotifyRoleChange(Role oldRole, Role newRole);

    // 配置参数
    std::string m_multicastGroup;
    uint16_t m_heartbeatPort;
    int m_priority;
    int m_heartbeatInterval;
    int m_timeoutThreshold;

    // 核心状态
    std::atomic<Role> m_currentRole;
    std::atomic<bool> m_running;
    std::atomic<std::chrono::system_clock::time_point> m_lastPrimaryHeartbeat;

    // 网络通信
    int m_socket;
    std::string m_localNodeId;

    // 线程管理
    std::thread m_heartbeatThread;
    std::thread m_receiveThread;

    // 回调
    RoleChangeCallback m_roleChangeCallback;

    // 序列号
    std::atomic<uint32_t> m_sequence;
};

/**
 * @brief 组播HA消息格式
 */
#pragma pack(push, 1)
struct MulticastHAMessage {
    uint16_t magic;         // 魔数 0xBEA7 (BEAT的ASCII编码)
    uint8_t msgType;        // 消息类型：1=选举公告, 2=心跳, 3=角色声明
    uint8_t role;           // 角色：1=Primary, 2=Standby, 0=Unknown
    int32_t priority;       // 优先级（网络字节序）
    uint32_t sequence;      // 序列号（网络字节序）
    uint64_t timestamp;     // 时间戳（网络字节序）
    char nodeId[32];        // 节点标识（IP地址）
};
#pragma pack(pop)

} // namespace app::infrastructure
