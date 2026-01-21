# LPDDR5 AC Timing Checker 项目

## 📋 项目状态

✅ **LPDDR5 AC Timing Checker 已完成并验证通过**

- ✅ 支持五类核心命令：ACT, RD, WR, PRE, REF
- ✅ 支持三种频率比：1:1, 1:2, 1:4
- ✅ 54个测试用例 × 3种配置 = 162个测试全部通过
- ✅ Property-Based Testing验证（1000+次迭代）
- ✅ 支持Power Down和Self Refresh命令
- ✅ 支持16 Bank和8 Bank Group模式

## 📖 文档导航

### 主要文档

1. **[LPDDR5_AC_Timing_Checker_完整设计文档.md](LPDDR5_AC_Timing_Checker_完整设计文档.md)** ⭐
   - **最重要的文档**，包含完整的项目信息
   - 项目概述、设计思路、实现细节
   - 验证方法、问题排查、使用指南
   - 建议阅读时间：60分钟

2. **[FREQ_RATIO_TEST_GUIDE.md](FREQ_RATIO_TEST_GUIDE.md)**
   - 频率比测试使用指南
   - 测试配置说明
   - 故障排查方法

## 🚀 快速开始

### 编译项目

```bash
cd DDR-MODEL
mkdir -p build
cd build
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..
make -j8
cd ..
```

### 运行测试

```bash
# 方法1：使用验证脚本（推荐）
bash test_verification.sh

# 方法2：使用频率比测试脚本
bash run_freq_ratio_test.sh

# 方法3：使用完整测试套件
bash run_all_freq_ratio_tests.sh

# 方法4：手动测试单个配置
cd build
./bin/dmutest FREQ_RATIO_TEST ../lib/DRAMsys/configs/lpddr5-example.json ../lib/DRAMsys/configs
cd ..
```

### 预期结果

所有测试应该显示：
```
======================================================================
测试总结
======================================================================
通过: 54
失败: 0
总计: 54

*** 所有测试通过! ***
======================================================================
```

## 📁 项目结构

```
DDR-MODEL/
├── build/                          # 编译产物和临时文件
│   ├── bin/dmutest                # 测试可执行文件
│   └── *.tdb                      # DRAMSys trace database
├── logs/                          # 测试日志
│   └── *.log                      # 测试运行日志
├── lib/
│   └── DRAMsys/                   # DRAMSys仿真器
│       ├── configs/               # 配置文件
│       │   ├── memspec/          # 内存规格配置
│       │   │   ├── JEDEC_LPDDR5-6400.json        # 1:1配置
│       │   │   ├── JEDEC_LPDDR5-6400_1to2.json   # 1:2配置
│       │   │   └── JEDEC_LPDDR5-6400_1to4.json   # 1:4配置
│       │   ├── lpddr5-example.json               # 1:1测试配置
│       │   ├── lpddr5-1to2-example.json          # 1:2测试配置
│       │   └── lpddr5-1to4-example.json          # 1:4测试配置
│       └── src/libdramsys/DRAMSys/
│           ├── configuration/memspec/
│           │   ├── MemSpecLPDDR5.h              # 频率比支持
│           │   └── MemSpecLPDDR5.cpp
│           └── controller/checker/
│               ├── CheckerLPDDR5.h              # AC Timing检查
│               └── CheckerLPDDR5.cpp            # 921行实现
├── DMU/
│   ├── lp5_ac_timing_test.cpp     # AC Timing测试（1734行）
│   ├── lp5_freq_ratio_test.cpp    # 频率比测试
│   └── main.cc                    # 测试入口
├── LPDDR5_AC_Timing_Checker_完整设计文档.md  # 主文档 ⭐
├── FREQ_RATIO_TEST_GUIDE.md       # 测试指南
├── test_verification.sh           # 验证脚本
├── run_freq_ratio_test.sh         # 频率比测试脚本
└── run_all_freq_ratio_tests.sh    # 完整测试套件
```

## 🔧 技术特性

### 1. 时间域分离设计
- DRAM时间域：所有时序参数使用DRAM时钟周期定义
- Controller时间域：Controller调度使用Controller时钟周期
- 自动转换：Checker自动处理两个时间域之间的转换

### 2. 向上取整策略
```cpp
// 确保满足最小时序要求
controller_cycles = ceil(dram_cycles / ratio)
```

### 3. Property-Based Testing
- 10个属性测试 × 100次随机迭代 = 1000次验证
- 自动生成随机Bank、BankGroup、Rank组合
- 高置信度保证功能正确

### 4. 多频率比支持

| 配置 | 频率比 | DRAM频率 | Controller频率 | 测试结果 |
|------|--------|----------|----------------|----------|
| 基准 | 1:1 | 1600 MHz | 1600 MHz | ✅ 54/54 |
| 半频 | 1:2 | 1600 MHz | 800 MHz | ✅ 54/54 |
| 四分频 | 1:4 | 1600 MHz | 400 MHz | ✅ 54/54 |

## 📊 测试覆盖率

### 命令类型覆盖
- ✅ ACT (Activate)
- ✅ RD (Read) / RDA (Read with Auto-precharge)
- ✅ WR (Write) / WRA (Write with Auto-precharge)
- ✅ MWR (Masked Write) / MWRA
- ✅ PREPB (Per-Bank Precharge) / PREAB (All-Bank Precharge)
- ✅ REFPB (Per-Bank Refresh) / REFAB (All-Bank Refresh)
- ✅ PDEA/PDEP/PDXA/PDXP (Power Down)
- ✅ SREFEN/SREFEX (Self Refresh)

### 时序参数覆盖
共验证30+个LPDDR5时序参数：
- tCK, tRCD, tRAS, tRPpb, tRPab, tRC, tRRD, tFAW
- tCCD, tCCD_L, tCCD_S
- tWTR, tWTR_L, tWTR_S
- tRTP, tWR, tRFCab, tRFCpb, tPBR2PBR
- tXP, tXSR, tCKE, tPPD, tRTRS, tCCDMW
- 以及更多...

## 🛠️ 环境要求

- **操作系统**：Linux或WSL (Windows Subsystem for Linux)
- **编译器**：GCC 9.0+（支持C++17和std::filesystem）
- **SystemC**：2.3.3或更高版本
- **CMake**：3.10或更高版本

## 📝 使用说明

### 清理临时文件

```bash
# 清理 build 目录
cd build && make clean && cd ..

# 清理 .tdb 文件
rm -f build/*.tdb
```

### 查看测试日志

```bash
# 查看最新的测试日志
ls -lt logs/*.log | head -1

# 查看特定配置的日志
cat logs/sim_FREQ_RATIO_TEST_*.log
```

## ⚠️ 注意事项

1. **不要在根目录直接运行 dmutest**
   - 使用提供的测试脚本
   - 或在 build 目录下运行

2. **临时文件位置**
   - `.tdb` 文件会生成在 `build/` 目录
   - `.log` 文件会生成在 `logs/` 目录
   - 不会污染项目根目录

3. **定期清理**
   - `.tdb` 文件会累积
   - 定期运行 `make clean` 清理

## 📞 问题反馈

如有问题或建议，请查看 [LPDDR5_AC_Timing_Checker_完整设计文档.md](LPDDR5_AC_Timing_Checker_完整设计文档.md) 中的问题排查章节。

---

## 📚 原始项目说明

### 代码集成说明
DDR的模型由两部分组成：自写CHIPort和DRAMSys，此二者已经进行封装在一起，可以只用DMU中的"DramMannageUnit"类进行调用。样例实现可以参照DMU目录下的"main.cc"文件。

### 依赖库配置

#### SystemC
```bash
export LD_LIBRARY_PATH=/opt/systemc-2/lib:$LD_LIBRARY_PATH
```

#### 标准文件系统库
由于gcc8.3.1的标准库中并没有集成文件系统 std::filesystem，要到gcc9之上才能支持，所以在编译链接时需要额外指定标准库stdc++fs：
```makefile
-lstdc++fs
```

#### DRAMSys配置文件目录
需要指定DRAMSys的配置文件目录，使用宏指定：
```makefile
DRAMSYS_RESOURCE_DIR = "你的路径"
# 例如："/cloud/home/liujungan1756/software/DDR_TLM/lib/DRAMSys/configs"
# 必须要指定，否则DRAMSys无法找到配置文件，会报错
```

---

**项目版本**：v1.0  
**最后更新**：2026年1月22日  
**状态**：生产就绪 ✅
