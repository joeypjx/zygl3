#pragma once

#include <string>
#include <mutex>
#include <memory>
#include "json.hpp"

namespace app::infrastructure {

class ConfigManager {
public:
    static void LoadFromFile(const std::string& path);
    static const nlohmann::json& Get();
    static const nlohmann::json* TryGet(const std::string& pointer);
    static std::string GetString(const std::string& pointer, const std::string& def = "");
    static int GetInt(const std::string& pointer, int def = 0);

private:
    static nlohmann::json s_config;
    static std::once_flag s_loaded;
};

}


