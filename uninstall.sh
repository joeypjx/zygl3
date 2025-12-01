#!/bin/bash

###############################################################################
# zygl 卸载脚本
# 用于从 CentOS Linux 服务器上卸载 zygl 服务
###############################################################################

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 配置变量
SERVICE_NAME="zygl"
INSTALL_PREFIX="/usr/local"
BIN_DIR="${INSTALL_PREFIX}/bin"
ETC_DIR="/etc/${SERVICE_NAME}"
DATA_DIR="/var/lib/${SERVICE_NAME}"
LOG_DIR="/var/log/${SERVICE_NAME}"
SYSTEMD_DIR="/etc/systemd/system"

# 检查是否为 root 用户
check_root() {
    if [ "$EUID" -ne 0 ]; then 
        echo -e "${RED}错误: 请使用 root 用户运行此脚本${NC}"
        echo "使用: sudo $0"
        exit 1
    fi
}

# 停止并禁用服务
stop_service() {
    if systemctl is-active --quiet "${SERVICE_NAME}.service" 2>/dev/null; then
        echo -e "${GREEN}停止 ${SERVICE_NAME} 服务...${NC}"
        systemctl stop "${SERVICE_NAME}.service"
    fi
    
    if systemctl is-enabled --quiet "${SERVICE_NAME}.service" 2>/dev/null; then
        echo -e "${GREEN}禁用 ${SERVICE_NAME} 服务...${NC}"
        systemctl disable "${SERVICE_NAME}.service"
    fi
}

# 删除 systemd 服务文件
remove_systemd_service() {
    local service_file="${SYSTEMD_DIR}/${SERVICE_NAME}.service"
    if [ -f "$service_file" ]; then
        echo -e "${GREEN}删除 systemd 服务文件...${NC}"
        rm -f "$service_file"
        systemctl daemon-reload
    fi
}

# 删除可执行文件
remove_binary() {
    local binary="${BIN_DIR}/${SERVICE_NAME}"
    if [ -f "$binary" ]; then
        echo -e "${GREEN}删除可执行文件: ${binary}${NC}"
        rm -f "$binary"
    fi
}

# 删除配置文件（可选）
remove_config() {
    echo -e "${YELLOW}是否删除配置文件? (y/n)${NC}"
    read -p "> " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        if [ -d "$ETC_DIR" ]; then
            echo -e "${GREEN}删除配置文件目录: ${ETC_DIR}${NC}"
            rm -rf "$ETC_DIR"
        fi
    else
        echo -e "${YELLOW}保留配置文件目录: ${ETC_DIR}${NC}"
    fi
}

# 删除数据目录（可选）
remove_data() {
    echo -e "${YELLOW}是否删除数据目录? (y/n)${NC}"
    read -p "> " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        if [ -d "$DATA_DIR" ]; then
            echo -e "${GREEN}删除数据目录: ${DATA_DIR}${NC}"
            rm -rf "$DATA_DIR"
        fi
    else
        echo -e "${YELLOW}保留数据目录: ${DATA_DIR}${NC}"
    fi
}

# 删除日志目录（可选）
remove_logs() {
    echo -e "${YELLOW}是否删除日志目录? (y/n)${NC}"
    read -p "> " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        if [ -d "$LOG_DIR" ]; then
            echo -e "${GREEN}删除日志目录: ${LOG_DIR}${NC}"
            rm -rf "$LOG_DIR"
        fi
    else
        echo -e "${YELLOW}保留日志目录: ${LOG_DIR}${NC}"
    fi
}

# 删除用户和组（已禁用，服务使用 root 用户运行）
# remove_user() {
#     # 服务使用 root 用户运行，不需要删除专用用户
# }

# 主卸载流程
main() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  zygl 服务卸载脚本${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    
    check_root
    
    stop_service
    remove_systemd_service
    remove_binary
    remove_config
    remove_data
    remove_logs
    
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  卸载完成！${NC}"
    echo -e "${GREEN}========================================${NC}"
}

# 运行主函数
main "$@"

