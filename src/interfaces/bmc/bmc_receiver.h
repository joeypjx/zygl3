#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <cstdint>
#include <netinet/in.h>
#include "src/domain/i_chassis_repository.h"

namespace app::interfaces {

/**
 * @brief BMC UDP 报文结构定义
 */
#pragma pack(push, 1)

// 风扇状态格式
struct UdpFanInfo {
    std::uint8_t  fanseq;      // 风扇序号（0-5），无此序号时为0xFF
    std::uint8_t  fanmode;     // 告警及工作模式（高4位：告警类型，低4位：工作模式）
    std::uint32_t fanspeed;    // 风扇转速（占空比）
};

// 传感器状态格式
struct UdpSensorInfo {
    std::uint8_t  sensorseq;      // 传感器序号（0-7），无此序号时为0xFF
    std::uint8_t  sensortype;     // 传感器类型
    std::uint8_t  sensorname[6];  // 传感器名称
    std::uint8_t  sensorvalue_L;  // 数值低字节（小数部分）
    std::uint8_t  sensorvalue_H;  // 数值高字节（整数部分）
    std::uint8_t  sensoralmtype;  // 告警类型
    std::uint8_t  sensorresv;     // 预留
};

// 电源槽位状态
struct UdpPowerBoardInfo {
    std::uint8_t  ipmbaddr;       // 槽位地址，槽位号（1字节）
    std::uint16_t moduletype;     // 模块设备号（2字节）
    std::uint16_t bmccompany;     // 厂商编号（2字节）
    std::uint8_t  bmcversion[8];  // BMC软件版本（8字节）
    std::uint8_t  snnum[8];       // 模块板卡序列号（8字节）
    std::uint8_t  protime[8];     // 生产日期（8字节，年月日各2位）
    std::uint8_t  status;         // 板卡健康状态，保留（1字节）
    std::uint8_t  sensornum;      // 传感器数量（1字节，5-8）
    UdpSensorInfo sensor[8];      // 传感器状态（12*8 = 96字节）
    std::uint8_t  resv[1];        // 保留（1字节）
};

// 负载槽位状态
struct UdpSlotBoardInfo {
    std::uint8_t  ipmbaddr;       // 槽位地址，槽位号（1字节）
    std::uint16_t moduletype;     // 模块设备号（2字节）
    std::uint8_t  prst;           // 在位信息（1字节，0：不在位，1：在位）
    std::uint16_t bmccompany;     // 厂商编号（2字节）
    std::uint8_t  bmcversion[8];  // BMC软件版本（8字节）
    std::uint8_t  snnum[8];       // 模块板卡序列号（8字节）
    std::uint8_t  protime[8];     // 生产日期（8字节，年月日各2位）
    std::uint8_t  status;         // 板卡健康状态，保留（1字节）
    std::uint8_t  sensornum;      // 传感器数量（1字节，5-8）
    UdpSensorInfo sensor[8];      // 传感器状态（12*8 = 96字节）
    std::uint8_t  resv[2];        // 备用（2字节）
};

// UDP 报文结构
struct UdpInfo {
    std::uint16_t head;           // 报文头（2字节，0x5AA5）
    std::uint16_t msglenth;       // 报文长度（2字节，包含报文头尾）
    std::uint16_t seqnum;         // 编号（2字节，1-65535循环递增）
    std::uint16_t msgtype;        // 报文标识（2字节，0x0002）
    std::uint32_t timestamp;      // 时间戳（4字节）
    std::uint16_t moduletype;     // 模块设备号（2字节）
    std::uint8_t  recv[2];        // 备用（2字节）
    std::uint8_t  boxname;        // 机箱型号（1字节，固定1）
    std::uint8_t  boxid;          // 机箱号（1字节）
    UdpFanInfo    fan[6];         // 风扇1-6状态（6*6 = 36字节，风扇序号0-5，无此风扇序号为0xFF）
    // 电源模块1-2（2*128 = 256字节）
    UdpPowerBoardInfo dyboard[2]; // 电源1/2（2个，每个128字节）
    // 负载槽1,2,3,4,6,7,9,10,11,12（10*130 = 1300字节）
    // 注意：协议中负载槽顺序是：槽1、槽2、槽3、槽4、槽6、槽7、槽9、槽10、槽11、槽12
    UdpSlotBoardInfo board[10];   // 负载1-4 6-7 9-12（10个，每个130字节）
    std::uint16_t tail;            // 报文尾（2字节，0xA55A）
};

#pragma pack(pop)

/**
 * @brief BMC UDP 组播接收器
 * @detail 通过UDP组播接收BMC状态报文
 */
class BmcReceiver {
public:
    /**
     * @brief 构造函数
     * @param chassisRepo 机箱仓储，用于更新板卡状态
     * @param multicastGroup 组播地址，默认 224.100.200.15
     * @param port 组播端口，默认 5715
     */
    BmcReceiver(
        std::shared_ptr<app::domain::IChassisRepository> chassisRepo,
        const std::string& multicastGroup = "224.100.200.15",
        uint16_t port = 5715
    );

    /**
     * @brief 析构函数
     */
    ~BmcReceiver();

    /**
     * @brief 启动接收服务
     */
    void Start();

    /**
     * @brief 停止接收服务
     */
    void Stop();

    /**
     * @brief 检查是否正在运行
     */
    bool IsRunning() const { return m_running; }

private:
    /**
     * @brief 接收循环
     */
    void ReceiveLoop();

    /**
     * @brief 处理接收到的UDP报文
     * @param data 接收到的数据
     * @param length 数据长度
     */
    void HandleReceivedPacket(const char* data, size_t length);

    /**
     * @brief 验证UDP报文格式
     * @param info UDP报文结构指针
     * @return 是否有效
     */
    bool ValidatePacket(const UdpInfo* info);

private:
    std::shared_ptr<app::domain::IChassisRepository> m_chassisRepo;
    std::string m_multicastGroup;
    uint16_t m_port;
    int m_socket;
    
    std::atomic<bool> m_running;
    std::thread m_receiveThread;
};

}

