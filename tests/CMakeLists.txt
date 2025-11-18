cmake_minimum_required(VERSION 3.10)
project(zygl3_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置Google Test路径
set(GTEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/googletest")
set(GTEST_INCLUDE_DIR "${GTEST_ROOT}/googletest/include")
set(GMOCK_INCLUDE_DIR "${GTEST_ROOT}/googlemock/include")

# 编译Google Test库
add_subdirectory("${GTEST_ROOT}" "${CMAKE_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${GTEST_INCLUDE_DIR})
include_directories(${GMOCK_INCLUDE_DIR})

# 源文件路径（用于链接）
# 注意：不包含main.cpp，因为Google Test通过gtest_main提供main函数
set(MAIN_SOURCES
    ${CMAKE_SOURCE_DIR}/../infrastructure/config/config_manager.cpp
    ${CMAKE_SOURCE_DIR}/../infrastructure/api_client/qyw_api_client.cpp
    ${CMAKE_SOURCE_DIR}/../infrastructure/collectors/data_collector_service.cpp
    ${CMAKE_SOURCE_DIR}/../infrastructure/controller/resource_controller.cpp
    ${CMAKE_SOURCE_DIR}/../interfaces/udp/resource_monitor_broadcaster.cpp
    ${CMAKE_SOURCE_DIR}/../interfaces/http/alert_receiver_server.cpp
    ${CMAKE_SOURCE_DIR}/../interfaces/cli/cli_service.cpp
)

# 测试源文件
set(TEST_SOURCES
    XKSJ_test.cpp
    XKJH_test.cpp
    RWKZ_test.cpp
    RWJK_test.cpp
    ZYJK_test.cpp
    ZYKZ_test.cpp
    # integration_test.cpp
)

# Mock和工具源文件
set(MOCK_SOURCES
    mocks/mock_api_client.cpp
    # utils/test_data_generator.cpp  # 如果实现需要
)

# 创建测试可执行文件
add_executable(zygl3_tests ${TEST_SOURCES} ${MAIN_SOURCES})

# 链接Google Test库
target_link_libraries(zygl3_tests
    gtest
    gtest_main
    gmock
    gmock_main
)

# 如果是macOS，可能需要链接pthread
if(APPLE)
    target_link_libraries(zygl3_tests pthread)
endif()

# 启用测试
enable_testing()

# 添加测试
add_test(NAME Unit2Tests COMMAND zygl3_tests)

