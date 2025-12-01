#pragma once

#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>

namespace app::infrastructure {

/**
 * @brief 日志配置管理器
 * @detail 配置 spdlog 同时输出到终端和文件
 */
class LoggerConfig {
public:
    /**
     * @brief 初始化日志系统
     * @param logDir 日志文件目录（默认为 /var/log/zygl）
     * @param logLevel 日志级别（默认为 info）
     * @param enableConsole 是否启用控制台输出（默认为 true）
     * @param enableFile 是否启用文件输出（默认为 true）
     * @param maxFileSize 单个日志文件最大大小（字节，默认 10MB）
     * @param maxFiles 保留的日志文件数量（默认 5）
     */
    static void Initialize(
        const std::string& logDir = "/var/log/zygl",
        spdlog::level::level_enum logLevel = spdlog::level::info,
        bool enableConsole = true,
        bool enableFile = true,
        size_t maxFileSize = 10 * 1024 * 1024,  // 10MB
        size_t maxFiles = 5
    );

    /**
     * @brief 从配置文件初始化日志系统
     * @param configPath 配置文件路径（从 ConfigManager 读取配置）
     */
    static void InitializeFromConfig(const std::string& configPath = "");

    /**
     * @brief 关闭日志系统
     */
    static void Shutdown();
};

}

