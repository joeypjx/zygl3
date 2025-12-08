#include "chassis_factory.h"
#include "nlohmann-json/json.hpp"
#include <spdlog/spdlog.h>
#include <fstream>

namespace app::infrastructure {

std::shared_ptr<app::domain::Chassis> ChassisFactory::CreateChassis(const ChassisConfig& config) {
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

std::vector<std::shared_ptr<app::domain::Chassis>> ChassisFactory::CreateFullTopology(
    const std::vector<ChassisConfig>& configs) {
    
    std::vector<std::shared_ptr<app::domain::Chassis>> topology;
    topology.reserve(9);
    
    for (const auto& config : configs) {
        topology.push_back(CreateChassis(config));
    }
    
    return topology;
}

std::vector<ChassisConfig> ChassisFactory::LoadConfigsFromFile(const std::string& configPath) {
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
        spdlog::error("[配置加载] 解析配置文件失败: {}", e.what());
        return configs;
    }
    
    return configs;
}

std::vector<ChassisConfig> ChassisFactory::CreateDefaultConfigs(const std::string& configFilePath) {
    // 1. 优先尝试从指定配置文件读取
    if (!configFilePath.empty()) {
        auto configs = LoadConfigsFromFile(configFilePath);
        if (!configs.empty()) {
            spdlog::info("[配置加载] 从文件加载机箱配置: {} (共 {} 个机箱)", 
                         configFilePath, configs.size());
            return configs;
        } else {
            spdlog::warn("[配置加载] 无法从文件加载配置: {}，尝试其他配置源...", configFilePath);
        }
    }
    
    // 2. 尝试从ConfigManager读取（从config.json中的/topology/chassis）
    auto configs = LoadConfigsFromFile("");
    if (!configs.empty()) {
        spdlog::info("[配置加载] 从config.json加载机箱配置 (共 {} 个机箱)", configs.size());
        return configs;
    }
    
    // 3. 如果配置文件不存在或无效，使用硬编码的默认配置
    spdlog::info("[配置加载] 使用硬编码默认配置");
    return CreateHardcodedConfigs();
}

std::vector<ChassisConfig> ChassisFactory::CreateHardcodedConfigs() {
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
            
            // 生成默认IP地址（根据chassis_config.json中的模式）
            // IP模式：192.168.${chassisNumber}*2[/+1].(${slotNumber}-1[/-8])*32+5
            // 说明：
            //   - 槽位1-5: 第三段 = chassisNumber * 2, 第四段 = (slotNumber - 1) * 32 + 5
            //   - 槽位6: 第三段 = chassisNumber * 2, 第四段 = 170 (固定值)
            //   - 槽位7: 第三段 = chassisNumber * 2, 第四段 = 180 (固定值)
            //   - 槽位8-12: 第三段 = chassisNumber * 2 + 1, 第四段 = (slotNumber - 8) * 32 + 5
            //   - 槽位13: 第三段 = chassisNumber * 2, 第四段 = 182 (固定值)
            //   - 槽位14: 第三段 = chassisNumber * 2, 第四段 = 183 (固定值)
            int thirdOctet;
            int fourthOctet;
            
            if (slotNum <= 7) {
                // 槽位1-7: 使用 chassisNumber * 2 作为第三段
                thirdOctet = chassisNum * 2;
                if (slotNum == 6) {
                    // 槽位6: 第四段固定为170
                    fourthOctet = 170;
                } else if (slotNum == 7) {
                    // 槽位7: 第四段固定为180
                    fourthOctet = 180;
                } else {
                    // 槽位1-5: 使用公式计算
                    fourthOctet = (slotNum - 1) * 32 + 5;
                }
            } else if (slotNum == 13) {
                // 槽位13: 第三段 = chassisNumber * 2, 第四段 = 182
                thirdOctet = chassisNum * 2;
                fourthOctet = 182;
            } else if (slotNum == 14) {
                // 槽位14: 第三段 = chassisNumber * 2, 第四段 = 183
                thirdOctet = chassisNum * 2;
                fourthOctet = 183;
            } else {
                // 槽位8-12: 使用 chassisNumber * 2 + 1 作为第三段
                thirdOctet = chassisNum * 2 + 1;
                fourthOctet = (slotNum - 8) * 32 + 5;
            }
            
            boardConfig.boardAddress = 
                "192.168." + std::to_string(thirdOctet) + "." + std::to_string(fourthOctet);
            
            // 根据槽位号确定板卡类型
            boardConfig.boardType = DetermineBoardType(slotNum);
            
            chassisConfig.boards.push_back(boardConfig);
        }
        
        configs.push_back(chassisConfig);
    }
    
    return configs;
}

}

