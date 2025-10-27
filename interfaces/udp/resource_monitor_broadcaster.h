#pragma once

#include "domain/i_chassis_repository.h"
#include "domain/i_stack_repository.h"
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

/**
 * @brief 任务查看请求报文
 */
struct TaskQueryRequest {
    char header[22];           // 报文头部
    uint16_t command;         // 命令码 F005H
    uint32_t requestId;       // 请求ID
    uint16_t chassisNumber;   // 机箱号
    uint16_t boardNumber;     // 板卡号
    uint16_t taskIndex;       // 任务序号
};

/**
 * @brief 任务查看响应报文
 */
struct TaskQueryResponse {
    char header[22];           // 报文头部
    uint16_t command;         // 命令码 F105H
    uint32_t responseId;      // 响应ID
    uint16_t taskStatus;      // 任务状态 0:正常 1:异常
    uint32_t taskId;          // 任务ID
    uint16_t workMode;        // 工作模式
    uint32_t boardIp;         // 板卡IP
    uint16_t cpuUsage;        // CPU使用率 0-1000千分比
    uint32_t memoryUsage;     // 内存使用率
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
        std::shared_ptr<app::domain::IStackRepository> stackRepo,
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

    /**
     * @brief 发送任务查询响应
     * @param request 任务查询请求
     * @return 是否发送成功
     */
    bool SendTaskQueryResponse(const TaskQueryRequest& request);

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

    /**
     * @brief 构建任务查询响应
     * @detail 通过机箱号、板卡号、任务序号查找taskID，再查询资源使用情况
     */
    void BuildTaskQueryResponse(TaskQueryResponse& response, const TaskQueryRequest& request);

    /**
     * @brief 将IP地址字符串转换为uint32
     */
    uint32_t IpStringToUint32(const std::string& ipStr);

private:
    std::shared_ptr<app::domain::IChassisRepository> m_chassisRepo;
    std::shared_ptr<app::domain::IStackRepository> m_stackRepo;
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

