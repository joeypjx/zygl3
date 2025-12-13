#pragma once

#include "chassis.h"
#include "board.h"
#include "value_objects.h"
#include <memory>
#include <vector>
#include <map>

namespace app::domain {

/**
 * @brief 机箱仓储接口
 */
class IChassisRepository {
public:
    virtual ~IChassisRepository() = default;

    /**
     * @brief 保存或更新机箱
     */
    virtual void Save(std::shared_ptr<Chassis> chassis) = 0;

    /**
     * @brief 根据机箱号查找机箱
     */
    virtual std::shared_ptr<Chassis> FindByNumber(int chassisNumber) = 0;

    /**
     * @brief 获取所有机箱
     */
    virtual std::vector<std::shared_ptr<Chassis>> GetAll() = 0;

    /**
     * @brief 根据板卡IP地址查找所属机箱
     */
    virtual std::shared_ptr<Chassis> FindByBoardAddress(const std::string& boardAddress) = 0;

    /**
     * @brief 清空所有机箱数据
     */
    virtual void Clear() = 0;

    /**
     * @brief 更新指定机箱的指定板卡
     * @param chassisNumber 机箱号
     * @param slotNumber 板卡槽位号 (1-14)
     * @param board 板卡对象
     * @return 是否更新成功
     */
    virtual bool UpdateBoard(int chassisNumber, int slotNumber, const Board& board) = 0;

    /**
     * @brief 通过机箱号和槽位号获取板卡对象
     * @param chassisNumber 机箱号
     * @param slotNumber 板卡槽位号 (1-14)
     * @return 板卡指针，如果机箱不存在或槽位号无效返回nullptr
     */
    virtual Board* GetBoardBySlot(int chassisNumber, int slotNumber) = 0;

    /**
     * @brief 通过机箱号和槽位号获取板卡对象（const版本）
     * @param chassisNumber 机箱号
     * @param slotNumber 板卡槽位号 (1-14)
     * @return 板卡指针，如果机箱不存在或槽位号无效返回nullptr
     */
    virtual const Board* GetBoardBySlot(int chassisNumber, int slotNumber) const = 0;

    /**
     * @brief 批量更新指定机箱内所有板卡的状态（基于板卡在位信息）
     * @param chassisNumber 机箱号
     * @param presenceMap 槽位号到在位状态的映射 (槽位号 1-14 -> true表示在位, false表示不在位)
     * @return 成功更新的板卡数量
     * @note 更新逻辑：
     *       - 如果板卡不在位(false)：无论当前状态如何，都更新为Offline
     *       - 如果板卡在位(true)：若当前状态是Offline则更新为Abnormal，否则不更新
     *       - 如果某个槽位号不在映射中，该板卡不会被更新
     */
    virtual size_t UpdateAllBoardsStatus(int chassisNumber, 
                                        const std::map<int, bool>& presenceMap) = 0;
};

}
