/**
 * @file mock_api_client.h
 * @brief Mock API客户端，用于单元测试
 * 
 * 注意：由于QywApiClient的方法不是virtual，这里使用组合方式
 * 创建一个测试包装类，内部使用实际的QywApiClient但拦截调用
 * 或者创建一个完全独立的测试实现
 * 
 * 实际实现中，我们创建一个测试专用的API客户端类
 */

#pragma once

#include "../../infrastructure/api_client/qyw_api_client.h"
#include <vector>
#include <string>
#include <memory>

namespace app::infrastructure::test {

/**
 * @brief Mock API客户端（测试实现）
 * 
 * 由于QywApiClient的方法不是virtual，我们创建一个独立的测试类
 * 它提供与QywApiClient相同的接口，但使用Mock数据
 */
class MockQywApiClient {
public:
    MockQywApiClient() {
        Reset();
    }
    
    // 实现与QywApiClient相同的接口
    std::vector<BoardInfoResponse> GetBoardInfo() {
        m_getBoardInfoCalled = true;
        if (m_shouldFail) {
            return std::vector<BoardInfoResponse>();
        }
        return m_mockBoardInfo;
    }
    
    std::vector<StackInfoResponse> GetStackInfo() {
        m_getStackInfoCalled = true;
        if (m_shouldFail) {
            return std::vector<StackInfoResponse>();
        }
        return m_mockStackInfo;
    }
    
    bool SendHeartbeat(const std::string& ip, const std::string& port) {
        m_sendHeartbeatCalled = true;
        m_lastHeartbeatIp = ip;
        m_lastHeartbeatPort = port;
        return !m_shouldFail;
    }
    
    DeployResponse DeployStacks(const std::vector<std::string>& labels, 
                                const std::string& account,
                                const std::string& password,
                                int stop = 0) {
        m_deployStacksCalled = true;
        m_lastDeployLabels = labels;
        m_lastDeployAccount = account;
        m_lastDeployPassword = password;
        return m_mockDeployResponse;
    }
    
    DeployResponse UndeployStacks(const std::vector<std::string>& labels) {
        m_undeployStacksCalled = true;
        m_lastUndeployLabels = labels;
        return m_mockDeployResponse;
    }
    
    bool ResetStacks() {
        m_resetStacksCalled = true;
        return !m_shouldFail;
    }
    
    // SetEndpoint方法（为了兼容）
    void SetEndpoint(const std::string& name, const std::string& path) {
        // 测试中不需要实际设置端点
        (void)name;  // 避免未使用参数警告
        (void)path;
    }
    
    // 类型转换：转换为QywApiClient*（用于测试环境）
    // 注意：这只在测试环境中使用，因为实际类型不匹配
    // 更好的做法是将QywApiClient改为接口，但在现有代码中我们使用这个workaround
    operator std::shared_ptr<QywApiClient>() {
        // 由于无法真正继承并override，我们创建一个包装
        // 实际测试中可能需要修改DataCollectorService使用接口
        // 这里提供一个workaround：返回一个指向自身的智能指针（但类型不匹配）
        // 更好的方案：创建测试专用的DataCollectorService或修改架构
        return nullptr;  // 这个需要特殊处理
    }
    
    // 测试辅助方法
    void SetMockBoardInfo(const std::vector<BoardInfoResponse>& data) {
        m_mockBoardInfo = data;
    }
    
    void SetMockStackInfo(const std::vector<StackInfoResponse>& data) {
        m_mockStackInfo = data;
    }
    
    void SetMockDeployResponse(const DeployResponse& response) {
        m_mockDeployResponse = response;
    }
    
    void SetShouldFail(bool fail) {
        m_shouldFail = fail;
    }
    
    // 测试验证方法
    bool GetBoardInfoCalled() const { return m_getBoardInfoCalled; }
    bool GetStackInfoCalled() const { return m_getStackInfoCalled; }
    bool SendHeartbeatCalled() const { return m_sendHeartbeatCalled; }
    bool DeployStacksCalled() const { return m_deployStacksCalled; }
    bool UndeployStacksCalled() const { return m_undeployStacksCalled; }
    bool ResetStacksCalled() const { return m_resetStacksCalled; }
    
    const std::string& GetLastHeartbeatIp() const { return m_lastHeartbeatIp; }
    const std::string& GetLastHeartbeatPort() const { return m_lastHeartbeatPort; }
    const std::vector<std::string>& GetLastDeployLabels() const { return m_lastDeployLabels; }
    const std::string& GetLastDeployAccount() const { return m_lastDeployAccount; }
    
    void Reset() {
        m_getBoardInfoCalled = false;
        m_getStackInfoCalled = false;
        m_sendHeartbeatCalled = false;
        m_deployStacksCalled = false;
        m_undeployStacksCalled = false;
        m_resetStacksCalled = false;
        m_lastHeartbeatIp.clear();
        m_lastHeartbeatPort.clear();
        m_lastDeployLabels.clear();
    }
    
private:
    std::vector<BoardInfoResponse> m_mockBoardInfo;
    std::vector<StackInfoResponse> m_mockStackInfo;
    DeployResponse m_mockDeployResponse;
    bool m_shouldFail;
    
    // 调用跟踪
    bool m_getBoardInfoCalled;
    bool m_getStackInfoCalled;
    bool m_sendHeartbeatCalled;
    bool m_deployStacksCalled;
    bool m_undeployStacksCalled;
    bool m_resetStacksCalled;
    
    std::string m_lastHeartbeatIp;
    std::string m_lastHeartbeatPort;
    std::vector<std::string> m_lastDeployLabels;
    std::string m_lastDeployAccount;
    std::string m_lastDeployPassword;
};

} // namespace app::infrastructure::test

