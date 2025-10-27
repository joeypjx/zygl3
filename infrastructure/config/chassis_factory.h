#pragma once

#include "domain/chassis.h"
#include "domain/board.h"
#include "domain/value_objects.h"
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
    std::shared_ptr<app::domain::Chassis> CreateChassis(const ChassisConfig& config) {
        auto chassis = std::make_shared<app::domain::Chassis>(config.chassisNumber, config.chassisName);
        
        // 为机箱预分配14个空板卡槽位
        chassis->ResizeBoards(14);
        
        // 初始化每个板卡槽位
        for (const auto& boardConfig : config.boards) {
            auto board = app::domain::Board(
                boardConfig.boardAddress,
                boardConfig.boardNumber,
                boardConfig.boardType
            );
            
            // 先获取槽位对应的板卡
            auto* targetBoard = chassis->GetBoardBySlot(boardConfig.boardNumber);
            if (targetBoard != nullptr) {
                // 用配置的板卡更新槽位
                *targetBoard = board;
            }
        }
        
        return chassis;
    }

    /**
     * @brief 创建完整的系统拓扑（9个机箱）
     * @param configs 9个机箱的配置信息
     * @return 创建的机箱列表
     */
    std::vector<std::shared_ptr<app::domain::Chassis>> CreateFullTopology(
        const std::vector<ChassisConfig>& configs) {
        
        std::vector<std::shared_ptr<app::domain::Chassis>> topology;
        topology.reserve(9);
        
        for (const auto& config : configs) {
            topology.push_back(CreateChassis(config));
        }
        
        return topology;
    }

    /**
     * @brief 根据槽位号确定板卡类型
     * @param slotNumber 槽位号 (1-14)
     * @return 板卡类型
     */
    static app::domain::BoardType DetermineBoardType(int slotNumber) {
        if (slotNumber == 6 || slotNumber == 7) {
            return app::domain::BoardType::Switch;   // 交换板卡
        } else if (slotNumber == 13 || slotNumber == 14) {
            return app::domain::BoardType::Power;    // 电源板卡
        } else {
            return app::domain::BoardType::Computing; // 计算板卡
        }
    }

    /**
     * @brief 创建默认配置（用于测试或初始化）
     * @detail 创建9个机箱，每个机箱14块板卡的默认配置
     * @return 9个机箱的配置列表
     */
    static std::vector<ChassisConfig> CreateDefaultConfigs() {
        std::vector<ChassisConfig> configs;
        configs.reserve(9);
        
        // 创建9个机箱的配置
        for (int chassisNum = 1; chassisNum <= 9; ++chassisNum) {
            ChassisConfig chassisConfig;
            chassisConfig.chassisNumber = chassisNum;
            chassisConfig.chassisName = "Chassis_" + std::to_string(chassisNum);
            
            // 为每个机箱创建14块板卡的配置
            for (int slotNum = 1; slotNum <= 14; ++slotNum) {
                BoardConfig boardConfig;
                boardConfig.boardNumber = slotNum;
                
                // 生成默认IP地址（可以根据实际需求修改）
                // 格式：192.168.0.${chassisNumber}${slotNumber}
                boardConfig.boardAddress = 
                    "192.168.0." + std::to_string(chassisNum * 100 + slotNum);
                
                // 根据槽位号确定板卡类型
                boardConfig.boardType = DetermineBoardType(slotNum);
                
                chassisConfig.boards.push_back(boardConfig);
            }
            
            configs.push_back(chassisConfig);
        }
        
        return configs;
    }
};

}
