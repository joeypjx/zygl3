#include "data_collector_service.h"
#include <spdlog/spdlog.h>

namespace app::infrastructure {

DataCollectorService::DataCollectorService(
    std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
    std::shared_ptr<app::domain::IStackRepository> stackRepo,
    std::shared_ptr<QywApiClient> apiClient,
    const std::string& clientIp,
    int intervalSeconds,
    int boardTimeoutSeconds)
    : m_chassisRepo(chassisRepo)
    , m_stackRepo(stackRepo)
    , m_apiClient(apiClient)
    , m_running(false)
    , m_intervalSeconds(intervalSeconds)
    , m_clientIp(clientIp)
    , m_boardTimeoutSeconds(boardTimeoutSeconds) {
}

DataCollectorService::~DataCollectorService() {
    Stop();
}

void DataCollectorService::Start() {
    if (m_running) {
        spdlog::info("数据采集服务已在运行");
        return;
    }
    
    m_running = true;
    m_collectThread = std::thread(&DataCollectorService::CollectLoop, this);
    spdlog::info("数据采集服务已启动");
}

void DataCollectorService::Stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    if (m_collectThread.joinable()) {
        m_collectThread.join();
    }
    spdlog::info("数据采集服务已停止");
}

void DataCollectorService::CollectLoop() {
    while (m_running) {
        try {
            spdlog::info("开始采集数据...");
            
            // 发送心跳保活
            SendHeartbeat();
            
            // 采集板卡信息
            CollectBoardInfo();
            
            // 采集业务链路信息
            CollectStackInfo();
            
            // 检查板卡在线状态，将超时的标记为离线
            CheckAndMarkOfflineBoards(m_boardTimeoutSeconds);
            
            spdlog::info("数据采集完成，等待 {} 秒...", m_intervalSeconds);
            
            // 等待指定时间
            std::this_thread::sleep_for(std::chrono::seconds(m_intervalSeconds));
            
        } catch (const std::exception& e) {
            spdlog::error("采集数据时发生异常: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(m_intervalSeconds));
        }
    }
}

void DataCollectorService::CollectBoardInfo() {
    spdlog::info("  采集板卡信息...");
    
    // 调用API获取板卡信息
    auto boardInfos = m_apiClient->GetBoardInfo();
    
    if (boardInfos.empty()) {
        spdlog::info("  板卡信息为空，可能是API未返回数据");
        return;
    }
    
    spdlog::info("  获取到 {} 条板卡信息", boardInfos.size());
    
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
                
                // 转换为 FanSpeed 列表
                std::vector<app::domain::FanSpeed> fanSpeeds;
                for (const auto& apiFanSpeed : apiBoardInfo.fanSpeeds) {
                    app::domain::FanSpeed fanSpeed;
                    fanSpeed.fanName = apiFanSpeed.fanName;
                    fanSpeed.speed = apiFanSpeed.speed;
                    fanSpeeds.push_back(fanSpeed);
                }
                
                // 更新板卡状态
                board->UpdateFromApiData(apiBoardInfo.boardName,
                                        apiBoardInfo.boardAddress,
                                        static_cast<app::domain::BoardType>(apiBoardInfo.boardType),
                                        apiBoardInfo.boardStatus,
                                        apiBoardInfo.voltage,
                                        apiBoardInfo.current,
                                        apiBoardInfo.temperature,
                                        fanSpeeds,
                                        taskInfos);
                
                // 更新到仓储
                m_chassisRepo->UpdateBoard(apiBoardInfo.chassisNumber, 
                                           apiBoardInfo.boardNumber, 
                                           *board);
            } else {
                spdlog::error("  未找到板卡: 机箱{}, 槽位{}", 
                              apiBoardInfo.chassisNumber, apiBoardInfo.boardNumber);
            }
        } else {
            spdlog::error("  未找到机箱: {}", apiBoardInfo.chassisNumber);
        }
    }
    
    spdlog::info("  板卡信息更新完成");
}

void DataCollectorService::SendHeartbeat() {
    spdlog::info("  发送IP心跳检测...");
    m_apiClient->SendHeartbeat(m_clientIp);
}

void DataCollectorService::CollectStackInfo() {
    spdlog::info("  采集业务链路信息...");
    
    // 调用API获取业务链路信息
    bool apiSuccess = false;
    auto stackInfos = m_apiClient->GetStackInfo(apiSuccess);
    
    // 调试日志：确认 apiSuccess 的值
    spdlog::info("  API调用结果: success={}, stackInfos.size()={}", apiSuccess, stackInfos.size());
    
    // 只有在API调用成功时才更新repository
    // 如果API调用失败，保留现有数据，避免因网络问题等临时故障导致数据丢失
    if (!apiSuccess) {
        spdlog::warn("  API调用失败，保留现有业务链路数据");
        return;
    }
    
    if (stackInfos.empty()) {
        spdlog::info("  业务链路信息为空（API调用成功但无数据），清空repository");
        // API调用成功但返回空列表，清空现有的stacks
        m_stackRepo->Clear();
        return;
    }
    
    spdlog::info("  获取到 {} 条业务链路信息", stackInfos.size());
    
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
        
        // 保存到仓储
        m_stackRepo->Save(stack);
    }
    
    spdlog::info("  业务链路信息更新完成");
}

void DataCollectorService::CheckAndMarkOfflineBoards(int timeoutSeconds) {
    // 获取所有机箱
    auto allChassis = m_chassisRepo->GetAll();
    
    int offlineCount = 0;
    for (const auto& chassis : allChassis) {
        auto& boards = chassis->GetAllBoardsMutable();
        int chassisNumber = chassis->GetChassisNumber();
        
        for (auto& board : boards) {
            // 检查板卡是否在线，如果不在线则标记为离线
            if (board.CheckAndMarkOfflineIfNeeded(timeoutSeconds)) {
                offlineCount++;
                
                // 保存更新后的板卡到仓储
                int slotNumber = board.GetBoardNumber();
                if (slotNumber > 0) {
                    m_chassisRepo->UpdateBoard(chassisNumber, slotNumber, board);
                }
                
                spdlog::info("  板卡离线: 机箱{} 槽位{}", chassisNumber, slotNumber);
            }
        }
    }
    
    if (offlineCount > 0) {
        spdlog::info("  检测到 {} 个板卡离线", offlineCount);
    }
}

}
