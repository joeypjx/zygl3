#pragma once

#include "domain/i_chassis_repository.h"
#include "domain/i_stack_repository.h"
#include "infrastructure/api_client/qyw_api_client.h"
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

namespace app::infrastructure {

/**
 * @brief 数据采集服务
 * @detail 周期性调用外部API，更新机箱板卡和业务数据
 */
class DataCollectorService {
public:
    DataCollectorService(
        std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
        std::shared_ptr<app::domain::IStackRepository> stackRepo,
        std::shared_ptr<QywApiClient> apiClient,
        int intervalSeconds = 10);

    ~DataCollectorService();

    /**
     * @brief 启动数据采集
     */
    void Start();

    /**
     * @brief 停止数据采集
     */
    void Stop();

    /**
     * @brief 是否正在运行
     */
    bool IsRunning() const { return m_running; }

private:
    /**
     * @brief 采集线程主循环
     */
    void CollectLoop();

    /**
     * @brief 采集板卡信息并更新仓储
     */
    void CollectBoardInfo();

    /**
     * @brief 采集业务链路信息并更新仓储
     */
    void CollectStackInfo();

private:
    std::shared_ptr<app::domain::IChassisRepository> m_chassisRepo;
    std::shared_ptr<app::domain::IStackRepository> m_stackRepo;
    std::shared_ptr<QywApiClient> m_apiClient;

    std::atomic<bool> m_running;
    std::thread m_collectThread;
    int m_intervalSeconds;
};

}
