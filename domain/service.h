#pragma once

#include "task.h"
#include <string>
#include <map>
#include <optional>

namespace app::domain {

/**
 * @brief 组件实体
 * @detail 算法组件，包含多个任务实例
 */
class Service {
public:
    Service() = default;

    Service(const std::string& serviceUUID, const std::string& serviceName, int serviceType)
        : m_serviceUUID(serviceUUID), 
          m_serviceName(serviceName), 
          m_serviceStatus(0), 
          m_serviceType(serviceType) {
    }

    // Getters
    const std::string& GetServiceUUID() const { return m_serviceUUID; }
    const std::string& GetServiceName() const { return m_serviceName; }
    int GetServiceStatus() const { return m_serviceStatus; }
    int GetServiceType() const { return m_serviceType; }

    /**
     * @brief 添加或更新任务
     */
    void AddOrUpdateTask(const std::string& taskID, const Task& task) {
        m_tasks[taskID] = task;
    }

    /**
     * @brief 查找任务
     */
    std::optional<Task> FindTask(const std::string& taskID) const {
        auto it = m_tasks.find(taskID);
        if (it != m_tasks.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief 获取所有任务
     */
    const std::map<std::string, Task>& GetAllTasks() const {
        return m_tasks;
    }

    /**
     * @brief 更新组件状态
     */
    void UpdateStatus(int status) {
        m_serviceStatus = status;
    }

private:
    std::string m_serviceUUID;              // 算法组件UUID
    std::string m_serviceName;              // 算法组件名称
    int m_serviceStatus = 0;                // 组件状态：0-已停用；1-已启用；2-运行正常；3-运行异常
    int m_serviceType = 0;                  // 组件类型：0-普通组件；1-普通链路引用的公共组件；2-公共链路自有组件

    std::map<std::string, Task> m_tasks;  // 任务列表，K: taskID, V: Task
};

}