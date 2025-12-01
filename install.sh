#!/bin/bash

###############################################################################
# zygl 安装脚本
# 用于在 CentOS Linux 服务器上安装 zygl 服务
###############################################################################

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

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

# 检查操作系统
check_os() {
    if ! command -v systemctl &> /dev/null; then
        echo -e "${RED}错误: 系统不支持 systemd${NC}"
        exit 1
    fi
}

# 检查可执行文件是否存在
check_binary() {
    local binary_path="$1"
    if [ ! -f "$binary_path" ]; then
        echo -e "${RED}错误: 找不到可执行文件: $binary_path${NC}"
        echo "请先编译项目: cd build && cmake .. && make"
        exit 1
    fi
    
    if [ ! -x "$binary_path" ]; then
        echo -e "${RED}错误: 文件不可执行: $binary_path${NC}"
        exit 1
    fi
}

# 创建用户和组（已禁用，使用 root 用户运行）
# create_user() {
#     # 服务将使用 root 用户运行，不需要创建专用用户
# }

# 创建目录结构
create_directories() {
    echo -e "${GREEN}创建目录结构...${NC}"
    mkdir -p "$BIN_DIR"
    mkdir -p "$ETC_DIR"
    mkdir -p "$DATA_DIR"
    mkdir -p "$LOG_DIR"
    
    # 设置权限（root 用户运行，使用 root 权限）
    chmod 755 "$ETC_DIR"
    chmod 755 "$DATA_DIR"
    chmod 755 "$LOG_DIR"
}

# 安装可执行文件
install_binary() {
    local source_binary="$1"
    local target_binary="${BIN_DIR}/${SERVICE_NAME}"
    
    echo -e "${GREEN}安装可执行文件...${NC}"
    cp "$source_binary" "$target_binary"
    chmod 755 "$target_binary"
    chown root:root "$target_binary"
    
    echo -e "${GREEN}可执行文件已安装到: ${target_binary}${NC}"
}

# 安装配置文件
install_config() {
    local source_dir="$(dirname "$0")"
    
    echo -e "${GREEN}安装配置文件...${NC}"
    
    # 安装 config.json
    if [ -f "${source_dir}/config.json" ]; then
        if [ -f "${ETC_DIR}/config.json" ]; then
            echo -e "${YELLOW}配置文件 ${ETC_DIR}/config.json 已存在，备份为 config.json.bak${NC}"
            cp "${ETC_DIR}/config.json" "${ETC_DIR}/config.json.bak"
        fi
        cp "${source_dir}/config.json" "${ETC_DIR}/config.json"
        chmod 644 "${ETC_DIR}/config.json"
        chown root:root "${ETC_DIR}/config.json"
        echo -e "${GREEN}已安装: ${ETC_DIR}/config.json${NC}"
    else
        echo -e "${YELLOW}警告: 找不到 config.json，请手动创建${NC}"
    fi
    
    # 安装 chassis_config.json
    if [ -f "${source_dir}/chassis_config.json" ]; then
        if [ -f "${ETC_DIR}/chassis_config.json" ]; then
            echo -e "${YELLOW}配置文件 ${ETC_DIR}/chassis_config.json 已存在，备份为 chassis_config.json.bak${NC}"
            cp "${ETC_DIR}/chassis_config.json" "${ETC_DIR}/chassis_config.json.bak"
        fi
        cp "${source_dir}/chassis_config.json" "${ETC_DIR}/chassis_config.json"
        chmod 644 "${ETC_DIR}/chassis_config.json"
        chown root:root "${ETC_DIR}/chassis_config.json"
        echo -e "${GREEN}已安装: ${ETC_DIR}/chassis_config.json${NC}"
    else
        echo -e "${YELLOW}警告: 找不到 chassis_config.json，请手动创建${NC}"
    fi
}

# 创建 systemd 服务文件
create_systemd_service() {
    local service_file="${SYSTEMD_DIR}/${SERVICE_NAME}.service"
    
    echo -e "${GREEN}创建 systemd 服务文件...${NC}"
    
    cat > "$service_file" <<EOF
[Unit]
Description=zygl Resource Management Service
After=network.target

[Service]
Type=simple
WorkingDirectory=${ETC_DIR}
ExecStart=${BIN_DIR}/${SERVICE_NAME}
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal
SyslogIdentifier=${SERVICE_NAME}

[Install]
WantedBy=multi-user.target
EOF

    chmod 644 "$service_file"
    echo -e "${GREEN}服务文件已创建: ${service_file}${NC}"
}

# 更新配置文件路径（如果需要）
update_config_paths() {
    # 注意：如果程序硬编码了配置文件路径，可能需要修改源代码
    # 这里我们假设程序会从工作目录读取配置文件
    # 或者可以通过环境变量指定配置文件路径
    echo -e "${YELLOW}提示: 确保程序配置指向 ${ETC_DIR}/config.json${NC}"
}

# 重新加载 systemd
reload_systemd() {
    echo -e "${GREEN}重新加载 systemd 配置...${NC}"
    systemctl daemon-reload
}

# 启用服务
enable_service() {
    echo -e "${GREEN}启用 ${SERVICE_NAME} 服务...${NC}"
    systemctl enable "${SERVICE_NAME}.service"
    echo -e "${GREEN}服务已启用，可以使用以下命令管理:${NC}"
    echo "  启动服务: systemctl start ${SERVICE_NAME}"
    echo "  停止服务: systemctl stop ${SERVICE_NAME}"
    echo "  重启服务: systemctl restart ${SERVICE_NAME}"
    echo "  查看状态: systemctl status ${SERVICE_NAME}"
    echo "  查看日志: journalctl -u ${SERVICE_NAME} -f"
}

# 主安装流程
main() {
    local source_binary=""
    
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  zygl 服务安装脚本${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    
    # 检查参数
    if [ $# -eq 1 ]; then
        source_binary="$1"
    elif [ -f "build/zygl" ]; then
        source_binary="build/zygl"
    elif [ -f "./zygl" ]; then
        source_binary="./zygl"
    else
        echo -e "${RED}错误: 找不到可执行文件${NC}"
        echo "用法: $0 [可执行文件路径]"
        echo "或者确保在项目根目录运行，且已编译 build/zygl"
        exit 1
    fi
    
    # 执行安装步骤
    check_root
    check_os
    check_binary "$source_binary"
    create_directories
    install_binary "$source_binary"
    install_config
    create_systemd_service
    reload_systemd
    
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  安装完成！${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "安装位置:"
    echo -e "  可执行文件: ${BIN_DIR}/${SERVICE_NAME}"
    echo -e "  配置文件:   ${ETC_DIR}/"
    echo -e "  数据目录:   ${DATA_DIR}/"
    echo -e "  日志目录:   ${LOG_DIR}/"
    echo ""
    echo -e "${YELLOW}重要提示:${NC}"
    echo -e "1. 请检查并修改配置文件: ${ETC_DIR}/config.json"
    echo -e "2. 请检查并修改配置文件: ${ETC_DIR}/chassis_config.json"
    echo -e "3. 如果程序需要从特定路径读取配置文件，请修改源代码或使用符号链接"
    echo ""
    
    # 询问是否立即启动服务
    read -p "是否立即启动服务? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        systemctl start "${SERVICE_NAME}.service"
        sleep 2
        systemctl status "${SERVICE_NAME}.service" --no-pager
    else
        enable_service
    fi
}

# 运行主函数
main "$@"

