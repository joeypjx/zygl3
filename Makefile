# Makefile for zygl project
# 支持 Linux、macOS 和 Windows (MinGW/MSYS2)
# 
# 使用方法：
#   make          - 构建项目（默认）
#   make clean    - 清理构建文件
#   make debug    - 构建调试版本（包含调试信息，禁用优化）
#   make release  - 构建发布版本（优化，去除调试信息）
#   make run      - 构建并运行
#   make install  - 安装到系统（需要 root 权限）
#   make uninstall - 从系统卸载
#   make help     - 显示帮助信息

# 编译器设置
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic
LDFLAGS = 

# 检测操作系统
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    # Linux 需要链接 pthread 库
    LDFLAGS += -pthread
    PLATFORM = linux
endif
ifeq ($(UNAME_S),Darwin)
    # macOS 不需要额外链接库，网络和线程库都在系统框架中
    PLATFORM = macos
endif
ifeq ($(OS),Windows_NT)
    # Windows 需要链接 WS2_32 网络库
    LDFLAGS += -lws2_32
    PLATFORM = windows
endif

# 项目配置
PROJECT_NAME = zygl
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin

# 包含目录
# 注意：spdlog 使用 <spdlog/spdlog.h>，所以包含路径应该指向包含 spdlog 目录的父目录
INCLUDES = -I. -Ithird_party

# 源文件列表（按模块组织）
# 配置模块、API客户端、数据采集、资源控制、工具、HA、UDP、HTTP、CLI、BMC
# 注意：第三方库（spdlog, nlohmann/json, cpp-httplib）都是 header-only，不需要编译
SOURCES = \
	main.cpp \
	src/infrastructure/config/config_manager.cpp \
	src/infrastructure/config/chassis_factory.cpp \
	src/infrastructure/config/logger_config.cpp \
	src/infrastructure/api_client/qyw_api_client.cpp \
	src/infrastructure/collectors/data_collector_service.cpp \
	src/infrastructure/controller/resource_controller.cpp \
	src/infrastructure/utils/udp_data_printer.cpp \
	src/infrastructure/ha/heartbeat_service.cpp \
	src/interfaces/udp/resource_monitor_broadcaster.cpp \
	src/interfaces/http/alert_receiver_server.cpp \
	src/interfaces/cli/cli_service.cpp \
	src/interfaces/bmc/bmc_receiver.cpp

# 对象文件
OBJECTS = $(SOURCES:%.cpp=$(BUILD_DIR)/%.o)

# 依赖文件
DEPS = $(OBJECTS:.o=.d)

# 可执行文件
TARGET = $(BIN_DIR)/$(PROJECT_NAME)

# 默认目标
all: $(TARGET)
	@echo "构建完成: $(TARGET)"

# 创建可执行文件
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	@echo "链接 $(TARGET)..."
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "复制配置文件到构建目录..."
	@cp -f config.json $(BIN_DIR)/config.json 2>/dev/null || true
	@cp -f config_full.json $(BIN_DIR)/config_full.json 2>/dev/null || true
	@cp -f chassis_config.json $(BIN_DIR)/chassis_config.json 2>/dev/null || true
	@echo "构建完成: $(TARGET)"

# 创建目录
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# 编译规则：从 .cpp 生成 .o 和 .d
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@echo "编译 $<..."
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# 包含依赖文件（如果存在）
-include $(DEPS)

# 清理
clean:
	@echo "清理构建文件..."
	rm -rf $(BUILD_DIR)
	@echo "清理完成"

# 安装（可选，复制到系统目录）
install: $(TARGET)
	@echo "安装 $(PROJECT_NAME)..."
	@mkdir -p /usr/local/bin
	@cp $(TARGET) /usr/local/bin/$(PROJECT_NAME)
	@chmod +x /usr/local/bin/$(PROJECT_NAME)
	@echo "安装完成: /usr/local/bin/$(PROJECT_NAME)"

# 卸载
uninstall:
	@echo "卸载 $(PROJECT_NAME)..."
	rm -f /usr/local/bin/$(PROJECT_NAME)
	@echo "卸载完成"

# 运行（需要配置文件）
# 支持通过命令行参数指定配置文件：make run CONFIG=config_full.json
run: $(TARGET)
	@cd $(BIN_DIR) && \
	if [ -n "$(CONFIG)" ]; then \
		./$(PROJECT_NAME) -c $(CONFIG); \
	else \
		./$(PROJECT_NAME); \
	fi

# 调试构建（包含调试信息，禁用优化）
debug: CXXFLAGS += -g -O0 -DDEBUG
debug: $(TARGET)
	@echo "调试版本构建完成: $(TARGET)"

# 发布构建（优化，去除调试信息）
release: CXXFLAGS += -O3 -DNDEBUG
release: clean $(TARGET)
	@echo "发布版本构建完成: $(TARGET)"

# 显示帮助信息
help:
	@echo "======================================================================"
	@echo "zygl 项目 Makefile 帮助"
	@echo "======================================================================"
	@echo ""
	@echo "可用目标:"
	@echo "  all        - 构建项目（默认）"
	@echo "  clean      - 清理构建文件"
	@echo "  debug      - 构建调试版本（包含调试信息，禁用优化）"
	@echo "  release    - 构建发布版本（优化，去除调试信息）"
	@echo "  run        - 构建并运行"
	@echo "             用法: make run [CONFIG=config_full.json]"
	@echo "  install    - 安装到系统（需要 root 权限）"
	@echo "  uninstall  - 从系统卸载"
	@echo "  help       - 显示此帮助信息"
	@echo ""
	@echo "示例:"
	@echo "  make                    # 构建项目"
	@echo "  make debug              # 构建调试版本"
	@echo "  make release            # 构建发布版本"
	@echo "  make run                # 运行（使用默认配置）"
	@echo "  make run CONFIG=config_full.json  # 运行（指定配置文件）"
	@echo "  make clean              # 清理构建文件"
	@echo ""
	@echo "配置文件:"
	@echo "  构建时会自动复制以下配置文件到构建目录:"
	@echo "    - config.json         # 主配置文件"
	@echo "    - config_full.json    # 完整版配置文件"
	@echo "    - chassis_config.json # 机箱配置文件"
	@echo ""
	@echo "平台支持:"
	@echo "  - Linux:   自动链接 pthread 库"
	@echo "  - macOS:   使用系统框架（无需额外链接）"
	@echo "  - Windows: 自动链接 ws2_32 库（MinGW/MSYS2）"
	@echo "======================================================================"

.PHONY: all clean install uninstall run debug release help

