#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace app::infrastructure::utils {

/**
 * @brief UDP数据打印工具类
 * @detail 用于打印接收到的或要发送的UDP二进制数据
 */
class UdpDataPrinter {
public:
    /**
     * @brief 打印接收到的UDP数据
     * @param data 数据指针
     * @param length 数据长度
     * @param sourceAddr 源地址（可选，用于显示发送方信息）
     * @param sourcePort 源端口（可选）
     */
    static void PrintReceivedData(const void* data, size_t length, 
                                  const std::string& sourceAddr = "", 
                                  uint16_t sourcePort = 0);

    /**
     * @brief 打印要发送的UDP数据
     * @param data 数据指针
     * @param length 数据长度
     * @param destAddr 目标地址（可选，用于显示接收方信息）
     * @param destPort 目标端口（可选）
     */
    static void PrintSentData(const void* data, size_t length,
                              const std::string& destAddr = "",
                              uint16_t destPort = 0);

    /**
     * @brief 打印UDP数据（通用方法）
     * @param data 数据指针
     * @param length 数据长度
     * @param direction 方向标识（"接收" 或 "发送"）
     * @param addr 地址信息（可选）
     * @param port 端口信息（可选）
     */
    static void PrintData(const void* data, size_t length,
                          const std::string& direction,
                          const std::string& addr = "",
                          uint16_t port = 0);

    /**
     * @brief 按顺序打印接收到的UDP数据（简单格式）
     * @param data 数据指针
     * @param length 数据长度
     * @param sourceAddr 源地址（可选，用于显示发送方信息）
     * @param sourcePort 源端口（可选）
     * @detail 直接按顺序打印所有字节的十六进制值，用空格分隔
     */
    static void PrintReceivedDataSimple(const void* data, size_t length,
                                        const std::string& sourceAddr = "",
                                        uint16_t sourcePort = 0);

    /**
     * @brief 按顺序打印要发送的UDP数据（简单格式）
     * @param data 数据指针
     * @param length 数据长度
     * @param destAddr 目标地址（可选，用于显示接收方信息）
     * @param destPort 目标端口（可选）
     * @detail 直接按顺序打印所有字节的十六进制值，用空格分隔
     */
    static void PrintSentDataSimple(const void* data, size_t length,
                                    const std::string& destAddr = "",
                                    uint16_t destPort = 0);

private:
    /**
     * @brief 将单个字节转换为十六进制字符串
     */
    static std::string ByteToHex(uint8_t byte);

    /**
     * @brief 判断字符是否可打印
     */
    static bool IsPrintable(uint8_t byte);
};

}

