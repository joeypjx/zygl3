#include "infrastructure/config/chassis_factory.h"
#include "infrastructure/persistence/in_memory_chassis_repository.h"
#include "infrastructure/persistence/in_memory_stack_repository.h"
#include "infrastructure/api_client/qyw_api_client.h"
#include "infrastructure/collectors/data_collector_service.h"
#include "interfaces/udp/resource_monitor_broadcaster.h"
#include "interfaces/http/alert_receiver_server.h"
#include "infrastructure/config/config_manager.h"
#include <iostream>
#include <memory>
#include <thread>

using namespace app::infrastructure;
using namespace app::domain;
using namespace app::interfaces;

int main() {
    std::cout << "=== 系统启动：初始化机箱数据 ===" << std::endl;
    
    // 0. 加载配置
    ConfigManager::LoadFromFile("config.json");

    // 1. 创建仓储
    auto chassisRepo = std::make_shared<InMemoryChassisRepository>();
    auto stackRepo = std::make_shared<InMemoryStackRepository>();
    
    // 2. 创建工厂类并初始化系统拓扑
    ChassisFactory factory;
    auto configs = ChassisFactory::CreateDefaultConfigs();
    auto topology = factory.CreateFullTopology(configs);
    
    // 3. 保存所有机箱到仓储
    for (const auto& chassis : topology) {
        chassisRepo->Save(chassis);
    }
    
    std::cout << "\n初始化完成！仓储中共有 " << chassisRepo->Size() << " 个机箱" << std::endl;
    
    // 4. 创建 API 客户端（读取配置）
    std::string apiBaseUrl = ConfigManager::GetString("/api/base_url", "localhost");
    int apiPort = ConfigManager::GetInt("/api/port", 8080);
    auto apiClient = std::make_shared<QywApiClient>(apiBaseUrl, apiPort);
    
    // 设置API端点路径
    apiClient->SetEndpoint("boardinfo", ConfigManager::GetString("/api/endpoints/boardinfo", "/api/v1/external/qyw/boardinfo"));
    apiClient->SetEndpoint("stackinfo", ConfigManager::GetString("/api/endpoints/stackinfo", "/api/v1/external/qyw/stackinfo"));
    apiClient->SetEndpoint("deploy", ConfigManager::GetString("/api/endpoints/deploy", "/api/v1/external/qyw/deploy"));
    apiClient->SetEndpoint("undeploy", ConfigManager::GetString("/api/endpoints/undeploy", "/api/v1/external/qyw/undeploy"));
    apiClient->SetEndpoint("heartbeat", ConfigManager::GetString("/api/endpoints/heartbeat", "/api/v1/sys-config/client/up"));
    
    // 5. 创建UDP组播服务（读取配置）
    std::cout << "\n创建UDP组播服务..." << std::endl;
    std::string udpBroadcasterGroup = ConfigManager::GetString("/udp/broadcaster/multicast_group", "234.186.1.99");
    uint16_t udpPort = static_cast<uint16_t>(ConfigManager::GetInt("/udp/port", 0x100A));
    auto broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
        chassisRepo, stackRepo, apiClient, udpBroadcasterGroup, udpPort);
    
    // 设置UDP广播器命令码
    broadcaster->SetCommand(
        ConfigManager::GetHexUint16("/udp/commands/resource_monitor", 0xF000),
        ConfigManager::GetHexUint16("/udp/commands/task_query_resp", 0xF105),
        ConfigManager::GetHexUint16("/udp/commands/task_start_resp", 0xF103),
        ConfigManager::GetHexUint16("/udp/commands/task_stop_resp", 0xF104),
        ConfigManager::GetHexUint16("/udp/commands/fault_report", 0xF107)
    );
    broadcaster->Start();

    auto listener = std::make_shared<ResourceMonitorListener>(
        broadcaster,
        ConfigManager::GetString("/udp/listener/multicast_group", "234.186.1.98"),
        udpPort);
    
    // 设置UDP监听器命令码
    listener->SetCommand(
        ConfigManager::GetHexUint16("/udp/commands/resource_monitor", 0xF000),
        ConfigManager::GetHexUint16("/udp/commands/task_query", 0xF005),
        ConfigManager::GetHexUint16("/udp/commands/task_start", 0xF003),
        ConfigManager::GetHexUint16("/udp/commands/task_stop", 0xF004)
    );
    listener->Start();
    
    // 6. 创建HTTP告警接收服务器（读取配置）
    std::cout << "\n创建HTTP告警接收服务器..." << std::endl;
    int httpAlertPort = ConfigManager::GetInt("/alert_server/port", 8888);
    std::string httpAlertHost = ConfigManager::GetString("/alert_server/host", "0.0.0.0");
    auto alertServer = std::make_shared<AlertReceiverServer>(chassisRepo, stackRepo, broadcaster, httpAlertPort, httpAlertHost);
    alertServer->Start();
    
    // 7. 创建数据采集服务（读取配置）
    std::cout << "\n创建数据采集服务（采集间隔：10秒）..." << std::endl;
    std::string clientIp = ConfigManager::GetString("/heartbeat/client_ip", "192.168.6.222");
    int intervalSeconds = ConfigManager::GetInt("/collector/interval_seconds", 10);
    int boardTimeoutSeconds = ConfigManager::GetInt("/collector/board_timeout_seconds", 120);
    DataCollectorService collector(chassisRepo, stackRepo, apiClient, clientIp, intervalSeconds, boardTimeoutSeconds);
    
    // 8. 启动数据采集（在后台线程运行）
    std::cout << "启动数据采集服务..." << std::endl;
    collector.Start();
    
    // 9. 主线程等待
    std::cout << "\n系统运行中... (按 Ctrl+C 退出)" << std::endl;
    try {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
    }
    
    // 10. 清理
    std::cout << "\n正在停止服务..." << std::endl;
    collector.Stop();
    alertServer->Stop();
    listener->Stop();
    broadcaster->Stop();
    
    std::cout << "\n系统运行结束" << std::endl;
    
    return 0;
}
