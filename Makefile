# Makefile for zygl project
# 支持 Linux、macOS 和 Windows (MinGW/MSYS2)

# 编译器设置
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic
LDFLAGS = 

# 检测操作系统
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -pthread
    PLATFORM = linux
endif
ifeq ($(UNAME_S),Darwin)
    PLATFORM = macos
endif
ifeq ($(OS),Windows_NT)
    LDFLAGS += -lws2_32
    PLATFORM = windows
endif

# 项目配置
PROJECT_NAME = zygl
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin

# 包含目录
INCLUDES = -I. -Ithird_party

# 源文件
SOURCES = \
	main.cpp \
	src/infrastructure/config/config_manager.cpp \
	src/infrastructure/config/chassis_factory.cpp \
	src/infrastructure/config/logger_config.cpp \
	src/infrastructure/api_client/qyw_api_client.cpp \
	src/infrastructure/collectors/data_collector_service.cpp \
	src/infrastructure/controller/resource_controller.cpp \
	src/interfaces/udp/resource_monitor_broadcaster.cpp \
	src/interfaces/http/alert_receiver_server.cpp \
	src/interfaces/cli/cli_service.cpp

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
	@echo "复制配置文件..."
	@cp -f config.json $(BIN_DIR)/config.json 2>/dev/null || true
	@cp -f chassis_config.json $(BIN_DIR)/chassis_config.json 2>/dev/null || true

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
run: $(TARGET)
	@cd $(BIN_DIR) && ./$(PROJECT_NAME)

# 调试构建
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# 发布构建
release: CXXFLAGS += -O3 -DNDEBUG
release: clean $(TARGET)

# 显示帮助信息
help:
	@echo "可用目标:"
	@echo "  all      - 构建项目（默认）"
	@echo "  clean    - 清理构建文件"
	@echo "  debug    - 构建调试版本"
	@echo "  release  - 构建发布版本"
	@echo "  run      - 构建并运行"
	@echo "  install  - 安装到系统（需要 root 权限）"
	@echo "  uninstall - 从系统卸载"
	@echo "  help     - 显示此帮助信息"

.PHONY: all clean install uninstall run debug release help

