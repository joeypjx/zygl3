#include "logger_config.h"
#include "config_manager.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <vector>
#include <memory>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdlib>

namespace app::infrastructure {

void LoggerConfig::Initialize(
    const std::string& logDir,
    spdlog::level::level_enum logLevel,
    bool enableConsole,
    bool enableFile,
    size_t maxFileSize,
    size_t maxFiles
) {
    std::vector<spdlog::sink_ptr> sinks;

    // 控制台输出 sink（带颜色）
    if (enableConsole) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(logLevel);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        sinks.push_back(console_sink);
    }

    // 文件输出 sink（轮转文件）
    if (enableFile) {
        try {
            // 确保日志目录存在（使用系统调用）
            struct stat info;
            if (stat(logDir.c_str(), &info) != 0) {
                // 目录不存在，尝试创建（需要递归创建父目录）
                std::string cmd = "mkdir -p " + logDir;
                system(cmd.c_str());
            }
            
            std::string logFile = logDir + "/zygl.log";
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                logFile, maxFileSize, maxFiles
            );
            file_sink->set_level(logLevel);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
            sinks.push_back(file_sink);
        } catch (const std::exception& ex) {
            // 如果文件写入失败，至少确保控制台输出可用
            if (enableConsole) {
                spdlog::warn("无法创建日志文件: {}，仅使用控制台输出", ex.what());
            }
        }
    }

    // 创建 logger 并设置为默认 logger
    if (!sinks.empty()) {
        auto logger = std::make_shared<spdlog::logger>("zygl", sinks.begin(), sinks.end());
        logger->set_level(logLevel);
        logger->flush_on(spdlog::level::warn);  // warn 及以上级别立即刷新
        spdlog::set_default_logger(logger);
        spdlog::set_level(logLevel);
    }
}

void LoggerConfig::InitializeFromConfig(const std::string& configPath) {
    // 从配置文件读取日志设置
    std::string logDir = ConfigManager::GetString("/logging/log_dir", "/var/log/zygl");
    std::string logLevelStr = ConfigManager::GetString("/logging/level", "info");
    bool enableConsole = ConfigManager::GetInt("/logging/enable_console", 1) != 0;
    bool enableFile = ConfigManager::GetInt("/logging/enable_file", 1) != 0;
    size_t maxFileSize = static_cast<size_t>(ConfigManager::GetInt("/logging/max_file_size_mb", 10)) * 1024 * 1024;
    size_t maxFiles = static_cast<size_t>(ConfigManager::GetInt("/logging/max_files", 5));

    // 转换日志级别字符串
    spdlog::level::level_enum logLevel = spdlog::level::info;
    if (logLevelStr == "trace") {
        logLevel = spdlog::level::trace;
    } else if (logLevelStr == "debug") {
        logLevel = spdlog::level::debug;
    } else if (logLevelStr == "info") {
        logLevel = spdlog::level::info;
    } else if (logLevelStr == "warn") {
        logLevel = spdlog::level::warn;
    } else if (logLevelStr == "error") {
        logLevel = spdlog::level::err;
    } else if (logLevelStr == "critical") {
        logLevel = spdlog::level::critical;
    }

    Initialize(logDir, logLevel, enableConsole, enableFile, maxFileSize, maxFiles);
}

void LoggerConfig::Shutdown() {
    spdlog::shutdown();
}

}

