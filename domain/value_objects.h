#pragma once

#include <string>
#include <vector>

namespace app::domain {

/**
 * @brief 板卡类型
 */
enum class BoardType : int8_t {
    Computing = 0,              // 计算板卡 (通用，可运行任务)
    Switch = 1,                 // 交换板卡 (以太网交换模块)
    Power = 2,                   // 电源板卡 (电源模块)
    GeneralComputingI = 3,       // 通用计算I型模块 (可运行任务)
    GeneralComputingII = 4,      // 通用计算II型模块 (可运行任务)
    HighPerformanceComputingI = 5, // 高性能计算I型模块 (可运行任务)
    HighPerformanceComputingII = 6, // 高性能计算II型模块 (可运行任务)
    Storage = 7,                 // 存储模块 (不可运行任务)
    Cache = 8,                   // 缓存模块 (不可运行任务)
    SRIO = 9,                    // SRIO模块 (不可运行任务)
    EthernetSwitch = 10          // 以太网交换模块 (与Switch相同，不可运行任务)
};

/**
 * @brief 板卡运行状态
 */
enum class BoardOperationalStatus : int8_t {
    Unknown = 0,   // 启动时的初始状态
    Normal = 1,   // 从API获取到 0-正常
    Abnormal = 2, // 从API获取到 1-异常
    Offline = 3   // API中未上报此板卡，判定为离线
};

/**
 * @brief 任务状态信息
 */
struct TaskStatusInfo {
    std::string taskID;         // 任务ID
    int taskStatus;            // 任务状态：1-运行中，2-已完成，3-异常，0-其他
    std::string serviceName;   // 算法组件名称
    std::string serviceUUID;   // 算法组件唯一标识
    std::string stackName;     // 业务链路名称
    std::string stackUUID;     // 业务链路唯一标识
    
    TaskStatusInfo() : taskStatus(0) {}
};

/**
 * @brief 资源使用情况
 */
struct ResourceUsage {
    float cpuCores = 0.0f;      // CPU总量
    float cpuUsed = 0.0f;       // CPU使用量
    float cpuUsage = 0.0f;      // CPU使用率
    float memorySize = 0.0f;    // 内存总量
    float memoryUsed = 0.0f;    // 内存使用量
    float memoryUsage = 0.0f;   // 内存使用率
    float netReceive = 0.0f;    // 网络接收流量
    float netSent = 0.0f;       // 网络发送流量
    float gpuMemUsed = 0.0f;    // GPU显存使用情况
};

/**
 * @brief 位置信息
 */
struct LocationInfo {
    std::string chassisName;    // 机箱名称
    int chassisNumber = 0;      // 机箱号
    std::string boardName;      // 板卡名称
    int boardNumber = 0;        // 板卡槽位号
    std::string boardAddress;   // 板卡IP地址
};

/**
 * @brief 风扇信息
 */
struct FanSpeed {
    std::string fanName;  // 风扇名称
    float speed;          // 风扇转速
    
    FanSpeed() : speed(0.0f) {}
};

}