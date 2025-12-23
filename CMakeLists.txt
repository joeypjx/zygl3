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
    src/infrastructure/utils/udp_data_printer.cpp
    src/infrastructure/ha/heartbeat_service.cpp
    src/interfaces/udp/resource_monitor_broadcaster.cpp
    src/interfaces/http/alert_receiver_server.cpp
    src/interfaces/cli/cli_service.cpp
    src/interfaces/bmc/bmc_receiver.cpp
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

# 构建类型配置
# 设置默认构建类型为 Debug（如果未指定）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Debug 模式下的编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        # MSVC: /Zi 生成调试信息，/Od 禁用优化
        target_compile_options(zygl PRIVATE /Zi /Od)
    else()
        # GCC/Clang: -g 生成调试信息，-O0 禁用优化
        target_compile_options(zygl PRIVATE -g -O0)
    endif()
endif()

# Release 模式下的编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        # MSVC: /O2 最大优化，/DNDEBUG 定义 NDEBUG 宏
        target_compile_options(zygl PRIVATE /O2 /DNDEBUG)
        target_compile_definitions(zygl PRIVATE NDEBUG)
    else()
        # GCC/Clang: -O3 最大优化，-DNDEBUG 定义 NDEBUG 宏，-s 去除符号表
        target_compile_options(zygl PRIVATE -O3 -DNDEBUG)
        target_compile_definitions(zygl PRIVATE NDEBUG)
        # 可选：去除调试符号以减小文件大小（如果需要调试信息，可以去掉 -s）
        # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
    endif()
endif()

