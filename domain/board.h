#pragma once

#include "value_objects.h"
#include <string>
#include <vector>
#include <chrono>

namespace app::domain {

/**
 * @brief 板卡实体
 * @detail 一个板卡包含配置信息（槽位、IP、类型）和动态状态（运行状态、任务列表）
 */
class Board {
public:
    Board() = default;

    Board(const std::string& boardAddress, int boardNumber, BoardType type)
        : m_boardAddress(boardAddress),
          m_boardNumber(boardNumber),
          m_boardType(type),
          m_status(BoardOperationalStatus::Unknown) {
    }

    // Getters
    const std::string& GetAddress() const { return m_boardAddress; }
    int GetBoardNumber() const { return m_boardNumber; }
    BoardType GetBoardType() const { return m_boardType; }
    BoardOperationalStatus GetStatus() const { return m_status; }

    // 获取任务列表
    const std::vector<TaskStatusInfo>& GetTasks() const { return m_tasks; }

    /**
     * @brief 检查此板卡类型是否允许运行任务
     */
    bool CanRunTasks() const {
        return m_boardType == BoardType::Computing;
    }

    /**
     * @brief 用来自API的实时数据更新此板卡的状态
     * @param statusFromApi 板卡状态 (0-正常, 1-异常)
     * @param tasksFromApi 板卡上的任务列表
     */
    void UpdateFromApiData(int statusFromApi, const std::vector<TaskStatusInfo>& tasksFromApi) {
        // 根据API的返回值更新状态
        m_status = (statusFromApi == 0) 
                   ? BoardOperationalStatus::Normal 
                   : BoardOperationalStatus::Abnormal;

        // 核心规则应用：只有计算板卡才能接受任务
        if (CanRunTasks()) {
            m_tasks = tasksFromApi;
        } else {
            m_tasks.clear(); // 强制清空
        }
        
        // 更新时间戳
        m_lastUpdateTime = std::chrono::system_clock::now();
    }

    /**
     * @brief 将此板卡标记为"离线"
     */
    void MarkAsOffline() {
        m_status = BoardOperationalStatus::Offline;
        m_tasks.clear();
        // 清空更新时间，表示离线
        m_lastUpdateTime = std::chrono::system_clock::time_point::min();
    }
    
    /**
     * @brief 获取最后更新时间
     */
    std::chrono::system_clock::time_point GetLastUpdateTime() const {
        return m_lastUpdateTime;
    }
    
    /**
     * @brief 判断板卡是否在线（基于最后更新时间）
     * @param timeoutSeconds 超时秒数（默认60秒）
     * @return 是否在线
     */
    bool IsOnline(int timeoutSeconds = 60) const {
        // 如果从未更新过（离线状态），返回false
        if (m_lastUpdateTime == std::chrono::system_clock::time_point::min()) {
            return false;
        }
        
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastUpdateTime).count();
        return elapsed < timeoutSeconds;
    }

private:
    std::string m_boardAddress;              // 板卡IP地址
    int m_boardNumber = 0;                   // 板卡槽位号
    BoardType m_boardType = BoardType::Computing; // 板卡类型
    BoardOperationalStatus m_status = BoardOperationalStatus::Unknown; // 板卡运行状态

    // 任务列表
    std::vector<TaskStatusInfo> m_tasks;
    
    // 最后更新时间（用于判断板卡是否在线）
    std::chrono::system_clock::time_point m_lastUpdateTime = std::chrono::system_clock::time_point::min();
};

}