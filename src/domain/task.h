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

    Task(const std::string& taskID, int taskStatus)
        : m_taskID(taskID), m_taskStatus(taskStatus) {
    }

    // Getters
    const std::string& GetTaskID() const { return m_taskID; }
    int GetTaskStatus() const { return m_taskStatus; }
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
     * @param status 任务状态：1-运行中，2-已完成，3-异常，0-其他
     */
    void UpdateStatus(int status) {
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
    int m_taskStatus;               // 任务状态：1-运行中，2-已完成，3-异常，0-其他
    std::string m_boardAddress;     // 任务运行的板卡IP地址
    
    ResourceUsage m_resources;      // 详细的资源使用情况
};

}