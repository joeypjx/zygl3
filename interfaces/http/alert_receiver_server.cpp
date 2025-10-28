#include "alert_receiver_server.h"
#include "../../json.hpp"
#include <iostream>
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
        std::cout << "告警接收服务器已在运行" << std::endl;
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
    std::cout << "告警接收服务器已启动，监听端口: " << m_port << std::endl;
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
    std::cout << "告警接收服务器已停止" << std::endl;
}

void AlertReceiverServer::ServerLoop() {
    std::string addr = m_host + ":" + std::to_string(m_port);
    m_server.listen(m_host.c_str(), m_port);
}

void AlertReceiverServer::HandleBoardAlert(const httplib::Request& req, httplib::Response& res) {
    try {
        std::cout << "\n收到板卡异常上报..." << std::endl;
        
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
        
        std::cout << "板卡异常信息:" << std::endl;
        std::cout << "  机箱: " << alert.chassisNumber << " (" << alert.chassisName << ")" << std::endl;
        std::cout << "  板卡: " << alert.boardNumber << " (" << alert.boardName << ")" << std::endl;
        std::cout << "  IP地址: " << alert.boardAddress << std::endl;
        std::cout << "  板卡状态: " << (alert.boardStatus == 0 ? "正常" : "异常") << std::endl;
        std::cout << "  告警信息数量: " << alert.alertMessages.size() << std::endl;
        for (size_t i = 0; i < alert.alertMessages.size(); ++i) {
            std::cout << "    告警" << (i + 1) << ": " << alert.alertMessages[i] << std::endl;
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
        
        // TODO: 这里可以根据需要更新仓储中的数据或做其他处理
        // 例如：更新机箱板卡状态为异常
        
        // 发送成功响应
        SendSuccessResponse(res);
        
    } catch (const json::exception& e) {
        std::cerr << "解析板卡异常上报JSON失败: " << e.what() << std::endl;
        SendErrorResponse(res, "无效的JSON格式: " + std::string(e.what()));
    } catch (const std::exception& e) {
        std::cerr << "处理板卡异常上报失败: " << e.what() << std::endl;
        SendErrorResponse(res, "处理失败: " + std::string(e.what()));
    }
}

void AlertReceiverServer::HandleServiceAlert(const httplib::Request& req, httplib::Response& res) {
    try {
        std::cout << "\n收到组件异常上报..." << std::endl;
        
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
                taskAlert.taskStatus = taskJson.value("taskStatus", "");
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
        
        std::cout << "组件异常信息:" << std::endl;
        std::cout << "  业务链路: " << alert.stackName << " (UUID: " << alert.stackUUID << ")" << std::endl;
        std::cout << "  组件: " << alert.serviceName << " (UUID: " << alert.serviceUUID << ")" << std::endl;
        std::cout << "  异常任务数量: " << alert.taskAlertInfos.size() << std::endl;
        
        for (const auto& taskAlert : alert.taskAlertInfos) {
            std::cout << "    任务ID: " << taskAlert.taskID << std::endl;
            std::cout << "      任务状态: " << taskAlert.taskStatus << std::endl;
            std::cout << "      运行位置: 机箱" << taskAlert.chassisNumber << 
                         ", 板卡" << taskAlert.boardNumber << 
                         " (" << taskAlert.boardAddress << ")" << std::endl;
            std::cout << "      告警数量: " << taskAlert.alertMessages.size() << std::endl;
            for (const auto& msg : taskAlert.alertMessages) {
                std::cout << "        - " << msg << std::endl;
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
        std::cerr << "解析组件异常上报JSON失败: " << e.what() << std::endl;
        SendErrorResponse(res, "无效的JSON格式: " + std::string(e.what()));
    } catch (const std::exception& e) {
        std::cerr << "处理组件异常上报失败: " << e.what() << std::endl;
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

