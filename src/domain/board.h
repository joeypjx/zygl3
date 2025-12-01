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
    const std::string& GetBoardName() const { return m_boardName; }
    int GetBoardNumber() const { return m_boardNumber; }
    BoardType GetBoardType() const { return m_boardType; }
    BoardOperationalStatus GetStatus() const { return m_status; }
    float GetVoltage() const { return m_voltage; }
    float GetCurrent() const { return m_current; }
    float GetTemperature() const { return m_temperature; }
    const std::vector<FanSpeed>& GetFanSpeeds() const { return m_fanSpeeds; }
    const std::vector<TaskStatusInfo>& GetTasks() const { return m_tasks; }
    std::chrono::system_clock::time_point GetLastUpdateTime() const { return m_lastUpdateTime; }

    /**
     * @brief 更新板卡状态
     * @param status 板卡运行状态
     */
    void UpdateStatus(BoardOperationalStatus status) {
        m_status = status;
        m_lastUpdateTime = std::chrono::system_clock::now();
    }

    /**
     * @brief 更新板卡状态（从API状态值）
     * @param statusFromApi 板卡状态 (0-正常, 1-异常)
     */
    void UpdateStatus(int statusFromApi) {
        m_status = (statusFromApi == 0) 
                   ? BoardOperationalStatus::Normal 
                   : BoardOperationalStatus::Abnormal;
        m_lastUpdateTime = std::chrono::system_clock::now();
    }

    /**
     * @brief 用来自API的实时数据更新此板卡的状态
     * @param boardName 板卡名称
     * @param boardAddress 板卡IP地址
     * @param boardType 板卡类型
     * @param statusFromApi 板卡状态 (0-正常, 1-异常)
     * @param voltage 电压
     * @param current 电流
     * @param temperature 温度
     * @param fanSpeeds 风扇信息列表
     * @param tasksFromApi 板卡上的任务列表
     * @note m_boardNumber 不会被更新，因为它是板卡的唯一标识
     */
    void UpdateFromApiData(const std::string& boardName,
                          const std::string& boardAddress,
                          BoardType boardType,
                          int statusFromApi,
                          float voltage,
                          float current,
                          float temperature,
                          const std::vector<FanSpeed>& fanSpeeds,
                          const std::vector<TaskStatusInfo>& tasksFromApi) {
        // 更新板卡基本信息（除了 m_boardNumber）
        m_boardName = boardName;
        m_boardAddress = boardAddress;
        m_boardType = boardType;
        
        // 根据API的返回值更新状态
        m_status = (statusFromApi == 0) 
                   ? BoardOperationalStatus::Normal 
                   : BoardOperationalStatus::Abnormal;

        // 更新监控数据
        m_voltage = voltage;
        m_current = current;
        m_temperature = temperature;
        m_fanSpeeds = fanSpeeds;

        // 更新任务列表（不再根据板卡类型过滤）
        m_tasks = tasksFromApi;
        
        // 更新时间戳
        m_lastUpdateTime = std::chrono::system_clock::now();
    }

    /**
     * @brief 检查板卡是否在线，如果不在线且当前状态不是离线，则标记为离线
     * @param timeoutSeconds 超时秒数（默认60秒）
     * @return 如果进行了离线标记返回true，否则返回false
     */
    bool CheckAndMarkOfflineIfNeeded(int timeoutSeconds = 60) {
        // 检查板卡是否在线（基于最后更新时间）
        bool isOnline = false;
        if (m_lastUpdateTime != std::chrono::system_clock::time_point::min()) {
            auto now = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastUpdateTime).count();
            isOnline = elapsed < timeoutSeconds;
        }
        
        // 如果不在线且当前状态不是离线，则标记为离线
        if (!isOnline) {
            if (m_status != BoardOperationalStatus::Offline) {
                m_status = BoardOperationalStatus::Offline;
                m_tasks.clear();
                // 保留更新时间，不清空，以便保留最后一次更新的时间信息
                return true;  // 进行了离线标记
            }
        }
        return false;  // 没有进行离线标记
    }

private:
    std::string m_boardAddress;              // 板卡IP地址
    std::string m_boardName;                 // 板卡名称
    int m_boardNumber = 0;                   // 板卡槽位号
    BoardType m_boardType = BoardType::Computing; // 板卡类型
    BoardOperationalStatus m_status = BoardOperationalStatus::Unknown; // 板卡运行状态
    
    // 板卡监控数据
    float m_voltage = 0.0f;                 // 电压
    float m_current = 0.0f;                 // 电流
    float m_temperature = 0.0f;             // 温度
    std::vector<FanSpeed> m_fanSpeeds;      // 风扇信息列表

    // 任务列表
    std::vector<TaskStatusInfo> m_tasks;
    
    // 最后更新时间（用于判断板卡是否在线）
    std::chrono::system_clock::time_point m_lastUpdateTime = std::chrono::system_clock::time_point::min();
};

}