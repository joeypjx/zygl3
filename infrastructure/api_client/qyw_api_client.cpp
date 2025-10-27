#include "qyw_api_client.h"
#include "json.hpp"
#include <sstream>
#include <iostream>

using nlohmann::json;

namespace app::infrastructure {

QywApiClient::QywApiClient(const std::string& baseUrl, int port)
    : m_baseUrl(baseUrl), m_port(port), m_client(baseUrl.c_str(), port) {
}

std::vector<BoardInfoResponse> QywApiClient::GetBoardInfo() {
    std::vector<BoardInfoResponse> result;
    
    auto res = m_client.Get("/api/v1/external/qyw/boardinfo");
    
    if (res && res->status == 200) {
        try {
            result = ParseBoardInfoResponse(res->body);
            std::cout << "成功获取板卡信息，共 " << result.size() << " 条" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "解析板卡信息失败: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "获取板卡信息失败，状态码: " << (res ? res->status : -1) << std::endl;
    }
    
    return result;
}

std::vector<StackInfoResponse> QywApiClient::GetStackInfo() {
    std::vector<StackInfoResponse> result;
    
    auto res = m_client.Get("/api/v1/external/qyw/stackinfo");
    
    if (res && res->status == 200) {
        try {
            result = ParseStackInfoResponse(res->body);
            std::cout << "成功获取业务链路信息，共 " << result.size() << " 条" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "解析业务链路信息失败: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "获取业务链路信息失败，状态码: " << (res ? res->status : -1) << std::endl;
    }
    
    return result;
}

DeployResponse QywApiClient::DeployStacks(const std::vector<std::string>& labels) {
    DeployResponse result;
    json requestBody;
    requestBody["stackLabels"] = labels;

    auto res = m_client.Post("/api/v1/external/qyw/deploy",
                             requestBody.dump(),
                             "application/json");

    if (res && res->status == 200) {
        try {
            result = ParseDeployResponse(res->body);
            std::cout << "部署业务链路完成 - 成功: " << result.successStackInfos.size()
                     << ", 失败: " << result.failureStackInfos.size() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "解析部署响应失败: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "部署业务链路失败，状态码: " << (res ? res->status : -1) << std::endl;
    }

    return result;
}

DeployResponse QywApiClient::UndeployStacks(const std::vector<std::string>& labels) {
    DeployResponse result;
    json requestBody;
    requestBody["stackLabels"] = labels;

    auto res = m_client.Post("/api/v1/external/qyw/undeploy",
                             requestBody.dump(),
                             "application/json");

    if (res && res->status == 200) {
        try {
            result = ParseDeployResponse(res->body);
            std::cout << "停用业务链路完成 - 成功: " << result.successStackInfos.size()
                     << ", 失败: " << result.failureStackInfos.size() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "解析停用响应失败: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "停用业务链路失败，状态码: " << (res ? res->status : -1) << std::endl;
    }

    return result;
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
                boardInfo.boardName = boardJson.value("boardName", "");  // 已修正拼写错误
                boardInfo.boardNumber = boardJson.value("boardNumber", 0);
                boardInfo.boardType = boardJson.value("boardType", 0);
                boardInfo.boardAddress = boardJson.value("boardAddress", "");
                boardInfo.boardStatus = boardJson.value("boardStatus", 0);
                
                // 解析任务信息
                if (boardJson.contains("taskInfos")) {
                    for (const auto& taskJson : boardJson["taskInfos"]) {
                        BoardInfoResponse::TaskInfo taskInfo;
                        taskInfo.taskID = taskJson.value("taskID", "");
                        taskInfo.taskStatus = taskJson.value("taskStatus", "");
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
        std::cerr << "JSON 解析错误: " << e.what() << std::endl;
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
                                taskInfo.taskStatus = taskJson.value("taskStatus", "");
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
        std::cerr << "JSON 解析错误: " << e.what() << std::endl;
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
        std::cerr << "JSON 解析错误: " << e.what() << std::endl;
    }

    return result;
}

}