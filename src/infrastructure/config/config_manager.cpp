#include "config_manager.h"
#include <spdlog/spdlog.h>
#include <fstream>

using nlohmann::json;

namespace app::infrastructure {

json ConfigManager::s_config = json::object();
std::once_flag ConfigManager::s_loaded;

void ConfigManager::LoadFromFile(const std::string& path) {
    std::call_once(s_loaded, [&]() {
        std::ifstream in(path);
        if (!in) {
            spdlog::error("配置文件不存在或无法读取: {}", path);
            return;
        }
        try {
            in >> s_config;
        } catch (const std::exception& e) {
            spdlog::error("解析配置文件失败: {}", e.what());
        }
    });
}

const json& ConfigManager::Get() {
    return s_config;
}

const json* ConfigManager::TryGet(const std::string& pointer) {
    try {
        const json& j = s_config.at(json::json_pointer(pointer));
        return &j;
    } catch (...) {
        return nullptr;
    }
}

std::string ConfigManager::GetString(const std::string& pointer, const std::string& def) {
    try {
        return s_config.at(json::json_pointer(pointer)).get<std::string>();
    } catch (...) {
        return def;
    }
}

int ConfigManager::GetInt(const std::string& pointer, int def) {
    try {
        return s_config.at(json::json_pointer(pointer)).get<int>();
    } catch (...) {
        return def;
    }
}

uint16_t ConfigManager::GetHexUint16(const std::string& pointer, uint16_t def) {
    try {
        std::string hexStr = s_config.at(json::json_pointer(pointer)).get<std::string>();
        // 移除 "0x" 前缀（如果有）
        if (hexStr.length() > 2 && hexStr.substr(0, 2) == "0x") {
            hexStr = hexStr.substr(2);
        }
        // 转换为整数
        return static_cast<uint16_t>(std::stoul(hexStr, nullptr, 16));
    } catch (...) {
        return def;
    }
}

uint32_t ConfigManager::GetHexUint32(const std::string& pointer, uint32_t def) {
    try {
        std::string hexStr = s_config.at(json::json_pointer(pointer)).get<std::string>();
        // 移除 "0x" 前缀（如果有）
        if (hexStr.length() > 2 && hexStr.substr(0, 2) == "0x") {
            hexStr = hexStr.substr(2);
        }
        // 转换为整数
        return static_cast<uint32_t>(std::stoul(hexStr, nullptr, 16));
    } catch (...) {
        return def;
    }
}

}


