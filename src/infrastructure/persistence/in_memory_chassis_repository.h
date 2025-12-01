#pragma once

#include "src/domain/i_chassis_repository.h"
#include "src/domain/chassis.h"
#include "src/domain/board.h"
#include <spdlog/spdlog.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace app::infrastructure {

/**
 * @brief 机箱内存仓储实现
 * @detail 使用内存存储机箱和板卡数据，支持多线程安全访问
 */
class InMemoryChassisRepository : public app::domain::IChassisRepository {
public:
    InMemoryChassisRepository() = default;
    ~InMemoryChassisRepository() override = default;

    // 禁止拷贝
    InMemoryChassisRepository(const InMemoryChassisRepository&) = delete;
    InMemoryChassisRepository& operator=(const InMemoryChassisRepository&) = delete;

    /**
     * @brief 保存或更新机箱
     */
    void Save(std::shared_ptr<app::domain::Chassis> chassis) override {
        if (!chassis) {
            spdlog::warn("InMemoryChassisRepository::Save: 尝试保存空的机箱对象");
            return;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        int chassisNumber = chassis->GetChassisNumber();
        bool isUpdate = (m_chassisMap.find(chassisNumber) != m_chassisMap.end());
        m_chassisMap[chassisNumber] = chassis;
        
        if (isUpdate) {
            spdlog::info("InMemoryChassisRepository::Save: 更新机箱 {}", chassisNumber);
        } else {
            spdlog::info("InMemoryChassisRepository::Save: 保存新机箱 {}", chassisNumber);
        }
    }

    /**
     * @brief 根据机箱号查找机箱
     */
    std::shared_ptr<app::domain::Chassis> FindByNumber(int chassisNumber) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_chassisMap.find(chassisNumber);
        if (it != m_chassisMap.end()) {
            spdlog::debug("InMemoryChassisRepository::FindByNumber: 找到机箱 {}", chassisNumber);
            return it->second;
        }
        spdlog::debug("InMemoryChassisRepository::FindByNumber: 未找到机箱 {}", chassisNumber);
        return nullptr;
    }

    /**
     * @brief 获取所有机箱
     */
    std::vector<std::shared_ptr<app::domain::Chassis>> GetAll() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::shared_ptr<app::domain::Chassis>> result;
        result.reserve(m_chassisMap.size());
        
        for (const auto& pair : m_chassisMap) {
            result.push_back(pair.second);
        }
        
        spdlog::debug("InMemoryChassisRepository::GetAll: 返回 {} 个机箱", result.size());
        return result;
    }

    /**
     * @brief 根据板卡IP地址查找所属机箱
     */
    std::shared_ptr<app::domain::Chassis> FindByBoardAddress(const std::string& boardAddress) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (const auto& pair : m_chassisMap) {
            const auto& chassis = pair.second;
            auto board = chassis->GetBoardByAddress(boardAddress);
            if (board != nullptr) {
                spdlog::debug("InMemoryChassisRepository::FindByBoardAddress: 找到板卡 {} 所属机箱 {}", 
                              boardAddress, chassis->GetChassisNumber());
                return chassis;
            }
        }
        
        spdlog::debug("InMemoryChassisRepository::FindByBoardAddress: 未找到板卡 {} 所属机箱", boardAddress);
        return nullptr;
    }

    /**
     * @brief 清空所有机箱数据
     */
    void Clear() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t count = m_chassisMap.size();
        m_chassisMap.clear();
        spdlog::info("InMemoryChassisRepository::Clear: 清空 {} 个机箱数据", count);
    }

    /**
     * @brief 获取机箱数量
     */
    size_t Size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_chassisMap.size();
    }

    /**
     * @brief 更新指定机箱的指定板卡
     */
    bool UpdateBoard(int chassisNumber, int slotNumber, const app::domain::Board& board) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_chassisMap.find(chassisNumber);
        if (it != m_chassisMap.end()) {
            auto chassis = it->second;
            bool result = chassis->UpdateBoardBySlot(slotNumber, board);
            if (result) {
                spdlog::info("InMemoryChassisRepository::UpdateBoard: 成功更新机箱 {} 槽位 {} 的板卡", 
                             chassisNumber, slotNumber);
            } else {
                spdlog::warn("InMemoryChassisRepository::UpdateBoard: 更新机箱 {} 槽位 {} 的板卡失败", 
                             chassisNumber, slotNumber);
            }
            return result;
        }
        
        spdlog::warn("InMemoryChassisRepository::UpdateBoard: 未找到机箱 {}", chassisNumber);
        return false;
    }

private:
    // K: chassisNumber, V: shared_ptr<Chassis>
    std::map<int, std::shared_ptr<app::domain::Chassis>> m_chassisMap;
    
    // 互斥锁，保证线程安全
    mutable std::mutex m_mutex;
};

}