/**
 * @file ac_timing_test.h
 * @brief DDR4 AC Timing Checker 测试模块头文件
 */

#ifndef AC_TIMING_TEST_H
#define AC_TIMING_TEST_H

namespace DRAMSys {
    class Configuration;
    class DRAMSys;
}

/**
 * @brief 运行DDR4 AC Timing测试
 * @param config DRAMSys配置对象
 * @return 测试是否全部通过
 */
bool run_ac_timing_tests(const DRAMSys::Configuration& config);

/**
 * @brief 运行DDR4 AC Timing测试（从DRAMSys实例获取配置）
 * @param dramSys DRAMSys实例指针
 * @return 测试是否全部通过
 */
bool run_ac_timing_tests(DRAMSys::DRAMSys* dramSys);

#endif // AC_TIMING_TEST_H
