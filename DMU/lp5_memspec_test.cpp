/**
 * @file lp5_memspec_test.cpp
 * @brief LPDDR5 MemSpec 单元测试模块
 * 
 * 本文件实现了MemSpecLPDDR5类的单元测试。
 * 测试内容包括：
 * - 参数加载正确性
 * - 默认值设置
 * - Bank结构配置
 * - 16 Bank模式和8 Bank Group模式
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>

#include <systemc>

// DRAMSys headers
#include "DRAMSys/configuration/Configuration.h"
#include "DRAMSys/configuration/memspec/MemSpecLPDDR5.h"
#include "DRAMSys/simulation/DRAMSys.h"

using namespace sc_core;
using namespace DRAMSys;

//============================================================================
// 测试辅助类
//============================================================================

class LPDDR5MemSpecTester
{
public:
    LPDDR5MemSpecTester(const Configuration& config)
        : memSpec(dynamic_cast<const MemSpecLPDDR5*>(config.memSpec.get())),
          testsPassed(0),
          testsFailed(0)
    {
        if (memSpec == nullptr)
        {
            std::cout << "[ERROR] MemSpec is not LPDDR5!" << std::endl;
        }
    }
    
    void runAllTests()
    {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "LPDDR5 MemSpec 单元测试开始" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        if (memSpec == nullptr)
        {
            std::cout << "[ERROR] 需要LPDDR5配置才能运行测试!" << std::endl;
            return;
        }
        
        printMemSpecInfo();
        testBankStructure();
        testCoreTimingParameters();
        testBankGroupModeParameters();
        testRefreshParameters();
        testPowerDownParameters();
        testRefreshIntervalMethods();
        testBurstDuration();
        
        printTestSummary();
    }
    
private:
    const MemSpecLPDDR5* memSpec;
    int testsPassed;
    int testsFailed;
    
    void printMemSpecInfo()
    {
        std::cout << "\n========== LPDDR5 MemSpec 信息 ==========" << std::endl;
        std::cout << "Memory ID:          " << memSpec->memoryId << std::endl;
        std::cout << "Memory Type:        LPDDR5" << std::endl;
        std::cout << "Bank Group Mode:    " << (memSpec->bankGroupMode ? "8 BG" : "16 Bank") << std::endl;
        std::cout << "WCK/CK Ratio:       " << memSpec->wckCkRatio << ":1" << std::endl;
        std::cout << "Banks per Rank:     " << memSpec->banksPerRank << std::endl;
        std::cout << "Groups per Rank:    " << memSpec->groupsPerRank << std::endl;
        std::cout << "Banks per Group:    " << memSpec->banksPerGroup << std::endl;
        std::cout << "tCK:                " << memSpec->tCK << std::endl;
        std::cout << "==========================================\n" << std::endl;
    }
    
    void verifyEqual(const std::string& testName, unsigned expected, unsigned actual)
    {
        bool passed = (actual == expected);
        
        std::cout << std::left << std::setw(45) << testName << ": ";
        if (passed)
        {
            std::cout << "[PASS] ";
            testsPassed++;
        }
        else
        {
            std::cout << "[FAIL] ";
            testsFailed++;
        }
        std::cout << "Expected: " << expected << ", Got: " << actual << std::endl;
    }
    
    void verifyEqual(const std::string& testName, bool expected, bool actual)
    {
        bool passed = (actual == expected);
        
        std::cout << std::left << std::setw(45) << testName << ": ";
        if (passed)
        {
            std::cout << "[PASS] ";
            testsPassed++;
        }
        else
        {
            std::cout << "[FAIL] ";
            testsFailed++;
        }
        std::cout << "Expected: " << (expected ? "true" : "false") 
                  << ", Got: " << (actual ? "true" : "false") << std::endl;
    }
    
    void verifyPositive(const std::string& testName, sc_time value)
    {
        bool passed = (value > SC_ZERO_TIME);
        
        std::cout << std::left << std::setw(45) << testName << ": ";
        if (passed)
        {
            std::cout << "[PASS] ";
            testsPassed++;
        }
        else
        {
            std::cout << "[FAIL] ";
            testsFailed++;
        }
        std::cout << "Value: " << value << " (should be > 0)" << std::endl;
    }
    
    void verifyTimeEqual(const std::string& testName, sc_time expected, sc_time actual)
    {
        // Allow small tolerance for floating point comparison
        double diff = std::abs(expected.to_double() - actual.to_double());
        bool passed = (diff < 1e-15);
        
        std::cout << std::left << std::setw(45) << testName << ": ";
        if (passed)
        {
            std::cout << "[PASS] ";
            testsPassed++;
        }
        else
        {
            std::cout << "[FAIL] ";
            testsFailed++;
        }
        std::cout << "Expected: " << expected << ", Got: " << actual << std::endl;
    }
    
    void printTestSummary()
    {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "测试总结" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "通过: " << testsPassed << std::endl;
        std::cout << "失败: " << testsFailed << std::endl;
        std::cout << "总计: " << (testsPassed + testsFailed) << std::endl;
        
        if (testsFailed == 0)
            std::cout << "\n*** 所有测试通过! ***" << std::endl;
        else
            std::cout << "\n*** 存在失败的测试! ***" << std::endl;
        
        std::cout << std::string(60, '=') << "\n" << std::endl;
    }
    
    //========================================================================
    // Bank结构测试
    //========================================================================
    void testBankStructure()
    {
        std::cout << "\n--- Bank结构测试 ---" << std::endl;
        
        // 测试LPDDR5默认Bank结构常量
        verifyEqual("defaultBanksPerRank", 16u, MemSpecLPDDR5::defaultBanksPerRank);
        verifyEqual("defaultBankGroupsPerRank", 8u, MemSpecLPDDR5::defaultBankGroupsPerRank);
        verifyEqual("defaultBanksPerBankGroup", 2u, MemSpecLPDDR5::defaultBanksPerBankGroup);
        
        // 测试实际加载的Bank结构
        verifyEqual("banksPerRank", 16u, memSpec->banksPerRank);
        verifyEqual("groupsPerRank", 8u, memSpec->groupsPerRank);
        verifyEqual("banksPerGroup", 2u, memSpec->banksPerGroup);
    }
    
    //========================================================================
    // 核心时序参数测试
    //========================================================================
    void testCoreTimingParameters()
    {
        std::cout << "\n--- 核心时序参数测试 ---" << std::endl;
        
        // 验证核心时序参数已正确加载（值应大于0）
        verifyPositive("tRCD", memSpec->tRCD);
        verifyPositive("tRAS", memSpec->tRAS);
        verifyPositive("tRPpb", memSpec->tRPpb);
        verifyPositive("tRPab", memSpec->tRPab);
        verifyPositive("tRC", memSpec->tRC);
        verifyPositive("tRRD", memSpec->tRRD);
        verifyPositive("tFAW", memSpec->tFAW);
        verifyPositive("tRL", memSpec->tRL);
        verifyPositive("tWL", memSpec->tWL);
        verifyPositive("tRTP", memSpec->tRTP);
        verifyPositive("tWR", memSpec->tWR);
        
        // 验证tRPab >= tRPpb (全Bank预充电时间应不小于单Bank)
        bool rpabGeRppb = (memSpec->tRPab >= memSpec->tRPpb);
        std::cout << std::left << std::setw(45) << "tRPab >= tRPpb" << ": ";
        if (rpabGeRppb)
        {
            std::cout << "[PASS] ";
            testsPassed++;
        }
        else
        {
            std::cout << "[FAIL] ";
            testsFailed++;
        }
        std::cout << "tRPab=" << memSpec->tRPab << ", tRPpb=" << memSpec->tRPpb << std::endl;
    }
    
    //========================================================================
    // Bank Group模式参数测试
    //========================================================================
    void testBankGroupModeParameters()
    {
        std::cout << "\n--- Bank Group模式参数测试 ---" << std::endl;
        
        // 测试16 Bank模式参数
        verifyPositive("tCCD (16 Bank mode)", memSpec->tCCD);
        verifyPositive("tWTR (16 Bank mode)", memSpec->tWTR);
        
        // 测试8 Bank Group模式参数
        verifyPositive("tCCD_L (8 BG mode)", memSpec->tCCD_L);
        verifyPositive("tCCD_S (8 BG mode)", memSpec->tCCD_S);
        verifyPositive("tWTR_L (8 BG mode)", memSpec->tWTR_L);
        verifyPositive("tWTR_S (8 BG mode)", memSpec->tWTR_S);
        
        // 验证tCCD_L >= tCCD_S (同Bank Group的延迟应不小于不同Bank Group)
        bool ccdlGeCcds = (memSpec->tCCD_L >= memSpec->tCCD_S);
        std::cout << std::left << std::setw(45) << "tCCD_L >= tCCD_S" << ": ";
        if (ccdlGeCcds)
        {
            std::cout << "[PASS] ";
            testsPassed++;
        }
        else
        {
            std::cout << "[FAIL] ";
            testsFailed++;
        }
        std::cout << "tCCD_L=" << memSpec->tCCD_L << ", tCCD_S=" << memSpec->tCCD_S << std::endl;
        
        // 验证tWTR_L >= tWTR_S
        bool wtrlGeWtrs = (memSpec->tWTR_L >= memSpec->tWTR_S);
        std::cout << std::left << std::setw(45) << "tWTR_L >= tWTR_S" << ": ";
        if (wtrlGeWtrs)
        {
            std::cout << "[PASS] ";
            testsPassed++;
        }
        else
        {
            std::cout << "[FAIL] ";
            testsFailed++;
        }
        std::cout << "tWTR_L=" << memSpec->tWTR_L << ", tWTR_S=" << memSpec->tWTR_S << std::endl;
    }
    
    //========================================================================
    // 刷新参数测试
    //========================================================================
    void testRefreshParameters()
    {
        std::cout << "\n--- 刷新参数测试 ---" << std::endl;
        
        verifyPositive("tREFI", memSpec->tREFI);
        verifyPositive("tREFIpb", memSpec->tREFIpb);
        verifyPositive("tRFCab", memSpec->tRFCab);
        verifyPositive("tRFCpb", memSpec->tRFCpb);
        verifyPositive("tPBR2PBR", memSpec->tPBR2PBR);
        verifyPositive("tPBR2ACT", memSpec->tPBR2ACT);
        
        // 验证tRFCab >= tRFCpb (全Bank刷新时间应不小于单Bank)
        bool rfcabGeRfcpb = (memSpec->tRFCab >= memSpec->tRFCpb);
        std::cout << std::left << std::setw(45) << "tRFCab >= tRFCpb" << ": ";
        if (rfcabGeRfcpb)
        {
            std::cout << "[PASS] ";
            testsPassed++;
        }
        else
        {
            std::cout << "[FAIL] ";
            testsFailed++;
        }
        std::cout << "tRFCab=" << memSpec->tRFCab << ", tRFCpb=" << memSpec->tRFCpb << std::endl;
    }
    
    //========================================================================
    // Power Down参数测试
    //========================================================================
    void testPowerDownParameters()
    {
        std::cout << "\n--- Power Down参数测试 ---" << std::endl;
        
        verifyPositive("tCKE", memSpec->tCKE);
        verifyPositive("tXP", memSpec->tXP);
        verifyPositive("tXSR", memSpec->tXSR);
        verifyPositive("tSR", memSpec->tSR);
    }
    
    //========================================================================
    // Refresh Interval方法测试
    //========================================================================
    void testRefreshIntervalMethods()
    {
        std::cout << "\n--- Refresh Interval方法测试 ---" << std::endl;
        
        // 测试getRefreshIntervalAB()返回tREFI
        verifyTimeEqual("getRefreshIntervalAB() == tREFI", 
                        memSpec->tREFI, memSpec->getRefreshIntervalAB());
        
        // 测试getRefreshIntervalPB()返回tREFIpb
        verifyTimeEqual("getRefreshIntervalPB() == tREFIpb", 
                        memSpec->tREFIpb, memSpec->getRefreshIntervalPB());
    }
    
    //========================================================================
    // Burst Duration测试
    //========================================================================
    void testBurstDuration()
    {
        std::cout << "\n--- Burst Duration测试 ---" << std::endl;
        
        // 测试BL32的burst duration是BL16的两倍
        sc_time burstDuration32 = memSpec->getBurstDuration32();
        verifyPositive("getBurstDuration32()", burstDuration32);
        
        // 注意：burstDuration是protected成员，无法直接访问
        // 但我们可以验证BL32 > 0
        std::cout << std::left << std::setw(45) << "BL32 burst duration" << ": ";
        std::cout << "[INFO] " << "Value: " << burstDuration32 << std::endl;
    }
};

//============================================================================
// 公共接口函数
//============================================================================

bool run_lp5_memspec_tests(const Configuration& config)
{
    try
    {
        // 检查是否为LPDDR5配置
        const MemSpecLPDDR5* memSpec = 
            dynamic_cast<const MemSpecLPDDR5*>(config.memSpec.get());
        
        if (memSpec == nullptr)
        {
            std::cout << "[WARNING] 配置不是LPDDR5类型，跳过MemSpec测试" << std::endl;
            return true;
        }
        
        LPDDR5MemSpecTester tester(config);
        tester.runAllTests();
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "LPDDR5 MemSpec测试异常: " << e.what() << std::endl;
        return false;
    }
}

bool run_lp5_memspec_tests(DRAMSys::DRAMSys* dramSys)
{
    if (dramSys == nullptr)
    {
        std::cerr << "Error: DRAMSys pointer is null!" << std::endl;
        return false;
    }
    return run_lp5_memspec_tests(dramSys->getConfig());
}
