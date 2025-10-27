#pragma once

#include "domain/i_chassis_repository.h"
#include <string>
#include <memory>
#include <thread>
#include <atomic>

namespace app::interfaces {

/**
 * @brief 资源监控报文结构
 */
#pragma pack(push, 1)
struct ResourceMonitorRequest {
    char header[22];           // 报文头部
    uint16_t command;         // 命令码 F000H
    uint32_t requestId;       // 请求ID
};

struct ResourceMonitorResponse {
    char header[22];           // 报文头部 (22字节)
    uint16_t command;          // 命令码 F000H (2字节)
    uint32_t responseId;        // 响应ID (4字节)
    uint8_t boardStatus[108];   // 机箱板卡状态 (108字节 = 9机箱×12板卡)
    uint8_t taskStatus[864];    // 任务状态 (864字节 = 9机箱×12板卡×8任务)
};
#pragma pack(pop)

/**
 * @brief 资源监控广播器
 * @detail 通过UDP组播发送资源监控响应报文
 */
class ResourceMonitorBroadcaster {
public:
    ResourceMonitorBroadcaster(
        std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
        const std::string& multicastGroup = "234.186.1.99",
        uint16_t port = 0x100A  // 端口100AH
    );
    
    ~ResourceMonitorBroadcaster();

    /**
     * @brief 启动广播服务
     */
    void Start();

    /**
     * @brief 停止广播服务
     */
    void Stop();

    /**
     * @brief 发送资源监控响应
     * @param requestId 请求ID
     * @return 是否发送成功
     */
    bool SendResponse(uint32_t requestId);

private:
    /**
     * @brief 从仓储构建响应数据
     */
    void BuildResponseData(ResourceMonitorResponse& response);

    /**
     * @brief 将板卡状态映射到字节数组
     * @detail 9个机箱×12块板卡 = 108字节
     * @note 根据之前的设计，每个机箱有14块板卡，但协议要求12块，取前12块
     */
    void MapBoardStatusToArray(uint8_t* array, int chassisNumber);

    /**
     * @brief 将任务状态映射到字节数组
     * @detail 9个机箱×12块板卡×8个任务 = 864字节
     */
    void MapTaskStatusToArray(uint8_t* array, int chassisNumber);

private:
    std::shared_ptr<app::domain::IChassisRepository> m_chassisRepo;
    std::string m_multicastGroup;
    uint16_t m_port;
    int m_socket;
    
    std::atomic<bool> m_running;
    std::atomic<uint32_t> m_nextResponseId;
    
    ResourceMonitorResponse m_responseBuffer;
};

/**
 * @brief 资源监控监听器
 * @detail 通过UDP组播接收资源监控请求报文
 */
class ResourceMonitorListener {
public:
    ResourceMonitorListener(
        std::shared_ptr<ResourceMonitorBroadcaster> broadcaster,
        const std::string& multicastGroup = "234.186.1.98",
        uint16_t port = 0x100A  // 端口100AH
    );
    
    ~ResourceMonitorListener();

    /**
     * @brief 启动监听服务
     */
    void Start();

    /**
     * @brief 停止监听服务
     */
    void Stop();

private:
    /**
     * @brief 监听循环
     */
    void ListenLoop();

private:
    std::shared_ptr<ResourceMonitorBroadcaster> m_broadcaster;
    std::string m_multicastGroup;
    uint16_t m_port;
    int m_socket;
    
    std::atomic<bool> m_running;
    std::thread m_listenThread;
};

}

