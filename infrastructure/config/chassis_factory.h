#pragma once

#include "domain/chassis.h"
#include "domain/board.h"
#include "domain/value_objects.h"
#include "config_manager.h"
#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

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
    static std::vector<ChassisConfig> LoadConfigsFromFile(const std::string& configPath = "") {
        std::vector<ChassisConfig> configs;
        
        try {
            nlohmann::json configJson;
            const nlohmann::json* chassisArray = nullptr;
            
            // 如果提供了文件路径，尝试从单独的文件读取
            if (!configPath.empty()) {
                std::ifstream file(configPath);
                if (file.is_open()) {
                    file >> configJson;
                    file.close();
                    
                    // 支持两种格式：直接是数组，或者包含在topology.chassis中
                    if (configJson.is_array()) {
                        chassisArray = &configJson;
                    } else if (configJson.contains("topology") && 
                               configJson["topology"].contains("chassis") &&
                               configJson["topology"]["chassis"].is_array()) {
                        chassisArray = &configJson["topology"]["chassis"];
                    }
                } else {
                    // 文件不存在，返回空列表
                    return configs;
                }
            } else {
                // 从ConfigManager读取（从config.json中的/topology/chassis）
                chassisArray = ConfigManager::TryGet("/topology/chassis");
            }
            
            // 解析配置
            if (chassisArray != nullptr && chassisArray->is_array()) {
                for (const auto& chassisJson : *chassisArray) {
                    ChassisConfig chassisConfig;
                    
                    // 读取机箱基本信息
                    chassisConfig.chassisNumber = chassisJson.value("chassisNumber", 0);
                    chassisConfig.chassisName = chassisJson.value("chassisName", "");
                    
                    // 读取板卡配置
                    if (chassisJson.contains("boards") && chassisJson["boards"].is_array()) {
                        for (const auto& boardJson : chassisJson["boards"]) {
                            BoardConfig boardConfig;
                            boardConfig.boardNumber = boardJson.value("boardNumber", 0);
                            boardConfig.boardAddress = boardJson.value("boardAddress", "");
                            
                            // 读取板卡类型
                            // 0-计算板卡, 1-交换板卡, 2-电源板卡
                            // 3-通用计算I型模块, 4-通用计算II型模块
                            // 5-高性能计算I型模块, 6-高性能计算II型模块
                            // 7-存储模块, 8-缓存模块, 9-SRIO模块, 10-以太网交换模块
                            int boardTypeInt = boardJson.value("boardType", 0);
                            switch (boardTypeInt) {
                                case 1:
                                    boardConfig.boardType = app::domain::BoardType::Switch;
                                    break;
                                case 2:
                                    boardConfig.boardType = app::domain::BoardType::Power;
                                    break;
                                case 3:
                                    boardConfig.boardType = app::domain::BoardType::GeneralComputingI;
                                    break;
                                case 4:
                                    boardConfig.boardType = app::domain::BoardType::GeneralComputingII;
                                    break;
                                case 5:
                                    boardConfig.boardType = app::domain::BoardType::HighPerformanceComputingI;
                                    break;
                                case 6:
                                    boardConfig.boardType = app::domain::BoardType::HighPerformanceComputingII;
                                    break;
                                case 7:
                                    boardConfig.boardType = app::domain::BoardType::Storage;
                                    break;
                                case 8:
                                    boardConfig.boardType = app::domain::BoardType::Cache;
                                    break;
                                case 9:
                                    boardConfig.boardType = app::domain::BoardType::SRIO;
                                    break;
                                case 10:
                                    boardConfig.boardType = app::domain::BoardType::EthernetSwitch;
                                    break;
                                default:
                                    boardConfig.boardType = app::domain::BoardType::Computing;
                                    break;
                            }
                            
                            chassisConfig.boards.push_back(boardConfig);
                        }
                    }
                    
                    configs.push_back(chassisConfig);
                }
            }
        } catch (const std::exception& e) {
            // 解析失败，返回空列表
            return configs;
        }
        
        return configs;
    }

    /**
     * @brief 创建默认配置（用于测试或初始化）
     * @detail 优先从配置文件读取，如果配置文件不存在或无效，则使用硬编码的默认配置
     * @param configFilePath 可选的单独配置文件路径（如 "chassis_config.json"）
     * @return 机箱配置列表
     */
    static std::vector<ChassisConfig> CreateDefaultConfigs(const std::string& configFilePath = "") {
        // 1. 优先尝试从指定配置文件读取
        if (!configFilePath.empty()) {
            auto configs = LoadConfigsFromFile(configFilePath);
            if (!configs.empty()) {
                std::cout << "[配置加载] 从文件加载机箱配置: " << configFilePath 
                          << " (共 " << configs.size() << " 个机箱)" << std::endl;
                return configs;
            } else {
                std::cout << "[配置加载] 无法从文件加载配置: " << configFilePath 
                          << "，尝试其他配置源..." << std::endl;
            }
        }
        
        // 2. 尝试从ConfigManager读取（从config.json中的/topology/chassis）
        auto configs = LoadConfigsFromFile("");
        if (!configs.empty()) {
            std::cout << "[配置加载] 从config.json加载机箱配置" 
                      << " (共 " << configs.size() << " 个机箱)" << std::endl;
            return configs;
        }
        
        // 3. 如果配置文件不存在或无效，使用硬编码的默认配置
        std::cout << "[配置加载] 使用硬编码默认配置" << std::endl;
        return CreateHardcodedConfigs();
    }

private:
    /**
     * @brief 创建硬编码的默认配置（原来的实现）
     * @return 9个机箱的配置列表
     */
    static std::vector<ChassisConfig> CreateHardcodedConfigs() {
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

public:
};

}
