/**
 * @file ZYKZ_test.cpp
 * @brief ResourceController::resetBoard方法测试
 * 
 * 测试resetBoard方法（正常和异常情况）
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <string>
#include "src/infrastructure/controller/resource_controller.h"

using namespace std;

/**
 * @brief ResourceController::resetBoard方法测试套件
 */
class ZYKZTest : public ::testing::Test {
protected:
    void SetUp() override {
        controller = std::make_unique<ResourceController>();
    }
    
    void TearDown() override {
        controller.reset();
    }
    
    std::unique_ptr<ResourceController> controller;
};

/**
 * @brief TC-resetBoard-Success: resetBoard方法正常情况测试
 * 
 * 测试使用有效的IP地址和槽位号的情况
 * 注意：由于需要真实的TCP连接，如果目标IP不可达，会返回网络错误或超时错误
 * 这里主要验证方法能够正常执行，不会崩溃，并且能够正确处理响应
 */
TEST_F(ZYKZTest, TC_resetBoard_Success) {
    // 使用有效的IP地址和槽位号
    std::string targetIp = "127.0.0.1";  // 本地回环地址
    std::vector<int> slotNumbers = {1, 2, 3};  // 测试3个槽位
    uint32_t reqId = 12345;
    
    // 调用resetBoard方法
    auto response = controller->resetBoard(targetIp, slotNumbers, reqId);
    
    // 验证响应结构有效
    ASSERT_NE(nullptr, &response) << "响应对象应该有效";
    
    // 验证响应消息不为空（即使操作失败，也应该有消息）
    ASSERT_FALSE(response.message.empty()) 
        << "响应消息不应该为空";
    
    // 加强验证：由于127.0.0.1:33000没有服务器监听，必须返回网络错误或超时错误
    // 不应该返回SUCCESS或PARTIAL_SUCCESS（因为没有真实的服务器响应）
    ASSERT_TRUE(
        response.result == ResourceController::OperationResult::NETWORK_ERROR ||
        response.result == ResourceController::OperationResult::TIMEOUT_ERROR ||
        response.result == ResourceController::OperationResult::INVALID_RESPONSE
    ) << "当目标IP不可达时，应该返回NETWORK_ERROR、TIMEOUT_ERROR或INVALID_RESPONSE，而不是SUCCESS";
    
    // 加强验证：如果连接成功但接收到的响应为空，必须返回INVALID_RESPONSE而不是SUCCESS
    // 这要求源代码在parseResponse中正确处理空响应的情况
    if (response.result == ResourceController::OperationResult::INVALID_RESPONSE) {
        // 如果返回INVALID_RESPONSE，错误消息必须包含"Empty response received"
        ASSERT_NE(std::string::npos, response.message.find("Empty response received"))
            << "当响应为空时，必须返回INVALID_RESPONSE，且错误消息必须包含'Empty response received'";
    }
    
    // 加强验证：错误消息必须包含有用的信息
    if (response.result == ResourceController::OperationResult::NETWORK_ERROR) {
        ASSERT_TRUE(
            response.message.find("Failed") != std::string::npos ||
            response.message.find("Connect") != std::string::npos ||
            response.message.find("Invalid") != std::string::npos ||
            response.message.find("error") != std::string::npos ||
            response.message.find("Error") != std::string::npos
        ) << "NETWORK_ERROR的错误消息应该包含有用的错误信息";
    } else if (response.result == ResourceController::OperationResult::TIMEOUT_ERROR) {
        ASSERT_TRUE(
            response.message.find("timeout") != std::string::npos ||
            response.message.find("Timeout") != std::string::npos ||
            response.message.find("超时") != std::string::npos
        ) << "TIMEOUT_ERROR的错误消息应该包含超时相关信息";
    }
    
    // 加强验证：当连接失败时，slot_results应该为空（因为没有收到有效响应）
    if (response.result == ResourceController::OperationResult::NETWORK_ERROR ||
        response.result == ResourceController::OperationResult::TIMEOUT_ERROR ||
        response.result == ResourceController::OperationResult::INVALID_RESPONSE) {
        ASSERT_EQ(0, response.slot_results.size()) 
            << "当操作失败时，slot_results应该为空";
    }
    
    // 加强验证：raw_response字段应该反映实际接收到的数据
    // 如果连接失败，raw_response应该为空或包含错误信息
    if (response.result == ResourceController::OperationResult::INVALID_RESPONSE) {
        ASSERT_TRUE(response.raw_response.empty()) 
            << "当返回INVALID_RESPONSE时，raw_response应该为空（表示没有收到有效响应）";
    }
    
    // 加强验证：如果响应数据大小不足（小于预期的OperationModel大小，约48字节），必须返回INVALID_RESPONSE而不是SUCCESS
    // 这要求源代码在parseResponse中正确处理响应数据大小不足的情况
    // 当前源代码在响应大小不足时默认返回SUCCESS，这是不正确的
    // 测试要求：如果响应不为空但大小不足，必须返回INVALID_RESPONSE
    // 注意：OperationModel是私有成员，我们使用估算的大小（约48字节：8+16+8+16+4）
    const size_t estimated_operation_model_size = 52; // 估算的OperationModel大小
    if (!response.raw_response.empty() && 
        response.raw_response.size() < estimated_operation_model_size) {
        ASSERT_EQ(ResourceController::OperationResult::INVALID_RESPONSE, response.result)
            << "当响应数据大小不足（小于OperationModel大小）时，必须返回INVALID_RESPONSE而不是SUCCESS。"
               "请在ResourceController::parseResponse中修复：响应大小不足时应返回INVALID_RESPONSE";
        ASSERT_TRUE(response.message.find("Empty") != std::string::npos ||
                    response.message.find("invalid") != std::string::npos ||
                    response.message.find("Invalid") != std::string::npos ||
                    response.message.find("size") != std::string::npos)
            << "当响应数据大小不足时，错误消息应该包含相关信息";
    }
    
    // 如果收到有效响应（理论上不应该发生，但保留此检查），验证槽位结果
    if (response.result == ResourceController::OperationResult::SUCCESS ||
        response.result == ResourceController::OperationResult::PARTIAL_SUCCESS) {
        ASSERT_EQ(slotNumbers.size(), response.slot_results.size()) 
            << "成功响应时，槽位结果数量应该与请求的槽位数量一致";
        
        // 验证每个槽位结果都有有效的槽位号
        for (size_t i = 0; i < response.slot_results.size(); ++i) {
            const auto& slotResult = response.slot_results[i];
            ASSERT_GE(slotResult.slot_number, 1) 
                << "槽位号应该大于等于1";
            ASSERT_LE(slotResult.slot_number, static_cast<int>(slotNumbers.size())) 
                << "槽位号应该在有效范围内";
        }
    }
}

/**
 * @brief TC-resetBoard-Failure: resetBoard方法异常情况测试
 * 
 * 测试使用无效的IP地址格式的情况，应该返回NETWORK_ERROR
 */
TEST_F(ZYKZTest, TC_resetBoard_Failure) {
    // 使用无效的IP地址格式（不是有效的IPv4地址格式）
    std::string invalidIp = "invalid.ip.address";  // 无效的IP地址格式
    std::vector<int> slotNumbers = {1};
    uint32_t reqId = 67890;
    
    // 调用resetBoard方法
    auto response = controller->resetBoard(invalidIp, slotNumbers, reqId);
    
    // 验证响应结构有效
    ASSERT_NE(nullptr, &response) << "响应对象应该有效";
    
    // 验证应该返回NETWORK_ERROR（因为IP地址格式无效，inet_pton会失败）
    ASSERT_EQ(ResourceController::OperationResult::NETWORK_ERROR, response.result)
        << "无效IP地址格式应该返回NETWORK_ERROR";
    
    // 验证错误消息不为空
    ASSERT_FALSE(response.message.empty()) 
        << "错误消息不应该为空";
    
    // 验证错误消息应该包含"Invalid target IP"
    ASSERT_NE(response.message.find("Invalid target IP"), std::string::npos) 
        << "错误消息应该包含'Invalid target IP'";
}

