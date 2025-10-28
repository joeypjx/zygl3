#include "data_collector_service.h"
#include <iostream>

namespace app::infrastructure {

DataCollectorService::DataCollectorService(
    std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
    std::shared_ptr<app::domain::IStackRepository> stackRepo,
    std::shared_ptr<QywApiClient> apiClient,
    const std::string& clientIp,
    int intervalSeconds)
    : m_chassisRepo(chassisRepo)
    , m_stackRepo(stackRepo)
    , m_apiClient(apiClient)
    , m_running(false)
    , m_intervalSeconds(intervalSeconds)
    , m_clientIp(clientIp) {
}

DataCollectorService::~DataCollectorService() {
    Stop();
}

void DataCollectorService::Start() {
    if (m_running) {
        std::cout << "数据采集服务已在运行" << std::endl;
        return;
    }
    
    m_running = true;
    m_collectThread = std::thread(&DataCollectorService::CollectLoop, this);
    std::cout << "数据采集服务已启动" << std::endl;
}

void DataCollectorService::Stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    if (m_collectThread.joinable()) {
        m_collectThread.join();
    }
    std::cout << "数据采集服务已停止" << std::endl;
}

void DataCollectorService::CollectLoop() {
    while (m_running) {
        try {
            std::cout << "\n开始采集数据..." << std::endl;
            
            // 发送心跳保活
            m_apiClient->SendHeartbeat(m_clientIp);
            
            // 采集板卡信息
            CollectBoardInfo();
            
            // 采集业务链路信息
            CollectStackInfo();
            
            // 检查板卡在线状态，将超时的标记为离线
            CheckAndMarkOfflineBoards();
            
            std::cout << "数据采集完成，等待 " << m_intervalSeconds << " 秒..." << std::endl;
            
            // 等待指定时间
            std::this_thread::sleep_for(std::chrono::seconds(m_intervalSeconds));
            
        } catch (const std::exception& e) {
            std::cerr << "采集数据时发生异常: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(m_intervalSeconds));
        }
    }
}

void DataCollectorService::CollectBoardInfo() {
    std::cout << "  采集板卡信息..." << std::endl;
    
    // 调用API获取板卡信息
    auto boardInfos = m_apiClient->GetBoardInfo();
    
    if (boardInfos.empty()) {
        std::cout << "  板卡信息为空，可能是API未返回数据" << std::endl;
        return;
    }
    
    std::cout << "  获取到 " << boardInfos.size() << " 条板卡信息" << std::endl;
    
    // 将 API 响应转换为领域对象并更新仓储
    for (const auto& apiBoardInfo : boardInfos) {
        // 根据机箱号查找机箱
        auto chassis = m_chassisRepo->FindByNumber(apiBoardInfo.chassisNumber);
        
        if (chassis) {
            // 创建或获取板卡对象
            auto* board = chassis->GetBoardBySlot(apiBoardInfo.boardNumber);
            
            if (board) {
                // 转换为 TaskStatusInfo 列表
                std::vector<app::domain::TaskStatusInfo> taskInfos;
                for (const auto& apiTask : apiBoardInfo.taskInfos) {
                    app::domain::TaskStatusInfo taskInfo;
                    taskInfo.taskID = apiTask.taskID;
                    taskInfo.taskStatus = apiTask.taskStatus;
                    taskInfo.serviceName = apiTask.serviceName;
                    taskInfo.serviceUUID = apiTask.serviceUUID;
                    taskInfo.stackName = apiTask.stackName;
                    taskInfo.stackUUID = apiTask.stackUUID;
                    taskInfos.push_back(taskInfo);
                }
                
                // 更新板卡状态
                board->UpdateFromApiData(apiBoardInfo.boardStatus, taskInfos);
                
                // 更新到仓储
                m_chassisRepo->UpdateBoard(apiBoardInfo.chassisNumber, 
                                           apiBoardInfo.boardNumber, 
                                           *board);
            } else {
                std::cerr << "  未找到板卡: 机箱" << apiBoardInfo.chassisNumber 
                          << ", 槽位" << apiBoardInfo.boardNumber << std::endl;
            }
        } else {
            std::cerr << "  未找到机箱: " << apiBoardInfo.chassisNumber << std::endl;
        }
    }
    
    std::cout << "  板卡信息更新完成" << std::endl;
}

void DataCollectorService::CollectStackInfo() {
    std::cout << "  采集业务链路信息..." << std::endl;
    
    // 调用API获取业务链路信息
    auto stackInfos = m_apiClient->GetStackInfo();
    
    if (stackInfos.empty()) {
        std::cout << "  业务链路信息为空，可能是API未返回数据" << std::endl;
        // 清空现有的stacks
        m_stackRepo->Clear();
        return;
    }
    
    std::cout << "  获取到 " << stackInfos.size() << " 条业务链路信息" << std::endl;
    
    // 清空现有数据，实现一次性替换
    m_stackRepo->Clear();
    
    // 将 API 响应转换为领域对象并更新仓储
    for (const auto& apiStackInfo : stackInfos) {
        // 创建业务链路对象
        auto stack = std::make_shared<app::domain::Stack>(
            apiStackInfo.stackUUID, 
            apiStackInfo.stackName
        );
        
        // 设置标签信息
        std::vector<app::domain::StackLabelInfo> labels;
        for (const auto& apiLabel : apiStackInfo.stackLabelInfos) {
            app::domain::StackLabelInfo label;
            label.stackLabelUUID = apiLabel.stackLabelUUID;
            label.stackLabelName = apiLabel.stackLabelName;
            labels.push_back(label);
        }
        if (!labels.empty()) {
            stack->SetLabels(labels);
        }
        
        // 更新部署状态和运行状态
        stack->UpdateDeployStatus(apiStackInfo.stackDeployStatus);
        stack->UpdateRunningStatus(apiStackInfo.stackRunningStatus);
        
        // 处理组件信息
        for (const auto& apiServiceInfo : apiStackInfo.serviceInfos) {
            // 创建组件对象
            app::domain::Service service(
                apiServiceInfo.serviceUUID,
                apiServiceInfo.serviceName,
                apiServiceInfo.serviceType
            );
            service.UpdateStatus(apiServiceInfo.serviceStatus);
            
            // 处理任务信息
            for (const auto& apiTaskInfo : apiServiceInfo.taskInfos) {
                // 创建任务对象
                app::domain::Task task(apiTaskInfo.taskID, apiTaskInfo.taskStatus);
                task.SetBoardAddress(apiTaskInfo.boardAddress);
                
                // 设置资源使用情况
                app::domain::ResourceUsage resources;
                resources.cpuCores = apiTaskInfo.cpuCores;
                resources.cpuUsed = apiTaskInfo.cpuUsed;
                resources.cpuUsage = apiTaskInfo.cpuUsage;
                resources.memorySize = apiTaskInfo.memorySize;
                resources.memoryUsed = apiTaskInfo.memoryUsed;
                resources.memoryUsage = apiTaskInfo.memoryUsage;
                resources.netReceive = apiTaskInfo.netReceive;
                resources.netSent = apiTaskInfo.netSent;
                resources.gpuMemUsed = apiTaskInfo.gpuMemUsed;
                task.UpdateResources(resources);
                
                // 添加任务到组件
                service.AddOrUpdateTask(apiTaskInfo.taskID, task);
            }
            
            // 添加组件到业务链路
            stack->AddOrUpdateService(service);
        }
        
        // 重新计算整体状态
        stack->CalculateOverallStatus();
        
        // 保存到仓储
        m_stackRepo->Save(stack);
    }
    
    std::cout << "  业务链路信息更新完成" << std::endl;
}

void DataCollectorService::CheckAndMarkOfflineBoards(int timeoutSeconds) {
    // 获取所有机箱
    auto allChassis = m_chassisRepo->GetAll();
    
    int offlineCount = 0;
    for (const auto& chassis : allChassis) {
        auto& boards = chassis->GetAllBoardsMutable();
        
        for (auto& board : boards) {
            // 检查板卡是否在线
            if (!board.IsOnline(timeoutSeconds)) {
                // 如果不在线且当前状态不是离线，则标记为离线
                if (board.GetStatus() != app::domain::BoardOperationalStatus::Offline) {
                    board.MarkAsOffline();
                    offlineCount++;
                    
                    std::cout << "  板卡离线: 机箱" << chassis->GetChassisNumber() 
                              << " 槽位" << board.GetBoardNumber() << std::endl;
                }
            }
        }
    }
    
    if (offlineCount > 0) {
        std::cout << "  检测到 " << offlineCount << " 个板卡离线" << std::endl;
    }
}

}
