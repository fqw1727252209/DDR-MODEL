/**
 * @file lp5_freq_ratio_test.h
 * @brief LPDDR5 频率比测试模块头文件
 * 
 * 本文件声明了LPDDR5在不同频率比（1:1, 1:2, 1:4）下的AC Timing约束验证测试接口。
 * 验证控制器在不同频率下运行时，所有时序约束仍然得到满足。
 */

#ifndef LP5_FREQ_RATIO_TEST_H
#define LP5_FREQ_RATIO_TEST_H

#include <string>

/**
 * @brief 运行LPDDR5频率比测试
 * @param config_file_1to1 1:1频率比配置文件路径
 * @param config_file_1to2 1:2频率比配置文件路径
 * @param config_file_1to4 1:4频率比配置文件路径
 * @param resource_dir 资源目录路径
 * @return 测试是否全部通过
 */
bool run_lp5_freq_ratio_tests(const std::string& config_file_1to1,
                               const std::string& config_file_1to2,
                               const std::string& config_file_1to4,
                               const std::string& resource_dir);

/**
 * @brief 测试单个频率比配置
 * @param config_file 配置文件路径
 * @param resource_dir 资源目录路径
 * @param freq_ratio_name 频率比名称（用于日志输出）
 * @return 测试是否通过
 */
bool test_single_freq_ratio(const std::string& config_file,
                            const std::string& resource_dir,
                            const std::string& freq_ratio_name);

#endif // LP5_FREQ_RATIO_TEST_H
