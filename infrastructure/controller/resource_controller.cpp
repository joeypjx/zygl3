#include "resource_controller.h"
#include <iostream>
#include <cstring>
#include <sstream>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

ResourceController::ResourceController() = default;

ResourceController::~ResourceController() = default;

ResourceController::OperationResponse ResourceController::resetBoard(
    const std::string& target_ip,
    const std::vector<int>& slot_numbers,
    uint32_t req_id) {
    return executeOperation("RESET", target_ip, slot_numbers, req_id);
}

ResourceController::OperationResponse ResourceController::powerOffChassisBoards(
    const std::string& target_ip,
    const std::vector<int>& slot_numbers,
    uint32_t req_id) {
    return executeOperation("POWOFF", target_ip, slot_numbers, req_id);
}

ResourceController::OperationResponse ResourceController::powerOnChassisBoards(
    const std::string& target_ip,
    const std::vector<int>& slot_numbers,
    uint32_t req_id) {
    return executeOperation("POWON", target_ip, slot_numbers, req_id);
}

bool ResourceController::SelfcheckBoard(const std::string& ipAddress) {
    // 使用 ping 命令检查连通性
    // ping -c 1 -W 1 IP地址，-c 1表示只ping一次，-W 1表示超时1秒
    std::string pingCommand = "ping -c 1 -W 1 " + ipAddress + " > /dev/null 2>&1";
    int result = std::system(pingCommand.c_str());
    
    // ping 成功返回0，失败返回非0
    return (result == 0);
}

ResourceController::OperationResponse ResourceController::executeOperation(
    const std::string& cmd,
    const std::string& target_ip,
    const std::vector<int>& slot_numbers,
    uint32_t req_id) {
    
    OperationResponse response;

    try {
        // 1) 使用与目标的直连socket进行请求-响应

        // 2) 构建操作模型并发送到目标 33000 端口
        ResourceController::OperationModel op_model = buildOperationModel(cmd, target_ip, slot_numbers, req_id);
        BinaryData binary_data(
            reinterpret_cast<const uint8_t*>(&op_model),
            reinterpret_cast<const uint8_t*>(&op_model) + sizeof(ResourceController::OperationModel)
        );

        std::cout << "Executing chassis operation: " << cmd << " to " << target_ip << ":" << server_port_ << "; waiting response on same connection" << std::endl;
        std::cout << "Sending binary data: " << ResourceController::binaryToHex(binary_data) << std::endl;

        int send_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (send_fd < 0) {
            response.result = OperationResult::NETWORK_ERROR;
            response.message = "Failed to create send socket";
            return response;
        }

        // 设置非阻塞并使用select实现连接超时
        int flags = ::fcntl(send_fd, F_GETFL, 0);
        if (flags >= 0) {
            ::fcntl(send_fd, F_SETFL, flags | O_NONBLOCK);
        }

        sockaddr_in remote{};
        remote.sin_family = AF_INET;
        remote.sin_port = htons(static_cast<uint16_t>(server_port_));
        if (::inet_pton(AF_INET, target_ip.c_str(), &remote.sin_addr) != 1) {
            ::close(send_fd);
            response.result = OperationResult::NETWORK_ERROR;
            response.message = "Invalid target IP";
            return response;
        }

        int rc = ::connect(send_fd, reinterpret_cast<sockaddr*>(&remote), sizeof(remote));
        if (rc < 0 && errno == EINPROGRESS) {
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(send_fd, &wfds);
            timeval ctv{};
            ctv.tv_sec = timeout_seconds_;
            ctv.tv_usec = 0;
            int sel = ::select(send_fd + 1, nullptr, &wfds, nullptr, &ctv);
            if (sel <= 0) {
                ::close(send_fd);
                response.result = (sel == 0) ? OperationResult::TIMEOUT_ERROR : OperationResult::NETWORK_ERROR;
                response.message = (sel == 0) ? "Connect timeout to target" : "Select error on connect";
                return response;
            }
            int so_error = 0;
            socklen_t len = sizeof(so_error);
            if (::getsockopt(send_fd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0 || so_error != 0) {
                ::close(send_fd);
                response.result = OperationResult::NETWORK_ERROR;
                response.message = "Connect failed to target";
                return response;
            }
        } else if (rc < 0) {
            ::close(send_fd);
            response.result = OperationResult::NETWORK_ERROR;
            response.message = "Connect error to target";
            return response;
        }

        // 连接建立后切回阻塞模式，以便 SO_SNDTIMEO / SO_RCVTIMEO 生效
        if (flags >= 0) {
            ::fcntl(send_fd, F_SETFL, flags & ~O_NONBLOCK);
        }

        // 可选：发送超时
        timeval snd_to{};
        snd_to.tv_sec = timeout_seconds_;
        snd_to.tv_usec = 0;
        ::setsockopt(send_fd, SOL_SOCKET, SO_SNDTIMEO, &snd_to, sizeof(snd_to));

        // 发送完整报文
        size_t total_sent = 0;
        const uint8_t* buf = binary_data.data();
        const size_t total_size = binary_data.size();
        while (total_sent < total_size) {
            ssize_t n = ::send(send_fd, reinterpret_cast<const char*>(buf + total_sent), static_cast<int>(total_size - total_sent), 0);
            if (n > 0) {
                total_sent += static_cast<size_t>(n);
            } else if (n < 0 && (errno == EINTR)) {
                continue;
            } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                ::close(send_fd);
                response.result = OperationResult::TIMEOUT_ERROR;
                response.message = "Send timeout to target";
                return response;
            } else {
                ::close(send_fd);
                response.result = OperationResult::NETWORK_ERROR;
                response.message = "Failed to send data to target";
                return response;
            }
        }

        // 3) 在同一连接上接收响应（带超时）
        timeval rcv_to{};
        rcv_to.tv_sec = timeout_seconds_;
        rcv_to.tv_usec = 0;
        ::setsockopt(send_fd, SOL_SOCKET, SO_RCVTIMEO, &rcv_to, sizeof(rcv_to));

        BinaryData tcp_response;
        tcp_response.resize(4096);
        ssize_t n = ::recv(send_fd, reinterpret_cast<char*>(tcp_response.data()), static_cast<int>(tcp_response.size()), 0);
        if (n > 0) {
            tcp_response.resize(static_cast<size_t>(n));
        } else {
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                ::close(send_fd);
                response.result = OperationResult::TIMEOUT_ERROR;
                response.message = "Receive timeout from target";
                return response;
            }
            tcp_response.clear();
        }
        ::close(send_fd);

        response.raw_response = tcp_response;
        std::cout << "Received response (" << tcp_response.size() << " bytes) from " << target_ip << ":" << server_port_ << ": " << ResourceController::binaryToHex(tcp_response) << std::endl;

        // 5) 解析响应
        std::string message;
        response.result = parseResponse(tcp_response, slot_numbers, response.slot_results, message);
        response.message = message;

        if (response.result == OperationResult::SUCCESS || response.result == OperationResult::PARTIAL_SUCCESS) {
            std::cout << "Chassis operation completed: " << message << std::endl;
        } else {
            std::cout << "Chassis operation failed: " << message << std::endl;
        }

    } catch (const std::runtime_error& e) {
        response.result = OperationResult::NETWORK_ERROR;
        response.message = std::string("Network error: ") + e.what();
        std::cout << "Chassis operation network error: " << response.message << std::endl;

    } catch (const std::exception& e) {
        response.result = OperationResult::UNKNOWN_ERROR;
        response.message = std::string("Unknown error: ") + e.what();
        std::cout << "Chassis operation unknown error: " << response.message << std::endl;
    }

    return response;
}

ResourceController::OperationModel ResourceController::buildOperationModel(
    const std::string& cmd,
    const std::string& target_ip,
    const std::vector<int>& slot_numbers,
    uint32_t req_id) const {
    
    ResourceController::OperationModel model;
    
    // 清零结构体
    memset(&model, 0, sizeof(ResourceController::OperationModel));

    // 填充字段，确保不超出数组边界
    strncpy(model.m_strFlag, operation_flag_.c_str(), sizeof(model.m_strFlag) - 1);
    strncpy(model.m_strIp, target_ip.c_str(), sizeof(model.m_strIp) - 1);
    strncpy(model.m_cmd, cmd.c_str(), sizeof(model.m_cmd) - 1);
    model.m_reqId = req_id;
    
    // 设置槽位数组：m_slot[x] x=槽位号，0对应1槽，11对应第12槽
    // 根据文档：m_slot[x]=1表示要操作，m_slot[x]=0表示不操作
    for (int slot_num : slot_numbers) {
        if (slot_num >= 1 && slot_num <= 12) {
            model.m_slot[slot_num - 1] = static_cast<char>(SlotStatus::REQUEST_OPERATION_OR_FAILED); // 1表示要操作
        } else {
            std::cout << "Invalid slot number: " << slot_num << ". Valid range is 1-12" << std::endl;
        }
    }
    
    return model;
}

ResourceController::OperationResult ResourceController::parseResponse(
    const BinaryData& response,
    const std::vector<int>& slot_numbers,
    std::vector<SlotResult>& slot_results,
    std::string& message) const {
    
    // the slot_results only show the index in slot_numbers, the slot_numbers is the slot numbers to operate
    slot_results.clear();
    
    if (response.empty()) {
        message = "Empty response received";
        return OperationResult::INVALID_RESPONSE;
    }
    
    // 如果响应数据足够大，尝试解析为OperationModel结构
    if (response.size() >= sizeof(ResourceController::OperationModel)) {
        const ResourceController::OperationModel* response_model = 
            reinterpret_cast<const ResourceController::OperationModel*>(response.data());
        
        std::ostringstream msg;
        msg << "Response - Flag: " << std::string(response_model->m_strFlag, sizeof(response_model->m_strFlag))
            << ", IP: " << std::string(response_model->m_strIp, sizeof(response_model->m_strIp))
            << ", CMD: " << std::string(response_model->m_cmd, sizeof(response_model->m_cmd))
            << ", ReqID: " << response_model->m_reqId;
        
        // 解析槽位状态
        int success_count = 0;
        int failed_count = 0;
        
        // 只处理请求中指定的槽位
        for (size_t i = 0; i < slot_numbers.size(); ++i) {
            int slot_num = slot_numbers[i];
            if (slot_num >= 1 && slot_num <= 12) {
                char slot_status = response_model->m_slot[slot_num - 1];
                SlotResult slot_result;
                slot_result.slot_number = i + 1; // 使用在slot_numbers中的索引
                
                if (slot_status == static_cast<char>(SlotStatus::NO_OPERATION_OR_SUCCESS)) {
                    slot_result.status = SlotStatus::NO_OPERATION_OR_SUCCESS;
                    success_count++;
                } else if (slot_status == static_cast<char>(SlotStatus::REQUEST_OPERATION_OR_FAILED)) {
                    slot_result.status = SlotStatus::REQUEST_OPERATION_OR_FAILED;
                    failed_count++;
                }
                
                slot_results.push_back(slot_result);
            }
        }
        
        msg << ", Processed slots: " << slot_numbers.size()
            << ", Success: " << success_count 
            << ", Failed: " << failed_count;
        
        message = msg.str();
        
        // 根据成功/失败数量确定结果
        if (failed_count == 0 && success_count > 0) {
            return OperationResult::SUCCESS;
        } else if (success_count > 0 && failed_count > 0) {
            return OperationResult::PARTIAL_SUCCESS;
        } else if (failed_count > 0) {
            return OperationResult::INVALID_RESPONSE;
        } else {
            return OperationResult::SUCCESS; // 没有明确失败的情况认为成功
        }
    }
        
    // 默认认为是成功的
    return OperationResult::SUCCESS;
}

