#include "udp_data_printer.h"
#include <spdlog/spdlog.h>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace app::infrastructure::utils {

void UdpDataPrinter::PrintReceivedData(const void* data, size_t length,
                                       const std::string& sourceAddr,
                                       uint16_t sourcePort) {
    std::string addrInfo;
    if (!sourceAddr.empty()) {
        addrInfo = "来自 " + sourceAddr;
        if (sourcePort > 0) {
            addrInfo += ":" + std::to_string(sourcePort);
        }
        addrInfo += " - ";
    }
    PrintData(data, length, "接收", addrInfo, sourcePort);
}

void UdpDataPrinter::PrintSentData(const void* data, size_t length,
                                   const std::string& destAddr,
                                   uint16_t destPort) {
    std::string addrInfo;
    if (!destAddr.empty()) {
        addrInfo = "发送到 " + destAddr;
        if (destPort > 0) {
            addrInfo += ":" + std::to_string(destPort);
        }
        addrInfo += " - ";
    }
    PrintData(data, length, "发送", addrInfo, destPort);
}

void UdpDataPrinter::PrintData(const void* data, size_t length,
                                const std::string& direction,
                                const std::string& addr,
                                uint16_t port) {
    if (data == nullptr || length == 0) {
        spdlog::warn("UDP数据打印: 数据为空");
        return;
    }

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    
    // 打印标题
    std::string title = "[UDP " + direction + "]";
    if (!addr.empty()) {
        title += " " + addr;
    }
    title += " 长度: " + std::to_string(length) + " 字节";
    spdlog::info("{}", title);
    spdlog::info("{}", std::string(title.length(), '='));

    // 按16字节一行打印（类似hexdump格式）
    for (size_t offset = 0; offset < length; offset += 16) {
        std::ostringstream hexLine;
        std::ostringstream asciiLine;
        
        // 偏移量
        hexLine << std::hex << std::setfill('0') << std::setw(8) << offset << "  ";
        
        // 十六进制数据（每16字节）
        for (size_t i = 0; i < 16; ++i) {
            if (offset + i < length) {
                hexLine << ByteToHex(bytes[offset + i]) << " ";
            } else {
                hexLine << "   ";  // 填充空格
            }
            
            // 每8字节后加一个空格
            if (i == 7) {
                hexLine << " ";
            }
        }
        
        // ASCII字符显示
        hexLine << " |";
        for (size_t i = 0; i < 16 && offset + i < length; ++i) {
            uint8_t byte = bytes[offset + i];
            if (IsPrintable(byte)) {
                hexLine << static_cast<char>(byte);
            } else {
                hexLine << ".";
            }
        }
        hexLine << "|";
        
        spdlog::info("{}", hexLine.str());
    }
    
    // 打印总计
    spdlog::info("总计: {} 字节", length);
}

std::string UdpDataPrinter::ByteToHex(uint8_t byte) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(2) 
        << static_cast<unsigned int>(byte);
    return oss.str();
}

bool UdpDataPrinter::IsPrintable(uint8_t byte) {
    return (byte >= 32 && byte < 127);
}

void UdpDataPrinter::PrintReceivedDataSimple(const void* data, size_t length,
                                             const std::string& sourceAddr,
                                             uint16_t sourcePort) {
    if (data == nullptr || length == 0) {
        spdlog::warn("UDP数据打印: 数据为空");
        return;
    }

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    
    // 构建标题
    std::ostringstream title;
    title << "[UDP 接收]";
    if (!sourceAddr.empty()) {
        title << " 来自 " << sourceAddr;
        if (sourcePort > 0) {
            title << ":" << sourcePort;
        }
    }
    title << " (" << length << " 字节): ";
    
    // 按顺序打印所有字节的十六进制值（无空格）
    std::ostringstream hexData;
    for (size_t i = 0; i < length; ++i) {
        hexData << ByteToHex(bytes[i]);
    }
    
    spdlog::info("{}{}", title.str(), hexData.str());
}

void UdpDataPrinter::PrintSentDataSimple(const void* data, size_t length,
                                         const std::string& destAddr,
                                         uint16_t destPort) {
    if (data == nullptr || length == 0) {
        spdlog::warn("UDP数据打印: 数据为空");
        return;
    }

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    
    // 构建标题
    std::ostringstream title;
    title << "[UDP 发送]";
    if (!destAddr.empty()) {
        title << " 发送到 " << destAddr;
        if (destPort > 0) {
            title << ":" << destPort;
        }
    }
    title << " (" << length << " 字节): ";
    
    // 按顺序打印所有字节的十六进制值（无空格）
    std::ostringstream hexData;
    for (size_t i = 0; i < length; ++i) {
        hexData << ByteToHex(bytes[i]);
    }
    
    spdlog::info("{}{}", title.str(), hexData.str());
}

}

