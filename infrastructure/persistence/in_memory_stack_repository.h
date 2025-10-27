#pragma once

#include "domain/i_stack_repository.h"
#include "domain/stack.h"
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <optional>

namespace app::infrastructure {

/**
 * @brief 业务链路内存仓储实现
 * @detail 使用内存存储业务链路数据，支持多线程安全访问
 */
class InMemoryStackRepository : public app::domain::IStackRepository {
public:
    InMemoryStackRepository() = default;
    ~InMemoryStackRepository() = default;

    // 禁止拷贝
    InMemoryStackRepository(const InMemoryStackRepository&) = delete;
    InMemoryStackRepository& operator=(const InMemoryStackRepository&) = delete;

    /**
     * @brief 保存或更新业务链路
     */
    void Save(std::shared_ptr<app::domain::Stack> stack) override {
        if (!stack) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string stackUUID = stack->GetStackUUID();
        m_stackMap[stackUUID] = stack;
        UpdateLabelIndex(stack);
    }

    /**
     * @brief 根据UUID查找业务链路
     */
    std::shared_ptr<app::domain::Stack> FindByUUID(const std::string& stackUUID) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_stackMap.find(stackUUID);
        if (it != m_stackMap.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief 根据标签查找所有业务链路
     */
    std::vector<std::shared_ptr<app::domain::Stack>> FindByLabel(const std::string& label) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<std::shared_ptr<app::domain::Stack>> result;
        
        auto it = m_labelIndex.find(label);
        if (it != m_labelIndex.end()) {
            const auto& stackUUIDs = it->second;
            result.reserve(stackUUIDs.size());
            
            for (const auto& uuid : stackUUIDs) {
                auto stackIt = m_stackMap.find(uuid);
                if (stackIt != m_stackMap.end()) {
                    result.push_back(stackIt->second);
                }
            }
        }
        
        return result;
    }

    /**
     * @brief 获取所有业务链路
     */
    std::vector<std::shared_ptr<app::domain::Stack>> GetAll() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::shared_ptr<app::domain::Stack>> result;
        result.reserve(m_stackMap.size());
        
        for (const auto& pair : m_stackMap) {
            result.push_back(pair.second);
        }
        
        return result;
    }

    /**
     * @brief 通过taskID查找任务的资源使用情况
     */
    std::optional<app::domain::ResourceUsage> GetTaskResources(const std::string& taskID) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (const auto& pair : m_stackMap) {
            const auto& stack = pair.second;
            auto resources = stack->GetTaskResources(taskID);
            if (resources) {
                return resources;
            }
        }
        
        return std::nullopt;
    }

    /**
     * @brief 清空所有业务链路数据
     */
    void Clear() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stackMap.clear();
        m_labelIndex.clear();
    }

    /**
     * @brief 获取业务链路数量
     */
    size_t Size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stackMap.size();
    }

private:
    /**
     * @brief 更新标签索引
     * @detail 用于快速根据标签查找业务链路
     */
    void UpdateLabelIndex(std::shared_ptr<app::domain::Stack> stack) {
        if (!stack) {
            return;
        }
        
        // 先移除旧的索引
        for (auto& pair : m_labelIndex) {
            auto& stackUUIDs = pair.second;
            stackUUIDs.erase(stack->GetStackUUID());
        }
        
        // 添加新的索引
        const auto& labels = stack->GetLabels();
        for (const auto& labelInfo : labels) {
            m_labelIndex[labelInfo.stackLabelUUID].insert(stack->GetStackUUID());
        }
    }

private:
    // K: stackUUID, V: shared_ptr<Stack>
    std::map<std::string, std::shared_ptr<app::domain::Stack>> m_stackMap;
    
    // K: labelUUID, V: set<stackUUID>
    std::map<std::string, std::set<std::string>> m_labelIndex;
    
    // 互斥锁，保证线程安全
    mutable std::mutex m_mutex;
};

} // namespace app::infrastructure
