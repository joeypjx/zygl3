#pragma once

#include "src/domain/i_chassis_repository.h"
#include "src/domain/i_stack_repository.h"
#include "src/interfaces/udp/resource_monitor_broadcaster.h"
#include "src/infrastructure/api_client/qyw_api_client.h"
#include "src/infrastructure/ha/heartbeat_service.h"
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

// cpp-httplib 是单头文件库
#include "cpp-httplib/httplib.h"

namespace app::interfaces {

/**
 * @brief 板卡异常上报请求结构
 */
struct BoardAlertRequest {
    std::string chassisName;
    int chassisNumber;
    std::string boardName;
    int boardNumber;
    int boardType;
    std::string boardAddress;
    int boardStatus;  // 0-正常, 1-异常
    std::vector<std::string> alertMessages;
};

/**
 * @brief 组件异常上报请求中的任务告警信息
 */
struct TaskAlertInfo {
    std::string taskID;
    int taskStatus;  // 1-运行中，2-已完成，3-异常，0-其他
    std::string chassisName;
    int chassisNumber;
    std::string boardName;
    int boardNumber;
    int boardType;
    std::string boardAddress;
    int boardStatus;  // 0-正常, 1-异常
    std::vector<std::string> alertMessages;
    
    TaskAlertInfo() : taskStatus(0), chassisNumber(0), boardNumber(0), boardType(0), boardStatus(0) {}
};

/**
 * @brief 组件异常上报请求结构
 */
struct ServiceAlertRequest {
    std::string stackName;
    std::string stackUUID;
    std::string serviceName;
    std::string serviceUUID;
    std::vector<TaskAlertInfo> taskAlertInfos;
};

/**
 * @brief 告警接收服务器
 * @detail 接收板卡异常和组件异常的上报
 */
class AlertReceiverServer {
public:
    AlertReceiverServer(
        std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
        std::shared_ptr<app::domain::IStackRepository> stackRepo,
        std::shared_ptr<ResourceMonitorBroadcaster> broadcaster,
        std::shared_ptr<app::infrastructure::QywApiClient> apiClient,
        const std::string& clientIp,
        std::shared_ptr<app::infrastructure::HeartbeatService> heartbeatService = nullptr,  // 可选：心跳服务（用于角色检查）
        int port = 8888,
        const std::string& host = "0.0.0.0",
        int heartbeatInterval = 10  // 心跳间隔（秒）
    );

    ~AlertReceiverServer();

    /**
     * @brief 启动服务器
     */
    void Start();

    /**
     * @brief 停止服务器
     */
    void Stop();

    /**
     * @brief 是否正在运行
     */
    bool IsRunning() const { return m_running; }

private:
    /**
     * @brief 启动服务器线程
     */
    void ServerLoop();

    /**
     * @brief 心跳发送线程
     */
    void HeartbeatLoop();

    /**
     * @brief 发送IP心跳检测
     */
    void SendHeartbeat();

    /**
     * @brief 处理板卡异常上报
     */
    void HandleBoardAlert(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理组件异常上报
     */
    void HandleServiceAlert(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 发送成功响应
     */
    void SendSuccessResponse(httplib::Response& res);

    /**
     * @brief 发送错误响应
     */
    void SendErrorResponse(httplib::Response& res, const std::string& message);

private:
    std::shared_ptr<app::domain::IChassisRepository> m_chassisRepo;
    std::shared_ptr<app::domain::IStackRepository> m_stackRepo;
    std::shared_ptr<ResourceMonitorBroadcaster> m_broadcaster;
    std::shared_ptr<app::infrastructure::QywApiClient> m_apiClient;
    std::shared_ptr<app::infrastructure::HeartbeatService> m_heartbeatService;  // 心跳服务（用于角色检查）
    std::string m_clientIp;
    int m_port;
    std::string m_host;
    int m_heartbeatInterval;
    httplib::Server m_server;
    
    std::atomic<bool> m_running;
    std::thread m_serverThread;
    std::thread m_heartbeatThread;
};

}

