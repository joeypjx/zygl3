#pragma once

#include "src/domain/chassis.h"
#include "src/domain/board.h"
#include "src/domain/value_objects.h"
#include "config_manager.h"
#include <memory>
#include <vector>
#include <string>

namespace app::infrastructure {

/**
 * @brief 板卡配置信息
 */
struct BoardConfig {
    int boardNumber;                    // 板卡槽位号
    std::string boardAddress;           // 板卡IP地址
    app::domain::BoardType boardType;   // 板卡类型
};

/**
 * @brief 机箱配置信息
 */
struct ChassisConfig {
    int chassisNumber;                  // 机箱号
    std::string chassisName;            // 机箱名称
    std::vector<BoardConfig> boards;   // 板卡配置列表
};

/**
 * @brief 机箱工厂类
 * @detail 用于创建和初始化系统拓扑（9个机箱，每个机箱14块板卡）
 */
class ChassisFactory {
public:
    ChassisFactory() = default;

    /**
     * @brief 创建单个机箱
     * @param config 机箱配置信息
     * @return 创建的机箱对象
     */
    std::shared_ptr<app::domain::Chassis> CreateChassis(const ChassisConfig& config);

    /**
     * @brief 创建完整的系统拓扑（9个机箱）
     * @param configs 9个机箱的配置信息
     * @return 创建的机箱列表
     */
    std::vector<std::shared_ptr<app::domain::Chassis>> CreateFullTopology(
        const std::vector<ChassisConfig>& configs);

    /**
     * @brief 根据槽位号确定板卡类型（默认规则）
     * @param slotNumber 槽位号 (1-14)
     * @return 板卡类型
     * @note 此方法仅用于没有配置文件时的默认规则。
     *       实际项目中，板卡类型应该从配置文件读取，因为现在有多种板卡类型：
     *       - 通用计算I型模块、通用计算II型模块
     *       - 高性能计算I型模块、高性能计算II型模块
     *       - 存储模块、缓存模块、SRIO模块等
     *       这些类型不能简单根据槽位号判断，必须从配置文件指定。
     */
    static app::domain::BoardType DetermineBoardType(int slotNumber) {
        if (slotNumber == 6 || slotNumber == 7) {
            return app::domain::BoardType::Switch;   // 交换板卡
        } else if (slotNumber == 13 || slotNumber == 14) {
            return app::domain::BoardType::Power;    // 电源板卡
        } else {
            return app::domain::BoardType::Computing; // 计算板卡（默认）
        }
    }

    /**
     * @brief 从配置文件读取机箱配置
     * @param configPath 配置文件路径（可选，如果为空则从ConfigManager读取）
     * @return 机箱配置列表，如果配置文件不存在或无效则返回空列表
     */
    static std::vector<ChassisConfig> LoadConfigsFromFile(const std::string& configPath = "");

    /**
     * @brief 创建默认配置（用于测试或初始化）
     * @detail 优先从配置文件读取，如果配置文件不存在或无效，则使用硬编码的默认配置
     * @param configFilePath 可选的单独配置文件路径（如 "chassis_config.json"）
     * @return 机箱配置列表
     */
    static std::vector<ChassisConfig> CreateDefaultConfigs(const std::string& configFilePath = "");

private:
    /**
     * @brief 创建硬编码的默认配置（原来的实现）
     * @return 9个机箱的配置列表
     */
    static std::vector<ChassisConfig> CreateHardcodedConfigs();

public:
};

}
