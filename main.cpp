#include "src/infrastructure/config/chassis_factory.h"
#include "src/infrastructure/persistence/in_memory_chassis_repository.h"
#include "src/infrastructure/persistence/in_memory_stack_repository.h"
#include "src/infrastructure/api_client/qyw_api_client.h"
#include "src/infrastructure/collectors/data_collector_service.h"
#include "src/infrastructure/ha/heartbeat_service.h"
#include "src/interfaces/udp/resource_monitor_broadcaster.h"
#include "src/interfaces/http/alert_receiver_server.h"
#include "src/interfaces/cli/cli_service.h"
#include "src/interfaces/bmc/bmc_receiver.h"
#include "src/infrastructure/config/config_manager.h"
#include "src/infrastructure/config/logger_config.h"
#include <iostream>
#include <memory>
#include <thread>
#include <string>
#include <cstdlib>

using namespace app::infrastructure;
using namespace app::domain;
using namespace app::interfaces;

void PrintUsage(const char* programName) {
    std::cout << "用法: " << programName << " [选项]\n"
              << "选项:\n"
              << "  -c, --config <文件>    指定配置文件路径 (默认: config.json)\n"
              << "  -h, --help             显示此帮助信息\n"
              << "\n"
              << "也可以通过环境变量 ZYGL_CONFIG 指定配置文件路径\n"
              << std::endl;
}

std::string GetConfigPath(int argc, char* argv[]) {
    // 1. 检查命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            exit(0);
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                return argv[i + 1];
            } else {
                std::cerr << "错误: -c/--config 选项需要指定配置文件路径" << std::endl;
                PrintUsage(argv[0]);
                exit(1);
            }
        }
    }
    
    // 2. 检查环境变量
    const char* envConfig = std::getenv("ZYGL_CONFIG");
    if (envConfig != nullptr) {
        return std::string(envConfig);
    }
    
    // 3. 使用默认值
    return "config.json";
}

int main(int argc, char* argv[]) {
    // 0. 获取配置文件路径
    std::string configPath = GetConfigPath(argc, argv);
    
    // 0. 加载配置
    ConfigManager::LoadFromFile(configPath);
    
    // 0.1. 初始化日志系统（同时输出到终端和文件）
    LoggerConfig::InitializeFromConfig();
    
    spdlog::info("=== 系统启动：初始化机箱数据 ===");

    // 1. 创建仓储
    auto chassisRepo = std::make_shared<InMemoryChassisRepository>();
    auto stackRepo = std::make_shared<InMemoryStackRepository>();
    
    // 2. 创建工厂类并初始化系统拓扑
    ChassisFactory factory;
    // 优先从 chassis_config.json 加载配置，如果不存在则从 config.json 或使用默认配置
    auto configs = ChassisFactory::CreateDefaultConfigs("chassis_config.json");
    auto topology = factory.CreateFullTopology(configs);
    
    // 3. 保存所有机箱到仓储
    for (const auto& chassis : topology) {
        chassisRepo->Save(chassis);
    }
    
    spdlog::info("初始化完成！仓储中共有 {} 个机箱", chassisRepo->Size());
    
    // 4. 创建 API 客户端（读取配置）
    std::string apiBaseUrl = ConfigManager::GetString("/api/base_url", "localhost");
    int apiPort = ConfigManager::GetInt("/api/port", 8080);
    auto apiClient = std::make_shared<QywApiClient>(apiBaseUrl, apiPort);
    
    // 设置API端点路径
    apiClient->SetEndpoint("boardinfo", ConfigManager::GetString("/api/endpoints/boardinfo", "/api/v1/external/qyw/boardinfo"));
    apiClient->SetEndpoint("stackinfo", ConfigManager::GetString("/api/endpoints/stackinfo", "/api/v1/external/qyw/stackinfo"));
    apiClient->SetEndpoint("deploy", ConfigManager::GetString("/api/endpoints/deploy", "/api/v1/stacks/labels/deploy"));
    apiClient->SetEndpoint("undeploy", ConfigManager::GetString("/api/endpoints/undeploy", "/api/v1/stacks/labels/undeploy"));
    apiClient->SetEndpoint("heartbeat", ConfigManager::GetString("/api/endpoints/heartbeat", "/api/v1/external/qyw/config"));
    apiClient->SetEndpoint("reset", ConfigManager::GetString("/api/endpoints/reset", "/api/v1/stacks/labels/reset"));

    // 5. 创建CLI服务   
    spdlog::info("启动CLI服务...");
    auto cliService = std::make_shared<CliService>(chassisRepo, stackRepo, apiClient);
    cliService->Start();
    
    // 6. 创建UDP组播服务（读取配置）
    spdlog::info("创建UDP组播服务...");
    std::string udpBroadcasterGroup = ConfigManager::GetString("/udp/broadcaster/multicast_group", "234.186.1.99");
    uint16_t udpPort = static_cast<uint16_t>(ConfigManager::GetInt("/udp/port", 0x100A));
    auto broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
        chassisRepo, stackRepo, apiClient, udpBroadcasterGroup, udpPort);
    
    // 设置UDP广播器命令码
    broadcaster->SetCommand(
        ConfigManager::GetHexUint16("/udp/commands/resource_monitor_resp", 0xF100),
        ConfigManager::GetHexUint16("/udp/commands/task_query_resp", 0xF105),
        ConfigManager::GetHexUint16("/udp/commands/task_start_resp", 0xF103),
        ConfigManager::GetHexUint16("/udp/commands/task_stop_resp", 0xF104),
        ConfigManager::GetHexUint16("/udp/commands/chassis_reset_resp", 0xF101),
        ConfigManager::GetHexUint16("/udp/commands/chassis_self_check_resp", 0xF102),
        ConfigManager::GetHexUint16("/udp/commands/fault_report", 0xF107),
        ConfigManager::GetHexUint16("/udp/commands/bmc_query_resp", 0xF106)
    );
    broadcaster->Start();

    // 创建心跳服务（读取配置，仅在ha/enabled为true时创建）
    std::shared_ptr<HeartbeatService> heartbeatService = nullptr;
    bool haEnabled = ConfigManager::GetBool("/ha/enabled", false);
    if (haEnabled) {
        spdlog::info("创建心跳服务...");
        std::string haMulticastGroup = ConfigManager::GetString("/ha/multicast_group", "224.100.200.16");
        uint16_t haHeartbeatPort = static_cast<uint16_t>(ConfigManager::GetInt("/ha/heartbeat/port", 9999));
        int haPriority = ConfigManager::GetInt("/ha/priority", 0);
        int haHeartbeatInterval = ConfigManager::GetInt("/ha/heartbeat/interval_seconds", 3);
        int haTimeoutThreshold = ConfigManager::GetInt("/ha/heartbeat/timeout_seconds", 9);
        heartbeatService = std::make_shared<HeartbeatService>(
            haMulticastGroup, haHeartbeatPort, haPriority, haHeartbeatInterval, haTimeoutThreshold);
        heartbeatService->Start(HeartbeatService::Role::Unknown);  // 从Unknown开始，自动协商
    } else {
        spdlog::info("HA功能已禁用（ha/enabled=false），跳过心跳服务创建");
    }
    
    auto listener = std::make_shared<ResourceMonitorListener>(
        broadcaster, heartbeatService,
        ConfigManager::GetString("/udp/listener/multicast_group", "234.186.1.98"),
        udpPort);
    
    // 设置UDP监听器命令码
    listener->SetCommand(
        ConfigManager::GetHexUint16("/udp/commands/resource_monitor", 0xF000),
        ConfigManager::GetHexUint16("/udp/commands/task_query", 0xF005),
        ConfigManager::GetHexUint16("/udp/commands/task_start", 0xF003),
        ConfigManager::GetHexUint16("/udp/commands/task_stop", 0xF004),
        ConfigManager::GetHexUint16("/udp/commands/chassis_reset", 0xF001),
        ConfigManager::GetHexUint16("/udp/commands/chassis_self_check", 0xF002),
        ConfigManager::GetHexUint16("/udp/commands/bmc_query", 0xF006)
    );
    listener->Start();
    
    // 8. 创建BMC接收器（读取配置）
    spdlog::info("创建BMC接收器...");
    std::string bmcMulticastGroup = ConfigManager::GetString("/bmc/multicast_group", "224.100.200.15");
    uint16_t bmcPort = static_cast<uint16_t>(ConfigManager::GetInt("/bmc/port", 5715));
    auto bmcReceiver = std::make_shared<BmcReceiver>(chassisRepo, bmcMulticastGroup, bmcPort);
    bmcReceiver->Start();
    
    // 9. 创建HTTP告警接收服务器（读取配置）
    spdlog::info("创建HTTP告警接收服务器...");
    int httpAlertPort = ConfigManager::GetInt("/alert_server/port", 8888);
    std::string httpAlertHost = ConfigManager::GetString("/alert_server/host", "0.0.0.0");
    int heartbeatInterval = ConfigManager::GetInt("/collector/interval_seconds", 10);  // 使用采集间隔作为心跳间隔
    auto alertServer = std::make_shared<AlertReceiverServer>(
        chassisRepo, stackRepo, broadcaster, apiClient, heartbeatService,
        httpAlertPort, httpAlertHost, heartbeatInterval);
    alertServer->Start();
    
    // 10. 创建数据采集服务（读取配置）
    int intervalSeconds = ConfigManager::GetInt("/collector/interval_seconds", 10);
    int boardTimeoutSeconds = ConfigManager::GetInt("/collector/board_timeout_seconds", 60);
    spdlog::info("创建数据采集服务（采集间隔：{}秒）...", intervalSeconds);
    DataCollectorService collector(chassisRepo, stackRepo, apiClient, intervalSeconds, boardTimeoutSeconds);
    
    // 11. 启动数据采集（在后台线程运行）
    spdlog::info("启动数据采集服务...");
    collector.Start();
    
    // 12. 主线程等待
    spdlog::info("系统运行中... (按 Ctrl+C 退出)");
    try {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        spdlog::error("错误: {}", e.what());
    }
    
    // 13. 清理
    spdlog::info("正在停止服务...");
    collector.Stop();
    alertServer->Stop();
    bmcReceiver->Stop();
    listener->Stop();
    broadcaster->Stop();
    if (heartbeatService) {
        heartbeatService->Stop();
    }
    cliService->Stop();
    
    spdlog::info("系统运行结束");
    
    // 关闭日志系统
    LoggerConfig::Shutdown();
    
    return 0;
}
