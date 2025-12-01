cmake_minimum_required(VERSION 3.10)
project(zygl3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件
set(SOURCES
    main.cpp
    src/infrastructure/config/config_manager.cpp
    src/infrastructure/config/chassis_factory.cpp
    src/infrastructure/config/logger_config.cpp
    src/infrastructure/api_client/qyw_api_client.cpp
    src/infrastructure/collectors/data_collector_service.cpp
    src/infrastructure/controller/resource_controller.cpp
    src/interfaces/udp/resource_monitor_broadcaster.cpp
    src/interfaces/http/alert_receiver_server.cpp
    src/interfaces/cli/cli_service.cpp
)

# 创建可执行文件
add_executable(zygl ${SOURCES})

# 包含目录（使用 target_include_directories 更现代的方式）
# 注意：spdlog 使用 <spdlog/spdlog.h>，所以包含路径应该指向包含 spdlog 目录的父目录
target_include_directories(zygl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# 运行时资源：将配置文件复制到可执行文件目录
add_custom_command(TARGET zygl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/config.json
            $<TARGET_FILE_DIR:zygl>/config.json
)

# 链接系统库（如果需要）
if(UNIX AND NOT APPLE)
    # Linux 系统
    target_link_libraries(zygl pthread)
endif()

# macOS 不需要额外链接库，网络和线程库都在系统框架中
if(APPLE)
    # macOS 特定配置（如果需要）
endif()

# Windows 特定配置
if(WIN32)
    # Windows 需要链接 WS2_32
    target_link_libraries(zygl ws2_32)
endif()

# 编译器选项
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

