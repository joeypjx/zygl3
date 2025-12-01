#include "alert_receiver_server.h"
#include "nlohmann-json/json.hpp"
#include <spdlog/spdlog.h>
#include <sstream>

using nlohmann::json;

namespace app::interfaces {

AlertReceiverServer::AlertReceiverServer(
    std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
    std::shared_ptr<app::domain::IStackRepository> stackRepo,
    std::shared_ptr<ResourceMonitorBroadcaster> broadcaster,
    int port,
    const std::string& host)
    : m_chassisRepo(chassisRepo)
    , m_stackRepo(stackRepo)
    , m_broadcaster(broadcaster)
    , m_port(port)
    , m_host(host)
    , m_running(false) {
}

AlertReceiverServer::~AlertReceiverServer() {
    Stop();
}

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
    spdlog::info("告警接收服务器已启动，监听端口: {}", m_port);
}

void AlertReceiverServer::Stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    m_server.stop();
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    spdlog::info("告警接收服务器已停止");
}

void AlertReceiverServer::ServerLoop() {
    m_server.listen(m_host.c_str(), m_port);
}

void AlertReceiverServer::HandleBoardAlert(const httplib::Request& req, httplib::Response& res) {
    try {
        spdlog::info("收到板卡异常上报...");
        
        // 解析JSON请求
        json j = json::parse(req.body);
        
        BoardAlertRequest alert;
        alert.chassisName = j.value("chassisName", "");
        alert.chassisNumber = j.value("chassisNumber", 0);
        alert.boardName = j.value("boardName", "");
        alert.boardNumber = j.value("boardNumber", 0);
        alert.boardType = j.value("boardType", 0);
        alert.boardAddress = j.value("boardAddress", "");
        alert.boardStatus = j.value("boardStatus", 0);
        
        // 解析告警信息列表
        if (j.contains("alertMessages")) {
            for (const auto& msg : j["alertMessages"]) {
                alert.alertMessages.push_back(msg.get<std::string>());
            }
        }
        
        spdlog::info("板卡异常信息:");
        spdlog::info("  机箱: {} ({})", alert.chassisNumber, alert.chassisName);
        spdlog::info("  板卡: {} ({})", alert.boardNumber, alert.boardName);
        spdlog::info("  IP地址: {}", alert.boardAddress);
        spdlog::info("  板卡状态: {}", (alert.boardStatus == 0 ? "正常" : "异常"));
        spdlog::info("  告警信息数量: {}", alert.alertMessages.size());
        for (size_t i = 0; i < alert.alertMessages.size(); ++i) {
            spdlog::info("    告警{}: {}", i + 1, alert.alertMessages[i]);
        }
        
        // 构建故障描述并发送UDP故障上报
        std::ostringstream faultDesc;
        faultDesc << "板卡异常 - 机箱:" << alert.chassisNumber 
                  << " 槽位:" << alert.boardNumber 
                  << " IP:" << alert.boardAddress;
        if (!alert.alertMessages.empty()) {
            faultDesc << " 告警:" << alert.alertMessages[0];
        }
        
        if (m_broadcaster) {
            m_broadcaster->SendFaultReport(faultDesc.str());
        }
        
        // 更新仓储中的板卡状态为异常
        // 根据机箱号和板卡IP地址（或槽位号）找到对应的机箱和板卡
        auto chassis = m_chassisRepo->FindByNumber(alert.chassisNumber);
        if (chassis) {
            // 优先通过IP地址查找板卡
            auto* board = chassis->GetBoardByAddress(alert.boardAddress);
            // 如果通过IP地址找不到，尝试通过槽位号查找
            if (!board && alert.boardNumber > 0) {
                board = chassis->GetBoardBySlot(alert.boardNumber);
            }
            
            if (board) {
                // 更新板卡状态：statusFromApi=1表示异常，0表示正常
                board->UpdateStatus(alert.boardStatus);
                
                // 保存更新后的板卡到仓储
                // 使用槽位号或IP地址对应的槽位号
                int slotNumber = alert.boardNumber > 0 ? alert.boardNumber : board->GetBoardNumber();
                if (slotNumber > 0) {
                    m_chassisRepo->UpdateBoard(alert.chassisNumber, slotNumber, *board);
                    spdlog::info("已更新板卡状态: 机箱{} 槽位{} 状态={}", 
                                 alert.chassisNumber, slotNumber, 
                                 (alert.boardStatus == 0 ? "正常" : "异常"));
                }
            } else {
                spdlog::error("未找到板卡: 机箱{} IP={} 槽位={}", 
                              alert.chassisNumber, alert.boardAddress, alert.boardNumber);
            }
        } else {
            spdlog::error("未找到机箱: {}", alert.chassisNumber);
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

void AlertReceiverServer::HandleServiceAlert(const httplib::Request& req, httplib::Response& res) {
    try {
        spdlog::info("收到组件异常上报...");
        
        // 解析JSON请求
        json j = json::parse(req.body);
        
        ServiceAlertRequest alert;
        alert.stackName = j.value("stackName", "");
        alert.stackUUID = j.value("stackUUID", "");
        alert.serviceName = j.value("serviceName", "");
        alert.serviceUUID = j.value("serviceUUID", "");
        
        // 解析任务告警信息列表
        if (j.contains("taskAlertInfos") && j["taskAlertInfos"].is_array()) {
            for (const auto& taskJson : j["taskAlertInfos"]) {
                TaskAlertInfo taskAlert;
                taskAlert.taskID = taskJson.value("taskID", "");
                taskAlert.taskStatus = taskJson.value("taskStatus", 0);  // 1-运行中，2-已完成，3-异常，0-其他
                taskAlert.chassisName = taskJson.value("chassisName", "");
                taskAlert.chassisNumber = taskJson.value("chassisNumber", 0);
                taskAlert.boardName = taskJson.value("boardName", "");
                taskAlert.boardNumber = taskJson.value("boardNumber", 0);
                taskAlert.boardType = taskJson.value("boardType", 0);
                taskAlert.boardAddress = taskJson.value("boardAddress", "");
                taskAlert.boardStatus = taskJson.value("boardStatus", 0);
                
                if (taskJson.contains("alertMessages") && taskJson["alertMessages"].is_array()) {
                    for (const auto& msg : taskJson["alertMessages"]) {
                        taskAlert.alertMessages.push_back(msg.get<std::string>());
                    }
                }
                
                alert.taskAlertInfos.push_back(taskAlert);
            }
        }
        
        spdlog::info("组件异常信息:");
        spdlog::info("  业务链路: {} (UUID: {})", alert.stackName, alert.stackUUID);
        spdlog::info("  组件: {} (UUID: {})", alert.serviceName, alert.serviceUUID);
        spdlog::info("  异常任务数量: {}", alert.taskAlertInfos.size());
        
        for (const auto& taskAlert : alert.taskAlertInfos) {
            spdlog::info("    任务ID: {}", taskAlert.taskID);
            spdlog::info("      任务状态: {}", taskAlert.taskStatus);
            spdlog::info("      运行位置: 机箱{}, 板卡{} ({})", 
                         taskAlert.chassisNumber, taskAlert.boardNumber, taskAlert.boardAddress);
            spdlog::info("      告警数量: {}", taskAlert.alertMessages.size());
            for (const auto& msg : taskAlert.alertMessages) {
                spdlog::info("        - {}", msg);
            }
        }
        
        // 构建故障描述并发送UDP故障上报
        std::ostringstream faultDesc;
        faultDesc << "组件异常 - 业务链路:" << alert.stackName 
                  << " 组件:" << alert.serviceName;
        if (!alert.taskAlertInfos.empty()) {
            const auto& firstTask = alert.taskAlertInfos[0];
            faultDesc << " 任务ID:" << firstTask.taskID;
            if (!firstTask.alertMessages.empty()) {
                faultDesc << " 告警:" << firstTask.alertMessages[0];
            }
        }
        
        if (m_broadcaster) {
            m_broadcaster->SendFaultReport(faultDesc.str());
        }
        
        // TODO: 这里可以根据需要更新仓储中的数据或做其他处理
        // 例如：更新组件状态为异常
        
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

void AlertReceiverServer::SendSuccessResponse(httplib::Response& res) {
    json response;
    response["code"] = 0;
    response["message"] = "success";
    response["data"] = "success";
    
    res.set_content(response.dump(), "application/json");
}

void AlertReceiverServer::SendErrorResponse(httplib::Response& res, const std::string& message) {
    json response;
    response["code"] = -1;
    response["message"] = message;
    response["data"] = "";
    
    res.set_content(response.dump(), "application/json");
}

}

