#pragma once

#include "board.h"
#include <string>
#include <vector>

namespace app::domain {

/**
 * @brief 机箱聚合根
 * @detail 一个机箱包含14块板卡
 */
class Chassis {
public:
    Chassis() = default;

    Chassis(int chassisNumber, const std::string& chassisName)
        : m_chassisNumber(chassisNumber), m_chassisName(chassisName) {
    }

    // Getters
    int GetChassisNumber() const { return m_chassisNumber; }
    const std::string& GetChassisName() const { return m_chassisName; }

    /**
     * @brief 获取所有板卡
     */
    const std::vector<Board>& GetAllBoards() const {
        return m_boards;
    }

    /**
     * @brief 获取板卡的引用版本（用于修改）
     */
    std::vector<Board>& GetAllBoardsMutable() {
        return m_boards;
    }

    /**
     * @brief 通过槽位号获取板卡引用
     * @param slotNumber 槽位号 (1-14)
     * @return 板卡指针，如果槽位号无效返回nullptr
     */
    Board* GetBoardBySlot(int slotNumber) {
        if (slotNumber >= 1 && slotNumber <= static_cast<int>(m_boards.size())) {
            return &m_boards[slotNumber - 1];
        }
        return nullptr;
    }

    /**
     * @brief 通过槽位号获取板卡引用（const版本）
     */
    const Board* GetBoardBySlot(int slotNumber) const {
        if (slotNumber >= 1 && slotNumber <= static_cast<int>(m_boards.size())) {
            return &m_boards[slotNumber - 1];
        }
        return nullptr;
    }

    /**
     * @brief 通过IP地址查找板卡
     * @param boardAddress 板卡IP地址
     * @return 板卡指针，如果未找到返回nullptr
     */
    Board* GetBoardByAddress(const std::string& boardAddress) {
        for (auto& board : m_boards) {
            if (board.GetAddress() == boardAddress) {
                return &board;
            }
        }
        return nullptr;
    }

    /**
     * @brief 通过IP地址查找板卡（const版本）
     */
    const Board* GetBoardByAddress(const std::string& boardAddress) const {
        for (const auto& board : m_boards) {
            if (board.GetAddress() == boardAddress) {
                return &board;
            }
        }
        return nullptr;
    }

    /**
     * @brief 在启动时，添加板卡
     */
    void AddBoard(const Board& board) {
        m_boards.push_back(board);
    }

    /**
     * @brief 设置板卡列表的大小（用于预分配14个板卡槽位）
     */
    void ResizeBoards(size_t count) {
        m_boards.resize(count);
    }

    /**
     * @brief 获取板卡数量
     */
    size_t GetBoardCount() const {
        return m_boards.size();
    }

    /**
     * @brief 根据槽位号更新板卡（用于增量更新）
     * @param slotNumber 槽位号 (1-14)
     * @param board 板卡对象
     * @return 是否更新成功
     */
    bool UpdateBoardBySlot(int slotNumber, const Board& board) {
        if (slotNumber >= 1 && slotNumber <= static_cast<int>(m_boards.size())) {
            m_boards[slotNumber - 1] = board;
            return true;
        }
        return false;
    }

private:
    int m_chassisNumber = 0;          // 机箱号
    std::string m_chassisName;        // 机箱名称

    // 板卡列表
    std::vector<Board> m_boards;
};

}