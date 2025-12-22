#include "data_collector_service.h"
#include <spdlog/spdlog.h>

namespace app::infrastructure {

DataCollectorService::DataCollectorService(
    std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
    std::shared_ptr<app::domain::IStackRepository> stackRepo,
    std::shared_ptr<QywApiClient> apiClient,
    int intervalSeconds,
    int boardTimeoutSeconds)
    : m_chassisRepo(chassisRepo)
    , m_stackRepo(stackRepo)
    , m_apiClient(apiClient)
    , m_running(false)
    , m_intervalSeconds(intervalSeconds)
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
            spdlog::debug("开始采集数据...");
            
            // 采集板卡信息
            CollectBoardInfo();
            
            // 采集业务链路信息
            CollectStackInfo();
            
            // 检查板卡超时状态，将超时且状态为Normal的标记为Abnormal
            CheckAndMarkAbnormalBoards(m_boardTimeoutSeconds);
            
            spdlog::debug("数据采集完成，等待 {} 秒...", m_intervalSeconds);
            
            // 等待指定时间
            std::this_thread::sleep_for(std::chrono::seconds(m_intervalSeconds));
            
        } catch (const std::exception& e) {
            spdlog::error("采集数据时发生异常: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(m_intervalSeconds));
        }
    }
}

void DataCollectorService::CollectBoardInfo() {
    spdlog::debug("  采集板卡信息...");
    
    // 调用API获取板卡信息
    auto boardInfos = m_apiClient->GetBoardInfo();
    
    if (boardInfos.empty()) {
        spdlog::info("  板卡信息为空，可能是API未返回数据");
        return;
    }
    
    spdlog::debug("  获取到 {} 条板卡信息", boardInfos.size());
    
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
                                        apiBoardInfo.voltage12V,
                                        apiBoardInfo.voltage33V,
                                        apiBoardInfo.current12A,
                                        apiBoardInfo.current33A,
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
    
    spdlog::debug("  板卡信息更新完成");
}

void DataCollectorService::CollectStackInfo() {
    spdlog::debug("  采集业务链路信息...");
    
    // 调用API获取业务链路信息
    bool apiSuccess = false;
    auto stackInfos = m_apiClient->GetStackInfo(apiSuccess);
        
    // 只有在API调用成功时才更新repository
    // 如果API调用失败，保留现有数据，避免因网络问题等临时故障导致数据丢失
    if (!apiSuccess) {
        spdlog::warn("  API调用失败，保留现有业务链路数据");
        return;
    }
    
    if (stackInfos.empty()) {
        spdlog::debug("  业务链路信息为空（API调用成功但无数据），清空repository");
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
        
        // 设置标签信息（新版API返回字符串数组，字符串为标签UUID）
        if (!apiStackInfo.stackLabelInfos.empty()) {
            stack->SetLabels(apiStackInfo.stackLabelInfos);
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

void DataCollectorService::CheckAndMarkAbnormalBoards(int timeoutSeconds) {
    // 获取所有机箱
    auto allChassis = m_chassisRepo->GetAll();
    
    int abnormalCount = 0;
    for (const auto& chassis : allChassis) {
        auto& boards = chassis->GetAllBoardsMutable();
        int chassisNumber = chassis->GetChassisNumber();
        
        for (auto& board : boards) {
            // 获取槽位号
            int slotNumber = board.GetBoardNumber();
            
            // 打印状态异常或不在位的板卡信息
            auto status = board.GetStatus();
            if (status == app::domain::BoardOperationalStatus::Abnormal) {
                spdlog::info("  板卡状态异常: 机箱{} 槽位{} IP:{}", 
                           chassisNumber, slotNumber, board.GetAddress());
            } else if (status == app::domain::BoardOperationalStatus::Offline) {
                spdlog::info("  板卡不在位: 机箱{} 槽位{} IP:{}", 
                           chassisNumber, slotNumber, board.GetAddress());
            }
            
            // 跳过槽位6和7的板卡超时检查
            if (slotNumber == 6 || slotNumber == 7) {
                continue;
            }
            
            // 检查板卡是否超时，如果超时且状态是Normal则标记为Abnormal
            if (board.CheckAndMarkAbnormalIfNeeded(timeoutSeconds)) {
                abnormalCount++;
                
                // 保存更新后的板卡到仓储
                if (slotNumber > 0) {
                    m_chassisRepo->UpdateBoard(chassisNumber, slotNumber, board);
                }
                
                spdlog::info("  板卡超时异常: 机箱{} 槽位{}", chassisNumber, slotNumber);
            }
        }
    }
    
    if (abnormalCount > 0) {
        spdlog::info("  检测到 {} 个板卡超时异常", abnormalCount);
    }
}

}
