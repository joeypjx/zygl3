#pragma once

#include "value_objects.h"
#include <string>

namespace app::domain {

/**
 * @brief 任务实体
 * @detail 任务的运行实例，包含详细的资源使用情况
 */
class Task {
public:
    Task() = default;

    Task(const std::string& taskID, const std::string& taskStatus)
        : m_taskID(taskID), m_taskStatus(taskStatus) {
    }

    // Getters
    const std::string& GetTaskID() const { return m_taskID; }
    const std::string& GetTaskStatus() const { return m_taskStatus; }
    const ResourceUsage& GetResources() const { return m_resources; }
    const std::string& GetBoardAddress() const { return m_boardAddress; }

    /**
     * @brief 更新资源使用情况
     */
    void UpdateResources(const ResourceUsage& resources) {
        m_resources = resources;
    }

    /**
     * @brief 更新任务状态
     */
    void UpdateStatus(const std::string& status) {
        m_taskStatus = status;
    }

    /**
     * @brief 设置任务运行的板卡地址
     */
    void SetBoardAddress(const std::string& boardAddress) {
        m_boardAddress = boardAddress;
    }

private:
    std::string m_taskID;           // 任务ID
    std::string m_taskStatus;       // 任务状态
    std::string m_boardAddress;     // 任务运行的板卡IP地址
    
    ResourceUsage m_resources;      // 详细的资源使用情况
};

}