#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

namespace app::domain {
    class IChassisRepository;
    class IStackRepository;
    enum class BoardType : int8_t;
    enum class BoardOperationalStatus : int8_t;
}

namespace app::infrastructure {
    class QywApiClient;
}

namespace app::interfaces {

/**
 * @brief CLI交互式命令服务
 * @detail 在独立线程中运行，提供交互式命令接口
 */
class CliService {
public:
    CliService(std::shared_ptr<class app::domain::IChassisRepository> chassisRepo,
               std::shared_ptr<class app::domain::IStackRepository> stackRepo,
               std::shared_ptr<class app::infrastructure::QywApiClient> apiClient);
    ~CliService();

    // 禁止拷贝
    CliService(const CliService&) = delete;
    CliService& operator=(const CliService&) = delete;

    /**
     * @brief 启动CLI服务
     */
    void Start();

    /**
     * @brief 停止CLI服务
     */
    void Stop();

    /**
     * @brief 检查服务是否运行中
     */
    bool IsRunning() const;

private:
    /**
     * @brief CLI线程主循环
     */
    void Run();

    /**
     * @brief 处理用户输入的命令
     */
    void ProcessCommand(const std::string& command);

    /**
     * @brief 打印帮助信息
     */
    void PrintHelp();

    /**
     * @brief 打印所有机箱的概览信息
     */
    void PrintAllChassisOverview();

    /**
     * @brief 打印指定机箱的详细信息
     */
    void PrintChassisDetail(int chassisNumber);

    /**
     * @brief 打印指定任务的详细信息
     */
    void PrintTaskDetail(int chassisNumber, int slotNumber, int taskIndex);

    /**
     * @brief 打印所有机箱的完整信息
     */
    void PrintAllChassisFullInfo();

    /**
     * @brief 打印所有业务链路的概览信息
     */
    void PrintAllStacksOverview();

    /**
     * @brief 打印指定业务链路的详细信息
     */
    void PrintStackDetail(const std::string& stackUUID);

    /**
     * @brief 打印所有业务链路的完整信息
     */
    void PrintAllStacksFullInfo();

    /**
     * @brief 将板卡类型转换为字符串
     */
    std::string BoardTypeToString(app::domain::BoardType type) const;

    /**
     * @brief 将板卡状态转换为字符串
     */
    std::string BoardStatusToString(app::domain::BoardOperationalStatus status) const;

    /**
     * @brief 将服务状态转换为字符串
     */
    std::string ServiceStatusToString(int status) const;

    /**
     * @brief 将任务状态转换为字符串
     */
    std::string TaskStatusToString(int status) const;

    /**
     * @brief 启动指定标签的业务链路
     */
    void DeployStacks(const std::vector<std::string>& labels);

    /**
     * @brief 停止指定标签的业务链路
     */
    void UndeployStacks(const std::vector<std::string>& labels);

    /**
     * @brief 打印分割线
     */
    void PrintSeparator() const;

private:
    std::shared_ptr<app::domain::IChassisRepository> m_chassisRepo;
    std::shared_ptr<app::domain::IStackRepository> m_stackRepo;
    std::shared_ptr<app::infrastructure::QywApiClient> m_apiClient;
    
    std::thread m_thread;
    std::atomic<bool> m_running;
};

} // namespace app::interfaces

