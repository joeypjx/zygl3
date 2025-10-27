#pragma once

#include "chassis.h"
#include "board.h"
#include <memory>
#include <vector>

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
};

}
