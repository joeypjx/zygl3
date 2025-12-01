#pragma once

#include "stack.h"
#include "value_objects.h"
#include <memory>
#include <vector>
#include <optional>

namespace app::domain {

/**
 * @brief 业务链路仓储接口
 */
class IStackRepository {
public:
    virtual ~IStackRepository() = default;

    /**
     * @brief 保存或更新业务链路
     */
    virtual void Save(std::shared_ptr<Stack> stack) = 0;

    /**
     * @brief 根据UUID查找业务链路
     */
    virtual std::shared_ptr<Stack> FindByUUID(const std::string& stackUUID) = 0;

    /**
     * @brief 获取所有业务链路
     */
    virtual std::vector<std::shared_ptr<Stack>> GetAll() = 0;

    /**
     * @brief 通过taskID查找任务的资源使用情况
     */
    virtual std::optional<ResourceUsage> GetTaskResources(const std::string& taskID) = 0;

    /**
     * @brief 清空所有业务链路数据
     */
    virtual void Clear() = 0;
};

}
