#include "config_manager.h"
#include <fstream>
#include <iostream>

using nlohmann::json;

namespace app::infrastructure {

json ConfigManager::s_config = json::object();
std::once_flag ConfigManager::s_loaded;

void ConfigManager::LoadFromFile(const std::string& path) {
    std::call_once(s_loaded, [&]() {
        std::ifstream in(path);
        if (!in) {
            std::cerr << "配置文件不存在或无法读取: " << path << std::endl;
            return;
        }
        try {
            in >> s_config;
        } catch (const std::exception& e) {
            std::cerr << "解析配置文件失败: " << e.what() << std::endl;
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

}


