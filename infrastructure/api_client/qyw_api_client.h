#pragma once

#include <string>
#include <vector>
#include <memory>

// cpp-httplib 是单头文件库
#include "httplib.h"

namespace app::infrastructure {

/**
 * @brief 外部API响应数据结构
 */
struct BoardInfoResponse {
    std::string chassisName;
    int chassisNumber;
    std::string boardName;
    int boardNumber;
    int boardType;
    std::string boardAddress;
    int boardStatus;  // 0-正常, 1-异常
    
    struct TaskInfo {
        std::string taskID;
        std::string taskStatus;
        std::string serviceName;
        std::string serviceUUID;
        std::string stackName;
        std::string stackUUID;
    };
    std::vector<TaskInfo> taskInfos;
    
    BoardInfoResponse() : chassisNumber(0), boardNumber(0), boardType(0), boardStatus(0) {}
};

struct LabelInfo {
    std::string stackLabelName;
    std::string stackLabelUUID;
};

struct ServiceTaskInfo {
    std::string taskID;
    std::string taskStatus;
    float cpuCores;
    float cpuUsed;
    float cpuUsage;
    float memorySize;
    float memoryUsed;
    float memoryUsage;
    float netReceive;
    float netSent;
    float gpuMemUsed;
    std::string chassisName;
    int chassisNumber;
    std::string boardName;
    int boardNumber;
    std::string boardAddress;
    
    ServiceTaskInfo() : chassisNumber(0), boardNumber(0) {}
};

struct ServiceInfo {
    std::string serviceName;
    std::string serviceUUID;
    int serviceStatus;    // 0-已停用, 1-已启用, 2-运行正常, 3-运行异常
    int serviceType;      // 0-普通组件, 1-普通链路引用的公共组件, 2-公共链路自有组件
    std::vector<ServiceTaskInfo> taskInfos;
    
    ServiceInfo() : serviceStatus(0), serviceType(0) {}
};

struct StackInfoResponse {
    std::string stackName;
    std::string stackUUID;
    std::vector<LabelInfo> stackLabelInfos;
    int stackDeployStatus;   // 0-未部署, 1-已部署
    int stackRunningStatus;  // 1-正常运行, 2-异常运行
    std::vector<ServiceInfo> serviceInfos;
    
    StackInfoResponse() : stackDeployStatus(0), stackRunningStatus(0) {}
};

/**
 * @brief 业务链路部署/停用结果信息
 */
struct StackOperationInfo {
    std::string stackName;    // 业务链路名称
    std::string stackUUID;    // 业务链路UUID
    std::string message;      // 详细信息

    StackOperationInfo() = default;
};

/**
 * @brief Deploy/Undeploy 操作响应结果
 */
struct DeployResponse {
    std::vector<StackOperationInfo> successStackInfos;  // 成功的业务链路信息
    std::vector<StackOperationInfo> failureStackInfos;  // 失败的业务链路信息

    DeployResponse() = default;
};

/**
 * @brief 715平台API客户端
 * @detail 封装对上游API的调用，使用cpp-httplib
 */
class QywApiClient {
public:
    QywApiClient(const std::string& baseUrl, int port);
    ~QywApiClient() = default;

    /**
     * @brief 获取所有板卡信息和状态
     * @return 板卡信息列表
     */
    std::vector<BoardInfoResponse> GetBoardInfo();

    /**
     * @brief 获取所有业务链路详情
     * @return 业务链路信息列表
     */
    std::vector<StackInfoResponse> GetStackInfo();

    /**
     * @brief 批量启用业务链路
     * @param labels 业务链路标签列表
     * @return 部署结果，包含成功和失败的业务链路详情
     */
    DeployResponse DeployStacks(const std::vector<std::string>& labels);

    /**
     * @brief 批量停用业务链路
     * @param labels 业务链路标签列表
     * @return 停用结果，包含成功和失败的业务链路详情
     */
    DeployResponse UndeployStacks(const std::vector<std::string>& labels);

    /**
     * @brief 发送心跳保活
     * @param clientIp 接口调用者的IP地址
     * @return 是否发送成功
     */
    bool SendHeartbeat(const std::string& clientIp);

private:
    std::string m_baseUrl;
    int m_port;
    httplib::Client m_client;

    /**
     * @brief 解析板卡信息的JSON响应
     */
    std::vector<BoardInfoResponse> ParseBoardInfoResponse(const std::string& jsonStr);

    /**
     * @brief 解析业务链路信息的JSON响应
     */
    std::vector<StackInfoResponse> ParseStackInfoResponse(const std::string& jsonStr);

    /**
     * @brief 解析部署/停用操作的JSON响应
     */
    DeployResponse ParseDeployResponse(const std::string& jsonStr);
};

}
