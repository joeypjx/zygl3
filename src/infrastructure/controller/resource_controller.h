#ifndef RESOURCE_CONTROLLER_H
#define RESOURCE_CONTROLLER_H

#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <cstdio>

class ResourceController {
public:
    using BinaryData = std::vector<uint8_t>;
    enum class OperationResult {
        SUCCESS,
        PARTIAL_SUCCESS,
        NETWORK_ERROR,
        TIMEOUT_ERROR,
        INVALID_RESPONSE,
        UNKNOWN_ERROR
    };

    enum class SlotStatus {
        NO_OPERATION_OR_SUCCESS = 0,    // 不操作或操作成功
        REQUEST_OPERATION_OR_FAILED = 1 // 请求操作或操作失败
    };

    struct SlotResult {
        int slot_number;     // 槽位号 (1-12)
        SlotStatus status;   // 槽位状态
    };

    struct OperationResponse {
        OperationResult result;
        std::string message;
        std::vector<SlotResult> slot_results; // 各槽位的操作结果
        BinaryData raw_response;
    };

    ResourceController();
    ~ResourceController();

    // 仅保留三个对外操作（支持单/多槽位，单槽位请传含一个元素的vector）
    OperationResponse resetBoard(const std::string& target_ip,
                                 const std::vector<int>& slot_numbers,
                                 uint32_t req_id = 0);

    OperationResponse powerOffChassisBoards(const std::string& target_ip,
                                           const std::vector<int>& slot_numbers,
                                           uint32_t req_id = 0);    
    
    OperationResponse powerOnChassisBoards(const std::string& target_ip,
                                          const std::vector<int>& slot_numbers,
                                          uint32_t req_id = 0);

    /**
     * @brief 自检板卡IP地址检查连通性
     * @param ipAddress 板卡IP地址
     * @return true表示ping通，false表示ping不通
     */
    static bool SelfcheckBoard(const std::string& ipAddress);

    // 工具方法（原先来自 TcpClient）
    static std::string binaryToString(const BinaryData& data) {
        return std::string(data.begin(), data.end());
    }

    static BinaryData stringToBinary(const std::string& str) {
        return BinaryData(str.begin(), str.end());
    }

    static std::string binaryToHex(const BinaryData& data) {
        std::string hex_string;
        hex_string.reserve(data.size() * 2);
        for (uint8_t byte : data) {
            char hex[3];
            std::snprintf(hex, sizeof(hex), "%02x", byte);
            hex_string += hex;
        }
        return hex_string;
    }

private:
    // 内部协议结构，仅类内使用
    struct OperationModel {
        char m_strFlag[8];
        char m_strIp[16];
        char m_cmd[8];
        char m_slot[16];
        uint32_t m_reqId;
    };
    // 固定参数：发送到 33000，本地监听 33001，超时默认 10 秒，标识符 "ETHSWB"
    int server_port_ = 33000;
    int receive_port_ = 33001;
    int timeout_seconds_ = 10;
    std::string operation_flag_ = "ETHSWB";

    // 内部方法：执行操作
    OperationResponse executeOperation(const std::string& cmd,
                                     const std::string& target_ip,
                                     const std::vector<int>& slot_numbers,
                                     uint32_t req_id);

    // 内部方法：构建操作模型
    OperationModel buildOperationModel(const std::string& cmd,
                                      const std::string& target_ip,
                                      const std::vector<int>& slot_numbers,
                                      uint32_t req_id) const;

    // 内部方法：解析响应
    OperationResult parseResponse(const BinaryData& response, 
                                  const std::vector<int>& slot_numbers,
                                 std::vector<SlotResult>& slot_results,
                                 std::string& message) const;
};

#endif // RESOURCE_CONTROLLER_H

