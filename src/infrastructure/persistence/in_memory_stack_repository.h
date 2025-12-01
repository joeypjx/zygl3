#pragma once

#include "src/domain/i_stack_repository.h"
#include "src/domain/stack.h"
#include <spdlog/spdlog.h>
#include <map>
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
            spdlog::warn("InMemoryStackRepository::Save: 尝试保存空的业务链路对象");
            return;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string stackUUID = stack->GetStackUUID();
        bool isUpdate = (m_stackMap.find(stackUUID) != m_stackMap.end());
        m_stackMap[stackUUID] = stack;
        
        if (isUpdate) {
            spdlog::info("InMemoryStackRepository::Save: 更新业务链路 {} ({})", 
                         stackUUID, stack->GetStackName());
        } else {
            spdlog::info("InMemoryStackRepository::Save: 保存新业务链路 {} ({})", 
                         stackUUID, stack->GetStackName());
        }
    }

    /**
     * @brief 根据UUID查找业务链路
     */
    std::shared_ptr<app::domain::Stack> FindByUUID(const std::string& stackUUID) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_stackMap.find(stackUUID);
        if (it != m_stackMap.end()) {
            spdlog::debug("InMemoryStackRepository::FindByUUID: 找到业务链路 {}", stackUUID);
            return it->second;
        }
        spdlog::debug("InMemoryStackRepository::FindByUUID: 未找到业务链路 {}", stackUUID);
        return nullptr;
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
        
        spdlog::debug("InMemoryStackRepository::GetAll: 返回 {} 个业务链路", result.size());
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
                spdlog::debug("InMemoryStackRepository::GetTaskResources: 找到任务 {} 的资源使用情况 (业务链路: {})", 
                              taskID, stack->GetStackName());
                return resources;
            }
        }
        
        spdlog::debug("InMemoryStackRepository::GetTaskResources: 未找到任务 {} 的资源使用情况", taskID);
        return std::nullopt;
    }

    /**
     * @brief 清空所有业务链路数据
     */
    void Clear() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t count = m_stackMap.size();
        m_stackMap.clear();
        spdlog::info("InMemoryStackRepository::Clear: 清空 {} 个业务链路数据", count);
    }

    /**
     * @brief 获取业务链路数量
     */
    size_t Size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stackMap.size();
    }

private:
    // K: stackUUID, V: shared_ptr<Stack>
    std::map<std::string, std::shared_ptr<app::domain::Stack>> m_stackMap;
    
    // 互斥锁，保证线程安全
    mutable std::mutex m_mutex;
};

} // namespace app::infrastructure
