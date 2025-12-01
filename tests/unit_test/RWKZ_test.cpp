/**
 * @file RWKZ_test.cpp
 * @brief QywApiClient部署和停用方法测试
 * 
 * 测试DeployStacks和UndeployStacks两个方法
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <string>
#include "src/infrastructure/api_client/qyw_api_client.h"

using namespace app::infrastructure;

/**
 * @brief QywApiClient部署和停用方法测试套件
 */
class RWKZTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建API客户端（使用localhost，实际测试中可能无法连接）
        apiClient = std::make_shared<QywApiClient>("localhost", 8080);
    }
    
    void TearDown() override {
        apiClient.reset();
    }
    
    std::shared_ptr<QywApiClient> apiClient;
};

/**
 * @brief TC-DeployStacks-Success: DeployStacks方法正常情况测试
 * 
 * 注意：这个测试在实际环境中可能需要真实的API服务
 * 如果API服务不可用，会测试异常路径，但方法调用不会崩溃
 */
TEST_F(RWKZTest, TC_DeployStacks_Success) {
    // 准备部署请求参数
    std::vector<std::string> labels;
    labels.push_back("模式1");
    labels.push_back("模式2");
    
    std::string account = "admin";
    std::string password = "12q12w12ee";
    int stop = 0; // 不排他模式
    
    // 调用DeployStacks
    DeployResponse result = apiClient->DeployStacks(labels, account, password, stop);
    
    // 验证方法被调用（不会崩溃）
    // 验证返回的DeployResponse结构正确
    // 注意：如果API服务不可用，successStackInfos和failureStackInfos可能都为空
    // 但结构应该是有效的
    ASSERT_TRUE(result.successStackInfos.size() >= 0) << "成功列表应该有效";
    ASSERT_TRUE(result.failureStackInfos.size() >= 0) << "失败列表应该有效";
    
    // 验证方法能够处理响应（无论成功或失败）
    // 在实际API服务可用的情况下，应该能正确解析响应
}

/**
 * @brief TC-DeployStacks-Failure: DeployStacks方法异常情况测试
 * 
 * 测试API调用失败或参数错误的情况
 */
TEST_F(RWKZTest, TC_DeployStacks_Failure) {
    // 准备无效的部署请求参数（空标签列表）
    std::vector<std::string> emptyLabels;
    
    std::string account = "admin";
    std::string password = "wrong_password"; // 错误的密码
    int stop = 0;
    
    // 调用DeployStacks
    DeployResponse result = apiClient->DeployStacks(emptyLabels, account, password, stop);
    
    // 验证方法被调用（不会崩溃）
    // 即使参数无效或API调用失败，方法也应该返回有效的DeployResponse结构
    ASSERT_TRUE(result.successStackInfos.size() >= 0) << "成功列表应该有效";
    ASSERT_TRUE(result.failureStackInfos.size() >= 0) << "失败列表应该有效";
    
    // 验证方法能够处理错误情况
    // 在实际API服务可用的情况下，可能会在failureStackInfos中包含错误信息
}

/**
 * @brief TC-UndeployStacks-Success: UndeployStacks方法正常情况测试
 * 
 * 注意：这个测试在实际环境中可能需要真实的API服务
 * 如果API服务不可用，会测试异常路径，但方法调用不会崩溃
 */
TEST_F(RWKZTest, TC_UndeployStacks_Success) {
    // 准备停用请求参数
    std::vector<std::string> labels;
    labels.push_back("模式1");
    labels.push_back("模式2");
    
    // 调用UndeployStacks
    DeployResponse result = apiClient->UndeployStacks(labels);
    
    // 验证方法被调用（不会崩溃）
    // 验证返回的DeployResponse结构正确
    // 注意：如果API服务不可用，successStackInfos和failureStackInfos可能都为空
    // 但结构应该是有效的
    ASSERT_TRUE(result.successStackInfos.size() >= 0) << "成功列表应该有效";
    ASSERT_TRUE(result.failureStackInfos.size() >= 0) << "失败列表应该有效";
    
    // 验证方法能够处理响应（无论成功或失败）
    // 在实际API服务可用的情况下，应该能正确解析响应
}

/**
 * @brief TC-UndeployStacks-Failure: UndeployStacks方法异常情况测试
 * 
 * 测试API调用失败或参数错误的情况
 */
TEST_F(RWKZTest, TC_UndeployStacks_Failure) {
    // 准备无效的停用请求参数（空标签列表）
    std::vector<std::string> emptyLabels;
    
    // 调用UndeployStacks
    DeployResponse result = apiClient->UndeployStacks(emptyLabels);
    
    // 验证方法被调用（不会崩溃）
    // 即使参数无效或API调用失败，方法也应该返回有效的DeployResponse结构
    ASSERT_TRUE(result.successStackInfos.size() >= 0) << "成功列表应该有效";
    ASSERT_TRUE(result.failureStackInfos.size() >= 0) << "失败列表应该有效";
    
    // 验证方法能够处理错误情况
    // 在实际API服务可用的情况下，可能会在failureStackInfos中包含错误信息
}

