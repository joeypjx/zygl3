#include "infrastructure/config/chassis_factory.h"
#include "infrastructure/persistence/in_memory_chassis_repository.h"
#include "infrastructure/persistence/in_memory_stack_repository.h"
#include "infrastructure/api_client/qyw_api_client.h"
#include "infrastructure/collectors/data_collector_service.h"
#include "interfaces/udp/resource_monitor_broadcaster.h"
#include <iostream>
#include <memory>
#include <thread>

using namespace app::infrastructure;
using namespace app::domain;
using namespace app::interfaces;

int main() {
    std::cout << "=== 系统启动：初始化机箱数据 ===" << std::endl;
    
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
    
    // 4. 创建 API 客户端（实际使用时需要替换为真实地址）
    std::string apiBaseUrl = "localhost";
    int apiPort = 8080;
    auto apiClient = std::make_shared<QywApiClient>(apiBaseUrl, apiPort);
    
    // 5. 创建UDP组播服务
    std::cout << "\n创建UDP组播服务..." << std::endl;
    auto broadcaster = std::make_shared<ResourceMonitorBroadcaster>(
        chassisRepo, stackRepo, apiClient, "234.186.1.99", 0x100A);
    broadcaster->Start();

    auto listener = std::make_shared<ResourceMonitorListener>(
        broadcaster, "234.186.1.98", 0x100A);
    listener->Start();
    
    // 6. 创建数据采集服务
    std::cout << "\n创建数据采集服务（采集间隔：10秒）..." << std::endl;
    DataCollectorService collector(chassisRepo, stackRepo, apiClient, 10);
    
    // 7. 启动数据采集（在后台线程运行）
    std::cout << "启动数据采集服务..." << std::endl;
    collector.Start();
    
    // 8. 主线程等待
    std::cout << "\n系统运行中... (按 Ctrl+C 退出)" << std::endl;
    try {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
    }
    
    // 9. 清理
    std::cout << "\n正在停止服务..." << std::endl;
    collector.Stop();
    listener->Stop();
    broadcaster->Stop();
    
    std::cout << "\n系统运行结束" << std::endl;
    
    return 0;
}
