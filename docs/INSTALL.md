# zygl 安装指南

本文档说明如何在 CentOS Linux 服务器上安装和配置 zygl 服务。

## 前置要求

1. **操作系统**: CentOS 7 或更高版本（或 RHEL 7+）
2. **权限**: root 用户权限
3. **已编译的可执行文件**: 确保已成功编译项目，生成 `build/zygl` 可执行文件

## 安装步骤

### 1. 编译项目

```bash
cd /path/to/zygl3
mkdir -p build
cd build
cmake ..
make -j4
```

### 2. 运行安装脚本

```bash
cd /path/to/zygl3
sudo ./install.sh
```

或者指定可执行文件路径：

```bash
sudo ./install.sh build/zygl
```

### 3. 配置服务

安装脚本会将配置文件复制到 `/etc/zygl/` 目录。请根据实际环境修改以下配置文件：

- `/etc/zygl/config.json` - 主配置文件
- `/etc/zygl/chassis_config.json` - 机箱配置文件

### 4. 启动服务

```bash
# 启动服务
sudo systemctl start zygl

# 查看服务状态
sudo systemctl status zygl

# 查看日志
sudo journalctl -u zygl -f

# 设置开机自启
sudo systemctl enable zygl
```

## 安装位置

安装脚本会在以下位置创建文件和目录：

- **可执行文件**: `/usr/local/bin/zygl`
- **配置文件**: `/etc/zygl/`
- **数据目录**: `/var/lib/zygl/`
- **日志目录**: `/var/log/zygl/`
- **服务文件**: `/etc/systemd/system/zygl.service`
- **运行用户**: `zygl` (系统用户)

## 服务管理

### 基本命令

```bash
# 启动服务
sudo systemctl start zygl

# 停止服务
sudo systemctl stop zygl

# 重启服务
sudo systemctl restart zygl

# 查看状态
sudo systemctl status zygl

# 查看日志
sudo journalctl -u zygl -f

# 查看最近 100 行日志
sudo journalctl -u zygl -n 100

# 启用开机自启
sudo systemctl enable zygl

# 禁用开机自启
sudo systemctl disable zygl
```

## 卸载

如果需要卸载服务，运行：

```bash
sudo ./uninstall.sh
```

卸载脚本会询问是否删除配置文件、数据目录和日志目录，您可以选择保留这些文件。

## 配置文件说明

### config.json

主配置文件，包含以下主要配置项：

- `api`: API 服务器配置（base_url, port, endpoints 等）
- `heartbeat`: 心跳配置（client_ip）
- `udp`: UDP 组播配置（multicast_group, port, commands 等）
- `alert_server`: 告警服务器配置（host, port）
- `collector`: 数据采集配置（interval_seconds, board_timeout_seconds）

### chassis_config.json

机箱配置文件，定义系统中所有机箱和板卡的配置信息。

## 故障排查

### 服务无法启动

1. 检查服务状态：
   ```bash
   sudo systemctl status zygl
   ```

2. 查看详细日志：
   ```bash
   sudo journalctl -u zygl -n 50 --no-pager
   ```

3. 检查配置文件：
   ```bash
   sudo cat /etc/zygl/config.json
   sudo cat /etc/zygl/chassis_config.json
   ```

4. 检查文件权限：
   ```bash
   ls -la /usr/local/bin/zygl
   ls -la /etc/zygl/
   ```

### 配置文件路径问题

如果程序无法找到配置文件，请确保：

1. 配置文件存在于 `/etc/zygl/` 目录
2. 配置文件权限正确（644，root:root）
3. 服务的工作目录设置为 `/etc/zygl`（已在 systemd 服务文件中配置）

### 端口冲突

如果服务启动失败，可能是端口被占用。检查配置文件中的端口设置：

- UDP 端口（默认 0x100A = 4106）
- HTTP 告警服务器端口（默认 8888）

## 安全建议

1. **文件权限**: 配置文件应设置为只读（644），只有 root 用户可以修改
2. **运行用户**: 服务以非特权用户 `zygl` 运行，提高安全性
3. **防火墙**: 根据需要配置防火墙规则，开放必要的端口
4. **日志轮转**: 建议配置日志轮转，避免日志文件过大

## 注意事项

1. 首次安装后，请务必检查并修改配置文件中的 IP 地址、端口等参数
2. 确保配置文件中的路径和参数符合实际环境
3. 建议在生产环境部署前进行充分测试
4. 定期备份配置文件和重要数据

## 技术支持

如有问题，请查看项目文档或联系技术支持。

