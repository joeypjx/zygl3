#pragma once

#include "domain/i_chassis_repository.h"
#include "domain/chassis.h"
#include "domain/board.h"
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
            return;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        int chassisNumber = chassis->GetChassisNumber();
        m_chassisMap[chassisNumber] = chassis;
    }

    /**
     * @brief 根据机箱号查找机箱
     */
    std::shared_ptr<app::domain::Chassis> FindByNumber(int chassisNumber) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_chassisMap.find(chassisNumber);
        if (it != m_chassisMap.end()) {
            return it->second;
        }
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
                return chassis;
            }
        }
        
        return nullptr;
    }

    /**
     * @brief 清空所有机箱数据
     */
    void Clear() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_chassisMap.clear();
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
            return chassis->UpdateBoardBySlot(slotNumber, board);
        }
        
        return false;
    }

private:
    // K: chassisNumber, V: shared_ptr<Chassis>
    std::map<int, std::shared_ptr<app::domain::Chassis>> m_chassisMap;
    
    // 互斥锁，保证线程安全
    mutable std::mutex m_mutex;
};

}