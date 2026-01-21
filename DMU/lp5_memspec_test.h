/**
 * @file lp5_memspec_test.h
 * @brief LPDDR5 MemSpec 单元测试模块头文件
 * 
 * 测试MemSpecLPDDR5类的参数加载、默认值设置和Bank结构配置
 */

#ifndef LP5_MEMSPEC_TEST_H
#define LP5_MEMSPEC_TEST_H

namespace DRAMSys {
    class Configuration;
    class DRAMSys;
}

/**
 * @brief 运行LPDDR5 MemSpec单元测试
 * @param config DRAMSys配置对象
 * @return 测试是否全部通过
 */
bool run_lp5_memspec_tests(const DRAMSys::Configuration& config);

/**
 * @brief 运行LPDDR5 MemSpec单元测试（从DRAMSys实例获取配置）
 * @param dramSys DRAMSys实例指针
 * @return 测试是否全部通过
 */
bool run_lp5_memspec_tests(DRAMSys::DRAMSys* dramSys);

#endif // LP5_MEMSPEC_TEST_H
