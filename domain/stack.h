#pragma once

#include "service.h"
#include "value_objects.h"
#include <string>
#include <map>
#include <vector>
#include <optional>

namespace app::domain {

/**
 * @brief 业务链路标签信息
 */
struct StackLabelInfo {
    std::string stackLabelName;   // 业务链路标签名称
    std::string stackLabelUUID;  // 业务链路标签UUID
};

/**
 * @brief 业务链路聚合根
 * @detail 业务链路包含多个组件，每个组件包含多个任务
 */
class Stack {
public:
    Stack() = default;

    Stack(const std::string& stackUUID, const std::string& stackName)
        : m_stackUUID(stackUUID), 
          m_stackName(stackName), 
          m_stackDeployStatus(0), 
          m_stackRunningStatus(0) {
    }

    // Getters
    const std::string& GetStackUUID() const { return m_stackUUID; }
    const std::string& GetStackName() const { return m_stackName; }
    int GetDeployStatus() const { return m_stackDeployStatus; }
    int GetRunningStatus() const { return m_stackRunningStatus; }
    const std::vector<StackLabelInfo>& GetLabels() const { return m_stackLabels; }

    /**
     * @brief 添加或更新组件
     */
    void AddOrUpdateService(const Service& service) {
        m_services[service.GetServiceUUID()] = service;
        // 可以在这里添加逻辑来重新计算整体运行状态
    }

    /**
     * @brief 查找组件
     */
    std::optional<Service> FindService(const std::string& serviceUUID) const {
        auto it = m_services.find(serviceUUID);
        if (it != m_services.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief 获取所有组件
     */
    const std::map<std::string, Service>& GetAllServices() const {
        return m_services;
    }

    /**
     * @brief 为第二个接口提供支持：通过taskID查找任务的资源使用情况
     * @param taskID 任务ID
     * @return 任务的资源使用情况，如果未找到返回nullopt
     */
    std::optional<ResourceUsage> GetTaskResources(const std::string& taskID) const {
        for (const auto& pair : m_services) {
            auto task_opt = pair.second.FindTask(taskID);
            if (task_opt) {
                return task_opt->GetResources();
            }
        }
        return std::nullopt;
    }

    /**
     * @brief 更新部署状态
     */
    void UpdateDeployStatus(int status) {
        m_stackDeployStatus = status;
    }

    /**
     * @brief 更新运行状态
     */
    void UpdateRunningStatus(int status) {
        m_stackRunningStatus = status;
    }

    /**
     * @brief 设置标签信息
     */
    void SetLabels(const std::vector<StackLabelInfo>& labels) {
        m_stackLabels = labels;
    }

private:
    std::string m_stackUUID;                      // 业务链路UUID
    std::string m_stackName;                      // 业务链路名称
    int m_stackDeployStatus = 0;                   // 部署状态：0-未部署；1-已部署
    int m_stackRunningStatus = 0;                  // 运行状态：1-正常运行；2-异常运行

    std::vector<StackLabelInfo> m_stackLabels;    // 业务链路标签信息
    std::map<std::string, Service> m_services;    // 组件列表，K: serviceUUID, V: Service
};

}