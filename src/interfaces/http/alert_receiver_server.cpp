#include "alert_receiver_server.h"
#include "nlohmann-json/json.hpp"
#include <spdlog/spdlog.h>
#include <sstream>

using nlohmann::json;

namespace app::interfaces {

/**
 * @brief 构造函数
 * @param chassisRepo 机箱仓储接口
 * @param stackRepo 业务链路仓储接口
 * @param broadcaster UDP广播器（用于发送故障上报）
 * @param apiClient API客户端（用于发送心跳）
 * @param heartbeatService 心跳服务（可选，用于主备角色检查）
 * @param port HTTP服务器监听端口
 * @param host HTTP服务器监听地址（"0.0.0.0"表示监听所有接口）
 * @param heartbeatInterval 心跳发送间隔（秒）
 */
AlertReceiverServer::AlertReceiverServer(
    std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
    std::shared_ptr<app::domain::IStackRepository> stackRepo,
    std::shared_ptr<ResourceMonitorBroadcaster> broadcaster,
    std::shared_ptr<app::infrastructure::QywApiClient> apiClient,
    std::shared_ptr<app::infrastructure::HeartbeatService> heartbeatService,
    int port,
    const std::string& host,
    int heartbeatInterval)
    : m_chassisRepo(chassisRepo)
    , m_stackRepo(stackRepo)
    , m_broadcaster(broadcaster)
    , m_apiClient(apiClient)
    , m_heartbeatService(heartbeatService)
    , m_port(port)
    , m_host(host)
    , m_heartbeatInterval(heartbeatInterval)
    , m_running(false) {
}

AlertReceiverServer::~AlertReceiverServer() {
    Stop();
}

/**
 * @brief 启动HTTP告警接收服务器
 * @note 启动两个线程：HTTP服务器线程和心跳发送线程
 */
void AlertReceiverServer::Start() {
    if (m_running) {
        spdlog::info("告警接收服务器已在运行");
        return;
    }
    
    // 设置路由
    m_server.Post("/api/v1/alert/board", 
        [this](const httplib::Request& req, httplib::Response& res) {
            this->HandleBoardAlert(req, res);
        });
    
    m_server.Post("/api/v1/alert/service", 
        [this](const httplib::Request& req, httplib::Response& res) {
            this->HandleServiceAlert(req, res);
        });
    
    m_running = true;
    m_serverThread = std::thread(&AlertReceiverServer::ServerLoop, this);
    m_heartbeatThread = std::thread(&AlertReceiverServer::HeartbeatLoop, this);
    spdlog::info("告警接收服务器已启动，监听端口: {}", m_port);
}

/**
 * @brief 停止HTTP告警接收服务器
 * @note 停止HTTP服务器和心跳线程，等待线程结束
 */
void AlertReceiverServer::Stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    m_server.stop();
    
    // 等待HTTP服务器线程结束
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    
    // 等待心跳线程结束
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.join();
    }
    spdlog::info("告警接收服务器已停止");
}

/**
 * @brief HTTP服务器循环（在独立线程中运行）
 * @note 阻塞调用，直到服务器停止
 */
void AlertReceiverServer::ServerLoop() {
    m_server.listen(m_host.c_str(), m_port);
}

/**
 * @brief 心跳发送循环（在独立线程中运行）
 * @note 定期向上游API发送IP心跳，启动后立即发送一次
 */
void AlertReceiverServer::HeartbeatLoop() {
    // 启动后立即发送一次心跳
    SendHeartbeat();
    
    // 按配置的间隔定期发送心跳
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(m_heartbeatInterval));
        if (m_running) {
            SendHeartbeat();
        }
    }
}

/**
 * @brief 发送IP心跳检测
 * @note 只有主节点才发送心跳，备节点不发送
 *       使用alert_server的host和port作为心跳的IP和端口
 */
void AlertReceiverServer::SendHeartbeat() {
    // 检查角色：只有主节点才发送心跳
    if (m_heartbeatService) {
        if (!m_heartbeatService->IsPrimary()) {
            spdlog::debug("当前为备节点，不发送IP心跳检测");
            return;
        }
    }
    
    if (!m_apiClient) {
        return;
    }
    
    spdlog::debug("发送IP心跳检测...");
    // 新版接口需要传递IP和端口，直接使用m_host和m_port
    m_apiClient->SendHeartbeat(m_host, std::to_string(m_port));
}

/**
 * @brief 处理板卡异常上报请求
 * @param req HTTP请求对象
 * @param res HTTP响应对象
 * @note 新版API要求请求体为对象数组格式，支持批量上报
 *       处理流程：1.解析JSON数组 2.更新板卡状态 3.发送UDP故障上报 4.返回响应
 */
void AlertReceiverServer::HandleBoardAlert(const httplib::Request& req, httplib::Response& res) {
    try {
        spdlog::info("收到板卡异常上报...");
        
        // 解析JSON请求（新版为对象数组）
        json j = json::parse(req.body);
        
        // 检查是否为数组格式（新版API要求）
        if (!j.is_array()) {
            spdlog::error("板卡异常上报请求格式错误：应为数组格式");
            SendErrorResponse(res, "请求格式错误：应为数组格式");
            return;
        }
        
        // 处理数组中的每个板卡告警（支持批量上报）
        for (const auto& alertJson : j) {
            // 解析板卡告警信息
            BoardAlertRequest alert;
            alert.chassisName = alertJson.value("chassisName", "");
            alert.chassisNumber = alertJson.value("chassisNumber", 0);
            alert.boardName = alertJson.value("boardName", "");
            alert.boardNumber = alertJson.value("boardNumber", 0);
            alert.boardType = alertJson.value("boardType", 0);
            alert.boardAddress = alertJson.value("boardAddress", "");
            alert.boardStatus = alertJson.value("boardStatus", 0);  // 0-正常, 1-异常, 2-不在位
            alert.alertMsg = alertJson.value("alertMsg", "");  // 新版为字符串字段（之前是数组）
            
            // 记录告警信息日志
            spdlog::info("板卡异常信息:");
            spdlog::info("  机箱: {} ({})", alert.chassisNumber, alert.chassisName);
            spdlog::info("  板卡: {} ({})", alert.boardNumber, alert.boardName);
            spdlog::info("  IP地址: {}", alert.boardAddress);
            std::string statusStr = (alert.boardStatus == 0 ? "正常" : 
                                    (alert.boardStatus == 1 ? "异常" : "不在位"));
            spdlog::info("  板卡状态: {}", statusStr);
            if (!alert.alertMsg.empty()) {
                spdlog::info("  告警信息: {}", alert.alertMsg);
            }
            
            // 构建故障描述并发送UDP故障上报
            std::ostringstream faultDesc;
            faultDesc << "板卡异常 - 机箱:" << alert.chassisNumber 
                      << " 槽位:" << alert.boardNumber 
                      << " IP:" << alert.boardAddress;
            if (!alert.alertMsg.empty()) {
                faultDesc << " 告警:" << alert.alertMsg;
            }
            
            // 通过UDP广播器发送故障上报（问题代码0表示板卡故障）
            if (m_broadcaster) {
                m_broadcaster->SendFaultReport(faultDesc.str(), 0);
            }
            
            // 更新仓储中的板卡状态
            // 根据机箱号和板卡IP地址（或槽位号）找到对应的机箱和板卡
            auto chassis = m_chassisRepo->FindByNumber(alert.chassisNumber);
            if (chassis) {
                // 优先通过IP地址查找板卡（更准确）
                auto* board = chassis->GetBoardByAddress(alert.boardAddress);
                // 如果通过IP地址找不到，尝试通过槽位号查找（备用方案）
                if (!board && alert.boardNumber > 0) {
                    board = chassis->GetBoardBySlot(alert.boardNumber);
                }
                
                if (board) {
                    // 更新板卡状态：statusFromApi=0表示正常，1表示异常，2表示不在位
                    board->UpdateStatus(alert.boardStatus);
                    
                    // 保存更新后的板卡到仓储
                    // 使用槽位号或IP地址对应的槽位号
                    int slotNumber = alert.boardNumber > 0 ? alert.boardNumber : board->GetBoardNumber();
                    if (slotNumber > 0) {
                        m_chassisRepo->UpdateBoard(alert.chassisNumber, slotNumber, *board);
                        spdlog::info("已更新板卡状态: 机箱{} 槽位{} 状态={}", 
                                     alert.chassisNumber, slotNumber, statusStr);
                    }
                } else {
                    spdlog::error("未找到板卡: 机箱{} IP={} 槽位={}", 
                                  alert.chassisNumber, alert.boardAddress, alert.boardNumber);
                }
            } else {
                spdlog::error("未找到机箱: {}", alert.chassisNumber);
            }
        }
        
        // 发送成功响应
        SendSuccessResponse(res);
        
    } catch (const json::exception& e) {
        spdlog::error("解析板卡异常上报JSON失败: {}", e.what());
        SendErrorResponse(res, "无效的JSON格式: " + std::string(e.what()));
    } catch (const std::exception& e) {
        spdlog::error("处理板卡异常上报失败: {}", e.what());
        SendErrorResponse(res, "处理失败: " + std::string(e.what()));
    }
}

/**
 * @brief 处理组件异常上报请求
 * @param req HTTP请求对象
 * @param res HTTP响应对象
 * @note 新版API为扁平化结构，直接包含所有字段（不再嵌套在taskAlertInfos中）
 *       处理流程：1.解析JSON 2.记录日志 3.发送UDP故障上报 4.返回响应
 */
void AlertReceiverServer::HandleServiceAlert(const httplib::Request& req, httplib::Response& res) {
    try {
        spdlog::info("收到组件异常上报...");
        
        // 解析JSON请求（新版为扁平化结构，不再嵌套在taskAlertInfos数组中）
        json j = json::parse(req.body);
        
        // 解析业务链路和组件信息
        ServiceAlertRequest alert;
        alert.stackName = j.value("stackName", "");
        alert.stackUUID = j.value("stackUUID", "");
        alert.serviceName = j.value("serviceName", "");
        alert.serviceUUID = j.value("serviceUUID", "");
        alert.taskID = j.value("taskID", "");
        alert.serviceId = j.value("serviceId", "");
        
        // taskStatus 在新版文档中为字符串类型（之前是数字）
        if (j.contains("taskStatus")) {
            if (j["taskStatus"].is_string()) {
                alert.taskStatus = j["taskStatus"].get<std::string>();
            } else {
                // 兼容处理：如果是数字，转换为字符串
                alert.taskStatus = std::to_string(j["taskStatus"].get<int>());
            }
        }
        
        // 解析任务副本信息
        alert.replicaNumber = j.value("replicaNumber", 0);
        
        // 解析运行位置信息
        alert.chassisName = j.value("chassisName", "");
        alert.chassisNumber = j.value("chassisNumber", 0);
        alert.boardName = j.value("boardName", "");
        alert.boardNumber = j.value("boardNumber", 0);
        alert.boardType = j.value("boardType", 0);
        alert.boardAddress = j.value("boardAddress", "");
        alert.boardStatus = j.value("boardStatus", 0);  // 0-正常, 1-异常, 2-不在位
        alert.alertMsg = j.value("alertMsg", "");  // 新版为字符串字段（之前是数组）
        
        spdlog::info("组件异常信息:");
        spdlog::info("  业务链路: {} (UUID: {})", alert.stackName, alert.stackUUID);
        spdlog::info("  组件: {} (UUID: {}, ID: {})", alert.serviceName, alert.serviceUUID, alert.serviceId);
        spdlog::info("  任务ID: {}", alert.taskID);
        spdlog::info("  任务状态: {}", alert.taskStatus);
        spdlog::info("  副本编号: {}", alert.replicaNumber);
        std::string boardStatusStr = (alert.boardStatus == 0 ? "正常" : 
                                     (alert.boardStatus == 1 ? "异常" : "不在位"));
        spdlog::info("  运行位置: 机箱{}, 板卡{} ({})", 
                     alert.chassisNumber, alert.boardNumber, alert.boardAddress);
        spdlog::info("  板卡状态: {}", boardStatusStr);
        if (!alert.alertMsg.empty()) {
            spdlog::info("  告警信息: {}", alert.alertMsg);
        }
        
        // 构建故障描述并发送UDP故障上报
        std::ostringstream faultDesc;
        faultDesc << "组件异常 - 业务链路:" << alert.stackName 
                  << " 组件:" << alert.serviceName
                  << " 任务ID:" << alert.taskID;
        if (!alert.alertMsg.empty()) {
            faultDesc << " 告警:" << alert.alertMsg;
        }
        
        // 通过UDP广播器发送故障上报（问题代码1表示组件故障）
        if (m_broadcaster) {
            m_broadcaster->SendFaultReport(faultDesc.str(), 1);
        }
        
        // TODO: 这里可以根据需要更新仓储中的数据或做其他处理
        // 例如：更新组件状态为异常、更新任务状态等
        
        // 发送成功响应
        SendSuccessResponse(res);
        
    } catch (const json::exception& e) {
        spdlog::error("解析组件异常上报JSON失败: {}", e.what());
        SendErrorResponse(res, "无效的JSON格式: " + std::string(e.what()));
    } catch (const std::exception& e) {
        spdlog::error("处理组件异常上报失败: {}", e.what());
        SendErrorResponse(res, "处理失败: " + std::string(e.what()));
    }
}

/**
 * @brief 发送成功响应
 * @param res HTTP响应对象
 * @note 标准响应格式：{ "code": 0, "message": "success", "data": "success" }
 */
void AlertReceiverServer::SendSuccessResponse(httplib::Response& res) {
    json response;
    response["code"] = 0;
    response["message"] = "success";
    response["data"] = "success";
    
    res.set_content(response.dump(), "application/json");
}

/**
 * @brief 发送错误响应
 * @param res HTTP响应对象
 * @param message 错误消息
 * @note 标准响应格式：{ "code": -1, "message": "...", "data": "" }
 */
void AlertReceiverServer::SendErrorResponse(httplib::Response& res, const std::string& message) {
    json response;
    response["code"] = -1;
    response["message"] = message;
    response["data"] = "";
    
    res.set_content(response.dump(), "application/json");
}

}

