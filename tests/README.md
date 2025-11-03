# 测试目录说明

本目录用于存放单元测试代码。

## Google Test 安装状态

✅ Google Test已从源码编译安装到本地：
- 源码位置: `third_party/googletest/`
- 编译产物: `third_party/googletest/build/lib/`
  - `libgtest.a` - Google Test核心库
  - `libgtest_main.a` - Google Test主程序入口
  - `libgmock.a` - Google Mock库
  - `libgmock_main.a` - Google Mock主程序入口

## 目录结构

```
tests/
├── CMakeLists.txt              # 测试项目的CMake配置
├── build/                      # 测试构建目录
│   ├── zygl3_tests            # 测试可执行文件
│   └── googletest/            # 编译后的Google Test库
├── unit2_test.cpp             # 单元2测试（已实现）
├── example_unit2_test.cpp     # 单元2测试示例（简化版）
├── mocks/                      # Mock对象（待创建）
│   ├── mock_api_client.h
│   └── mock_repository.h
└── utils/                      # 测试工具（待创建）
    ├── test_data_generator.h
    └── test_helpers.h
```

## 构建和运行测试

### 1. 构建测试
```bash
cd tests
mkdir -p build && cd build
cmake ..
make
```

### 2. 运行测试
```bash
# 运行所有测试
./zygl3_tests

# 使用Google Test过滤器运行特定测试
./zygl3_tests --gtest_filter="Unit2Test.*"

# 运行单个测试用例
./zygl3_tests --gtest_filter="Unit2Test.TC_2_1_ChassisSaveAndQuery"

# 显示详细输出
./zygl3_tests --gtest_color=yes
```

### 3. 使用CTest运行（推荐）
```bash
cd build
ctest --output-on-failure
```

## 已实现的测试

### 单元2: 资源数据存取单元
- ✅ TC-2.1: 机箱保存和查询
- ✅ TC-2.2: 板卡更新
- ✅ TC-2.3: 并发访问安全
- ✅ TC-2.4: 获取所有机箱
- ✅ TC-2.5: 查询不存在的机箱
- ✅ TC-2.6: 保存空指针
- ✅ TC-2.7: 更新不存在的板卡

## 待实现的测试

参考 [TEST_CASES.md](../docs/TEST_CASES.md) 中的详细测试用例设计：

- [ ] 单元1测试（资源采集单元）
- [ ] 单元3测试（业务采集单元）
- [ ] 单元4测试（业务数据存取单元）
- [ ] 单元5测试（前端组播交互单元）
- [ ] 单元6测试（命令处理和数据组合单元）
- [ ] 集成测试

## 测试覆盖率

运行测试覆盖率检查（需要安装gcov/lcov）：

```bash
# 编译时启用覆盖率
cd tests/build
cmake .. -DCMAKE_BUILD_TYPE=Coverage
make
./zygl3_tests

# 生成覆盖率报告
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

## 参考文档

- 详细测试用例设计: [TEST_CASES.md](../docs/TEST_CASES.md)
- 单元架构说明: [UNIT_ARCHITECTURE.md](../docs/UNIT_ARCHITECTURE.md)
- Google Test文档: https://google.github.io/googletest/
