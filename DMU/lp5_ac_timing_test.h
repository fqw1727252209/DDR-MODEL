/**
 * @file lp5_ac_timing_test.h
 * @brief LPDDR5 AC Timing Checker 测试模块头文件
 * 
 * 本文件声明了LPDDR5 AC Timing约束的验证测试接口。
 * 测试五类核心命令的时序约束：RD, WR, ACT, PRE, REF
 * 支持16 Bank模式和8 Bank Group模式
 */

#ifndef LP5_AC_TIMING_TEST_H
#define LP5_AC_TIMING_TEST_H

// Include necessary headers instead of forward declarations to avoid namespace conflicts
#include "DRAMSys/configuration/Configuration.h"
#include "DRAMSys/simulation/DRAMSys.h"

/**
 * @brief 运行LPDDR5 AC Timing测试
 * @param config DRAMSys配置对象
 * @return 测试是否全部通过
 */
bool run_lp5_ac_timing_tests(const DRAMSys::Configuration& config);

/**
 * @brief 运行LPDDR5 AC Timing测试（使用DRAMSys实例）
 * @param dramSys DRAMSys实例指针
 * @return 测试是否全部通过
 */
bool run_lp5_ac_timing_tests(DRAMSys::DRAMSys* dramSys);

#endif // LP5_AC_TIMING_TEST_H
