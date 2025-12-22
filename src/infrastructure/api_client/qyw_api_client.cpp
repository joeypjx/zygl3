#include "qyw_api_client.h"
#include "nlohmann-json/json.hpp"
#include <spdlog/spdlog.h>

using nlohmann::json;

namespace app::infrastructure {

/**
 * @brief 构造函数
 * @param baseUrl API服务器地址
 * @param port API服务器端口
 */
QywApiClient::QywApiClient(const std::string& baseUrl, int port)
    : m_baseUrl(baseUrl), m_port(port), m_client(baseUrl.c_str(), port) {
}

/**
 * @brief 设置API端点路径
 * @param name 端点名称（boardinfo, stackinfo, deploy, undeploy, heartbeat, reset）
 * @param path 端点路径
 */
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

/**
 * @brief 获取所有板卡信息和状态
 * @return 板卡信息列表，如果获取失败则返回空列表
 * @note 使用GET方法调用板卡信息接口
 */
std::vector<BoardInfoResponse> QywApiClient::GetBoardInfo() {
    std::vector<BoardInfoResponse> result;
    
    // 发送GET请求获取板卡信息
    auto res = m_client.Get(m_boardinfoEndpoint.c_str());
    
    if (res && res->status == 200) {
        try {
            // 解析JSON响应并转换为BoardInfoResponse对象列表
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

/**
 * @brief 获取所有业务链路详情
 * @param success 输出参数，指示API调用是否成功（包括网络请求和JSON解析）
 * @return 业务链路信息列表，如果获取失败则返回空列表
 * @note 使用POST方法调用业务链路详情接口，请求体可以为空
 */
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
            // 解析JSON响应并转换为StackInfoResponse对象列表
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

/**
 * @brief 批量启用业务链路
 * @param labels 业务链路标签列表（例如：["工作模式1", "工作模式2"]）
 * @param account 用户账号
 * @param password 用户密码
 * @param stop 是否排他模式（0-不排他，1-先停止其他业务再启动本服务）
 * @return 部署结果，包含成功和失败的业务链路详情
 * @note 使用POST方法调用部署接口，支持批量操作
 */
DeployResponse QywApiClient::DeployStacks(const std::vector<std::string>& labels, 
                                          const std::string& account,
                                          const std::string& password,
                                          int stop) {
    DeployResponse result;
    
    // 构建POST请求体
    json requestBody;
    requestBody["stackLabels"] = labels;
    requestBody["account"] = account;
    requestBody["password"] = password;
    requestBody["stop"] = stop;

    // 发送POST请求
    auto res = m_client.Post(m_deployEndpoint.c_str(),
                             requestBody.dump(),
                             "application/json");

    if (res && res->status == 200) {
        try {
            // 解析响应，提取成功和失败的业务链路信息
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

/**
 * @brief 批量停用业务链路
 * @param labels 业务链路标签列表
 * @return 停用结果，包含成功和失败的业务链路详情
 * @note 使用POST方法调用停用接口，支持批量操作
 */
DeployResponse QywApiClient::UndeployStacks(const std::vector<std::string>& labels) {
    DeployResponse result;
    
    // 构建POST请求体
    json requestBody;
    requestBody["stackLabels"] = labels;

    // 发送POST请求
    auto res = m_client.Post(m_undeployEndpoint.c_str(),
                             requestBody.dump(),
                             "application/json");

    if (res && res->status == 200) {
        try {
            // 解析响应，提取成功和失败的业务链路信息
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

/**
 * @brief 发送IP心跳检测
 * @param ip 客户端IP地址
 * @param port 客户端端口号
 * @return 是否发送成功（HTTP状态码200且响应code为0）
 * @note 新版接口使用GET方法，路径格式：/api/v1/external/qyw/config?ip=xxx&port=xxx
 */
bool QywApiClient::SendHeartbeat(const std::string& ip, const std::string& port) {
    // 新版接口：/api/v1/external/qyw/config?ip=xxx&port=xxx
    std::string path = m_heartbeatEndpoint + "?ip=" + ip + "&port=" + port;
    
    // 发送GET请求
    auto res = m_client.Get(path.c_str());
    
    if (res && res->status == 200) {
        try {
            json j = json::parse(res->body);
            
            // 解析标准响应格式：{ "code": 0, "message": "success", "data": "success" }
            // code=0 表示成功，其他值表示失败
            if (j.contains("code") && j["code"] == 0) {
                spdlog::info("IP心跳检测发送成功，ip: {}, port: {}", ip, port);
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

/**
 * @brief 业务链路复位接口（停止当前所有业务链路）
 * @return 是否操作成功（HTTP状态码200且响应code为0）
 * @note 使用GET方法调用复位接口，用于停止所有正在运行的业务链路
 */
bool QywApiClient::ResetStacks() {
    // 发送GET请求
    auto res = m_client.Get(m_resetEndpoint.c_str());
    
    if (res && res->status == 200) {
        try {
            json j = json::parse(res->body);
            
            // 解析标准响应格式：{ "code": 0, "message": "success", "data": "success" }
            // code=0 表示成功，其他值表示失败
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

/**
 * @brief 解析板卡信息的JSON响应
 * @param jsonStr JSON字符串
 * @return 板卡信息列表
 * @note 解析标准响应格式：{ "code": 0, "message": "success", "data": [...] }
 *       新版API中电压和电流分为12V/33V和12A/33A两路
 */
std::vector<BoardInfoResponse> QywApiClient::ParseBoardInfoResponse(const std::string& jsonStr) {
    std::vector<BoardInfoResponse> result;
    
    try {
        json j = json::parse(jsonStr);
        
        // 解析标准响应格式：{ "code": 0, "message": "success", "data": [...] }
        // code=0 表示成功，data字段包含板卡信息数组
        if (j.contains("code") && j["code"] == 0 && j.contains("data")) {
            const auto& data = j["data"];
            
            // 遍历每个板卡信息
            for (const auto& boardJson : data) {
                BoardInfoResponse boardInfo;
                
                // 解析板卡基本信息
                boardInfo.chassisName = boardJson.value("chassisName", "");
                boardInfo.chassisNumber = boardJson.value("chassisNumber", 0);
                boardInfo.boardName = boardJson.value("boardName", ""); 
                boardInfo.boardNumber = boardJson.value("boardNumber", 0);
                boardInfo.boardType = boardJson.value("boardType", 0);
                boardInfo.boardAddress = boardJson.value("boardAddress", "");
                boardInfo.boardStatus = boardJson.value("boardStatus", 0);  // 0-正常, 1-异常, 2-不在位
                
                // 解析电压和电流（新版分为12V/33V和12A/33A）
                boardInfo.voltage12V = boardJson.value("voltage12V", 0.0f);
                boardInfo.voltage33V = boardJson.value("voltage33V", 0.0f);
                boardInfo.current12A = boardJson.value("current12A", 0.0f);
                boardInfo.current33A = boardJson.value("current33A", 0.0f);
                boardInfo.temperature = boardJson.value("temperature", 0.0f);
                
                // 解析风扇信息（可选字段）
                if (boardJson.contains("fanSpeeds")) {
                    for (const auto& fanJson : boardJson["fanSpeeds"]) {
                        FanSpeed fanSpeed;
                        fanSpeed.fanName = fanJson.value("fanName", "");
                        fanSpeed.speed = fanJson.value("speed", 0.0f);
                        boardInfo.fanSpeeds.push_back(fanSpeed);
                    }
                }
                
                // 解析任务信息（可选字段）
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

/**
 * @brief 解析业务链路信息的JSON响应
 * @param jsonStr JSON字符串
 * @return 业务链路信息列表
 * @note 解析标准响应格式：{ "code": 0, "message": "success", "data": [...] }
 *       新版API中stackLabelInfos改为字符串数组（之前是对象数组）
 */
std::vector<StackInfoResponse> QywApiClient::ParseStackInfoResponse(const std::string& jsonStr) {
    std::vector<StackInfoResponse> result;
    
    try {
        json j = json::parse(jsonStr);
        
        // 解析标准响应格式：{ "code": 0, "message": "success", "data": [...] }
        // code=0 表示成功，data字段包含业务链路信息数组
        if (j.contains("code") && j["code"] == 0 && j.contains("data")) {
            const auto& data = j["data"];
            
            // 遍历每个业务链路信息
            for (const auto& stackJson : data) {
                StackInfoResponse stackInfo;
                
                // 解析业务链路基本信息
                stackInfo.stackName = stackJson.value("stackName", "");
                stackInfo.stackUUID = stackJson.value("stackUUID", "");
                
                // 解析标签信息（新版为字符串数组，直接存储标签UUID或名称）
                if (stackJson.contains("stackLabelInfos")) {
                    for (const auto& labelStr : stackJson["stackLabelInfos"]) {
                        if (labelStr.is_string()) {
                            stackInfo.stackLabelInfos.push_back(labelStr.get<std::string>());
                        }
                    }
                }

                // 解析部署状态和运行状态
                stackInfo.stackDeployStatus = stackJson.value("stackDeployStatus", 0);  // 0-未部署, 1-已部署
                stackInfo.stackRunningStatus = stackJson.value("stackRunningStatus", 0);  // 1-正常运行, 2-异常运行, 3-启用中
                
                // 解析组件信息（可选字段）
                if (stackJson.contains("serviceInfos")) {
                    for (const auto& serviceJson : stackJson["serviceInfos"]) {
                        ServiceInfo serviceInfo;
                        
                        // 解析组件基本信息
                        serviceInfo.serviceName = serviceJson.value("serviceName", "");
                        serviceInfo.serviceUUID = serviceJson.value("serviceUUID", "");
                        serviceInfo.serviceStatus = serviceJson.value("serviceStatus", 0);  // 0-已停用, 1-已启用, 2-运行正常, 3-运行异常
                        serviceInfo.serviceType = serviceJson.value("serviceType", 0);  // 0-普通组件, 1-普通链路引用的公共组件, 2-公共链路自有组件
                        
                        // 解析任务信息（可选字段）
                        if (serviceJson.contains("taskInfos")) {
                            for (const auto& taskJson : serviceJson["taskInfos"]) {
                                ServiceTaskInfo taskInfo;
                                
                                // 解析任务基本信息
                                taskInfo.taskID = taskJson.value("taskID", "");
                                taskInfo.taskStatus = taskJson.value("taskStatus", 0);  // 1-运行中, 2-已完成, 3-异常, 0-其他
                                
                                // 解析CPU资源使用情况
                                taskInfo.cpuCores = taskJson.value("cpuCores", 0.0f);
                                taskInfo.cpuUsed = taskJson.value("cpuUsed", 0.0f);
                                taskInfo.cpuUsage = taskJson.value("cpuUsage", 0.0f);
                                
                                // 解析内存资源使用情况
                                taskInfo.memorySize = taskJson.value("memorySize", 0.0f);
                                taskInfo.memoryUsed = taskJson.value("memoryUsed", 0.0f);
                                taskInfo.memoryUsage = taskJson.value("memoryUsage", 0.0f);
                                
                                // 解析网络资源使用情况（新版增加了单位字段）
                                taskInfo.netReceive = taskJson.value("netReceive", 0.0f);
                                taskInfo.netReceiveUnit = taskJson.value("netReceiveUnit", "");
                                taskInfo.netSent = taskJson.value("netSent", 0.0f);
                                taskInfo.netSentUnit = taskJson.value("netSentUnit", "");
                                
                                // 解析GPU资源使用情况
                                taskInfo.gpuMemUsed = taskJson.value("gpuMemUsed", 0.0f);
                                
                                // 解析任务运行位置信息
                                taskInfo.chassisName = taskJson.value("chassisName", "");
                                taskInfo.chassisNumber = taskJson.value("chassisNumber", 0);
                                taskInfo.boardName = taskJson.value("boardName", "");
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

/**
 * @brief 解析部署/停用操作的JSON响应
 * @param jsonStr JSON字符串
 * @return 部署/停用结果，包含成功和失败的业务链路详情
 * @note 解析标准响应格式：{ "code": 0, "message": "success", "data": [{ "successStackInfos": [...], "failureStackInfos": [...] }] }
 *       响应中包含成功和失败的业务链路列表，用于批量操作的详细结果反馈
 */
DeployResponse QywApiClient::ParseDeployResponse(const std::string& jsonStr) {
    DeployResponse result;

    try {
        json j = json::parse(jsonStr);

        // 解析标准响应格式：{ "code": 0, "message": "success", "data": [...] }
        // code=0 表示成功，data字段包含操作结果数组
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