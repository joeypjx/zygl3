#include "qyw_api_client.h"
#include "nlohmann-json/json.hpp"
#include <spdlog/spdlog.h>

using nlohmann::json;

namespace app::infrastructure {

QywApiClient::QywApiClient(const std::string& baseUrl, int port)
    : m_baseUrl(baseUrl), m_port(port), m_client(baseUrl.c_str(), port) {
}

void QywApiClient::SetEndpoint(const std::string& name, const std::string& path) {
    if (name == "boardinfo") {
        m_boardinfoEndpoint = path;
    } else if (name == "stackinfo") {
        m_stackinfoEndpoint = path;
    } else if (name == "deploy") {
        m_deployEndpoint = path;
    } else if (name == "undeploy") {
        m_undeployEndpoint = path;
    } else if (name == "heartbeat") {
        m_heartbeatEndpoint = path;
    } else if (name == "reset") {
        m_resetEndpoint = path;
    }
}

std::vector<BoardInfoResponse> QywApiClient::GetBoardInfo() {
    std::vector<BoardInfoResponse> result;
    
    auto res = m_client.Get(m_boardinfoEndpoint.c_str());
    
    if (res && res->status == 200) {
        try {
            result = ParseBoardInfoResponse(res->body);
            spdlog::info("成功获取板卡信息，共 {} 条", result.size());
        } catch (const std::exception& e) {
            spdlog::error("解析板卡信息失败: {}", e.what());
        }
    } else {
        spdlog::error("获取板卡信息失败，状态码: {}", (res ? res->status : -1));
    }
    
    return result;
}

std::vector<StackInfoResponse> QywApiClient::GetStackInfo(bool& success) {
    std::vector<StackInfoResponse> result;
    success = false;
    
    // 业务链路详情接口改为POST请求
    json requestBody;  // POST请求体可以为空
    auto res = m_client.Post(m_stackinfoEndpoint.c_str(),
                             requestBody.dump(),
                             "application/json");
    
    if (res && res->status == 200) {
        try {
            result = ParseStackInfoResponse(res->body);
            success = true;  // API调用成功
            spdlog::info("成功获取业务链路信息，共 {} 条", result.size());
        } catch (const std::exception& e) {
            spdlog::error("解析业务链路信息失败: {}", e.what());
            // success 保持为 false，表示解析失败
            spdlog::debug("GetStackInfo: 解析失败，success保持为false");
        }
    } else {
        spdlog::error("获取业务链路信息失败，状态码: {}", (res ? res->status : -1));
        // success 保持为 false，表示API调用失败
        spdlog::debug("GetStackInfo: API调用失败，success保持为false");
    }
    
    return result;
}

DeployResponse QywApiClient::DeployStacks(const std::vector<std::string>& labels, 
                                          const std::string& account,
                                          const std::string& password,
                                          int stop) {
    DeployResponse result;
    json requestBody;
    requestBody["stackLabels"] = labels;
    requestBody["account"] = account;
    requestBody["password"] = password;
        requestBody["stop"] = stop;

    auto res = m_client.Post(m_deployEndpoint.c_str(),
                             requestBody.dump(),
                             "application/json");

    if (res && res->status == 200) {
        try {
            result = ParseDeployResponse(res->body);
            spdlog::info("部署业务链路完成 - 成功: {}, 失败: {}", 
                         result.successStackInfos.size(), result.failureStackInfos.size());
        } catch (const std::exception& e) {
            spdlog::error("解析部署响应失败: {}", e.what());
        }
    } else {
        spdlog::error("部署业务链路失败，状态码: {}", (res ? res->status : -1));
    }

    return result;
}

DeployResponse QywApiClient::UndeployStacks(const std::vector<std::string>& labels) {
    DeployResponse result;
    json requestBody;
    requestBody["stackLabels"] = labels;

    auto res = m_client.Post(m_undeployEndpoint.c_str(),
                             requestBody.dump(),
                             "application/json");

    if (res && res->status == 200) {
        try {
            result = ParseDeployResponse(res->body);
            spdlog::info("停用业务链路完成 - 成功: {}, 失败: {}", 
                         result.successStackInfos.size(), result.failureStackInfos.size());
        } catch (const std::exception& e) {
            spdlog::error("解析停用响应失败: {}", e.what());
        }
    } else {
        spdlog::error("停用业务链路失败，状态码: {}", (res ? res->status : -1));
    }

    return result;
}

bool QywApiClient::SendHeartbeat(const std::string& clientIp) {
    std::string path = m_heartbeatEndpoint + "?clientIp=" + clientIp;
    
    auto res = m_client.Get(path.c_str());
    
    if (res && res->status == 200) {
        try {
            json j = json::parse(res->body);
            
            // 解析标准响应格式：{ "code": 0, "message": "success", "data": "success" }
            if (j.contains("code") && j["code"] == 0) {
                spdlog::info("IP心跳检测发送成功，clientIp: {}", clientIp);
                return true;
            } else {
                spdlog::error("IP心跳检测响应异常，code: {}, message: {}", 
                              j.value("code", -1), j.value("message", ""));
            }
        } catch (const std::exception& e) {
            spdlog::error("解析心跳响应失败: {}", e.what());
        }
    } else {
        spdlog::error("IP心跳检测失败，状态码: {}", (res ? res->status : -1));
    }
    
    return false;
}

bool QywApiClient::ResetStacks() {
    auto res = m_client.Get(m_resetEndpoint.c_str());
    
    if (res && res->status == 200) {
        try {
            json j = json::parse(res->body);
            
            // 解析标准响应格式：{ "code": 0, "message": "success", "data": "success" }
            if (j.contains("code") && j["code"] == 0) {
                spdlog::info("业务链路复位成功");
                return true;
            } else {
                spdlog::error("业务链路复位响应异常，code: {}, message: {}", 
                              j.value("code", -1), j.value("message", ""));
            }
        } catch (const std::exception& e) {
            spdlog::error("解析复位响应失败: {}", e.what());
        }
    } else {
        spdlog::error("业务链路复位失败，状态码: {}", (res ? res->status : -1));
    }
    
    return false;
}

std::vector<BoardInfoResponse> QywApiClient::ParseBoardInfoResponse(const std::string& jsonStr) {
    std::vector<BoardInfoResponse> result;
    
    try {
        json j = json::parse(jsonStr);
        
        // 解析标准响应格式：{ "code": 0, "message": "success", "data": [...] }
        if (j.contains("code") && j["code"] == 0 && j.contains("data")) {
            const auto& data = j["data"];
            
            for (const auto& boardJson : data) {
                BoardInfoResponse boardInfo;
                
                boardInfo.chassisName = boardJson.value("chassisName", "");
                boardInfo.chassisNumber = boardJson.value("chassisNumber", 0);
                boardInfo.boardName = boardJson.value("boardName", ""); 
                boardInfo.boardNumber = boardJson.value("boardNumber", 0);
                boardInfo.boardType = boardJson.value("boardType", 0);
                boardInfo.boardAddress = boardJson.value("boardAddress", "");
                boardInfo.boardStatus = boardJson.value("boardStatus", 0);
                boardInfo.voltage = boardJson.value("voltage", 0.0f);
                boardInfo.current = boardJson.value("current", 0.0f);
                boardInfo.temperature = boardJson.value("temperature", 0.0f);
                
                // 解析风扇信息
                if (boardJson.contains("fanSpeeds")) {
                    for (const auto& fanJson : boardJson["fanSpeeds"]) {
                        FanSpeed fanSpeed;
                        fanSpeed.fanName = fanJson.value("fanName", "");
                        fanSpeed.speed = fanJson.value("speed", 0.0f);
                        boardInfo.fanSpeeds.push_back(fanSpeed);
                    }
                }
                
                // 解析任务信息
                if (boardJson.contains("taskInfos")) {
                    for (const auto& taskJson : boardJson["taskInfos"]) {
                        TaskInfo taskInfo;
                        taskInfo.taskID = taskJson.value("taskID", "");
                        taskInfo.taskStatus = taskJson.value("taskStatus", 0);  // 1-运行中, 2-已完成, 3-异常, 0-其他
                        taskInfo.serviceName = taskJson.value("serviceName", "");
                        taskInfo.serviceUUID = taskJson.value("serviceUUID", "");
                        taskInfo.stackName = taskJson.value("stackName", "");
                        taskInfo.stackUUID = taskJson.value("stackUUID", "");
                        
                        boardInfo.taskInfos.push_back(taskInfo);
                    }
                }
                
                result.push_back(boardInfo);
            }
        }
    } catch (const json::exception& e) {
        spdlog::error("JSON 解析错误: {}", e.what());
    }
    
    return result;
}

std::vector<StackInfoResponse> QywApiClient::ParseStackInfoResponse(const std::string& jsonStr) {
    std::vector<StackInfoResponse> result;
    
    try {
        json j = json::parse(jsonStr);
        
        // 解析标准响应格式
        if (j.contains("code") && j["code"] == 0 && j.contains("data")) {
            const auto& data = j["data"];
            
            for (const auto& stackJson : data) {
                StackInfoResponse stackInfo;
                
                stackInfo.stackName = stackJson.value("stackName", "");
                stackInfo.stackUUID = stackJson.value("stackUUID", "");
                
                // 解析标签信息
                if (stackJson.contains("stackLabelInfos")) {
                    for (const auto& labelJson : stackJson["stackLabelInfos"]) {
                        LabelInfo labelInfo;
                        labelInfo.stackLabelName = labelJson.value("stackLabelName", "");  // 已修正拼写错误
                        labelInfo.stackLabelUUID = labelJson.value("stackLabelUUID", "");  // 已修正拼写错误
                        stackInfo.stackLabelInfos.push_back(labelInfo);
                    }
                }

                stackInfo.stackDeployStatus = stackJson.value("stackDeployStatus", 0);  // 已修正拼写错误
                stackInfo.stackRunningStatus = stackJson.value("stackRunningStatus", 0);
                
                // 解析组件信息
                if (stackJson.contains("serviceInfos")) {
                    for (const auto& serviceJson : stackJson["serviceInfos"]) {
                        ServiceInfo serviceInfo;
                        serviceInfo.serviceName = serviceJson.value("serviceName", "");
                        serviceInfo.serviceUUID = serviceJson.value("serviceUUID", "");
                        serviceInfo.serviceStatus = serviceJson.value("serviceStatus", 0);
                        serviceInfo.serviceType = serviceJson.value("serviceType", 0);
                        
                        // 解析任务信息
                        if (serviceJson.contains("taskInfos")) {
                            for (const auto& taskJson : serviceJson["taskInfos"]) {
                                ServiceTaskInfo taskInfo;
                                taskInfo.taskID = taskJson.value("taskID", "");
                                taskInfo.taskStatus = taskJson.value("taskStatus", 0);  // 1-运行中, 2-已完成, 3-异常, 0-其他
                                taskInfo.cpuCores = taskJson.value("cpuCores", 0.0f);
                                taskInfo.cpuUsed = taskJson.value("cpuUsed", 0.0f);
                                taskInfo.cpuUsage = taskJson.value("cpuUsage", 0.0f);
                                taskInfo.memorySize = taskJson.value("memorySize", 0.0f);
                                taskInfo.memoryUsed = taskJson.value("memoryUsed", 0.0f);
                                taskInfo.memoryUsage = taskJson.value("memoryUsage", 0.0f);
                                taskInfo.netReceive = taskJson.value("netReceive", 0.0f);
                                taskInfo.netSent = taskJson.value("netSent", 0.0f);
                                taskInfo.gpuMemUsed = taskJson.value("gpuMemUsed", 0.0f);
                                taskInfo.chassisName = taskJson.value("chassisName", "");
                                taskInfo.chassisNumber = taskJson.value("chassisNumber", 0);
                                taskInfo.boardName = taskJson.value("boardName", "");  // 已修正拼写错误
                                taskInfo.boardNumber = taskJson.value("boardNumber", 0);
                                taskInfo.boardAddress = taskJson.value("boardAddress", "");
                                
                                serviceInfo.taskInfos.push_back(taskInfo);
                            }
                        }
                        
                        stackInfo.serviceInfos.push_back(serviceInfo);
                    }
                }
                
                result.push_back(stackInfo);
            }
        }
    } catch (const json::exception& e) {
        spdlog::error("JSON 解析错误: {}", e.what());
    }
    
    return result;
}

DeployResponse QywApiClient::ParseDeployResponse(const std::string& jsonStr) {
    DeployResponse result;

    try {
        json j = json::parse(jsonStr);

        // 解析标准响应格式：{ "code": 0, "message": "success", "data": [...] }
        if (j.contains("code") && j["code"] == 0 && j.contains("data")) {
            const auto& data = j["data"];

            // data 是一个数组，但根据 API 规范应该只有一个元素
            if (!data.empty()) {
                const auto& deployResult = data[0];

                // 解析成功的业务链路信息
                if (deployResult.contains("successStackInfos")) {
                    for (const auto& stackJson : deployResult["successStackInfos"]) {
                        StackOperationInfo stackInfo;
                        stackInfo.stackName = stackJson.value("stackName", "");
                        stackInfo.stackUUID = stackJson.value("stackUUID", "");
                        stackInfo.message = stackJson.value("message", "");
                        result.successStackInfos.push_back(stackInfo);
                    }
                }

                // 解析失败的业务链路信息
                if (deployResult.contains("failureStackInfos")) {
                    for (const auto& stackJson : deployResult["failureStackInfos"]) {
                        StackOperationInfo stackInfo;
                        stackInfo.stackName = stackJson.value("stackName", "");
                        stackInfo.stackUUID = stackJson.value("stackUUID", "");
                        stackInfo.message = stackJson.value("message", "");
                        result.failureStackInfos.push_back(stackInfo);
                    }
                }
            }
        }
    } catch (const json::exception& e) {
        spdlog::error("JSON 解析错误: {}", e.what());
    }

    return result;
}

}