#pragma once

#include <string>
#include <mutex>
#include <memory>
#include <cstdint>
#include "nlohmann-json/json.hpp"

namespace app::infrastructure {

class ConfigManager {
public:
    static void LoadFromFile(const std::string& path);
    static const nlohmann::json& Get();
    static const nlohmann::json* TryGet(const std::string& pointer);
    static std::string GetString(const std::string& pointer, const std::string& def = "");
    static int GetInt(const std::string& pointer, int def = 0);
    
    // 读取十六进制字符串并转换为整数（用于UDP命令码）
    static uint16_t GetHexUint16(const std::string& pointer, uint16_t def = 0);
    
    // 读取十六进制字符串并转换为uint32
    static uint32_t GetHexUint32(const std::string& pointer, uint32_t def = 0);

private:
    static nlohmann::json s_config;
    static std::once_flag s_loaded;
};

}


