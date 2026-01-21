/**
 * @file ac_timing_test.cpp
 * @brief DDR4 AC Timing Checker 测试模块
 * 
 * 本文件实现了DDR4 AC Timing约束的验证测试。
 * 测试五类核心命令的时序约束：RD, WR, ACT, PRE, REF
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstring>

#include <systemc>
#include <tlm>

// DRAMSys headers
#include "DRAMSys/configuration/Configuration.h"
#include "DRAMSys/configuration/memspec/MemSpecDDR4.h"
#include "DRAMSys/controller/checker/CheckerDDR4.h"
#include "DRAMSys/controller/Command.h"
#include "DRAMSys/common/dramExtensions.h"
#include "DRAMSys/simulation/DRAMSys.h"

using namespace sc_core;
using namespace tlm;
using namespace DRAMSys;

//============================================================================
// 测试辅助类
//============================================================================

class ACTimingTester
{
public:
    ACTimingTester(const Configuration& config)
        : checker(config),
          memSpec(dynamic_cast<const MemSpecDDR4*>(config.memSpec.get())),
          testsPassed(0),
          testsFailed(0)
    {
        if (memSpec == nullptr)
        {
            std::cout << "[WARNING] MemSpec is not DDR4, some tests may not work correctly." << std::endl;
        }
        else
        {
            printTimingParameters();
        }
    }
    
    void runAllTests()
    {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "DDR4 AC Timing Checker 测试开始" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        if (memSpec == nullptr)
        {
            std::cout << "[ERROR] 需要DDR4配置才能运行测试!" << std::endl;
            return;
        }
        
        testACTConstraints();
        testRDConstraints();
        testWRConstraints();
        testPREConstraints();
        testREFConstraints();
        
        printTestSummary();
    }
    
private:
    CheckerDDR4 checker;
    const MemSpecDDR4* memSpec;
    int testsPassed;
    int testsFailed;
    std::vector<tlm_generic_payload*> payloads;
    std::vector<unsigned char*> dataBuffers;
    
    tlm_generic_payload* createPayload(Rank rank, BankGroup bankGroup, Bank bank,
                                        Row row = Row(0), Column column = Column(0),
                                        unsigned burstLength = 8)
    {
        auto* payload = new tlm_generic_payload();
        
        // 分配数据缓冲区
        unsigned char* data = new unsigned char[64];
        memset(data, 0, 64);
        
        payload->set_address(0);
        payload->set_data_ptr(data);
        payload->set_data_length(64);
        payload->set_streaming_width(64);
        payload->set_byte_enable_ptr(nullptr);
        payload->set_byte_enable_length(0);
        payload->set_command(TLM_READ_COMMAND);
        payload->set_response_status(TLM_INCOMPLETE_RESPONSE);
        
        // 使用setExtension而不是setAutoExtension，避免内存管理器问题
        ControllerExtension::setExtension(*payload, 0, rank, bankGroup, bank,
                                           row, column, burstLength);
        payloads.push_back(payload);
        dataBuffers.push_back(data);
        return payload;
    }
    
    void cleanupPayloads()
    {
        for (auto* p : payloads)
        {
            // 手动释放扩展
            auto* ext = p->get_extension<ControllerExtension>();
            if (ext) {
                p->clear_extension(ext);
                delete ext;
            }
            delete p;
        }
        for (auto* d : dataBuffers) delete[] d;
        payloads.clear();
        dataBuffers.clear();
    }
    
    void printTimingParameters()
    {
        std::cout << "\n========== DDR4 时序参数 ==========" << std::endl;
        std::cout << "tCK:     " << memSpec->tCK << std::endl;
        std::cout << "tRCD:    " << memSpec->tRCD << std::endl;
        std::cout << "tRAS:    " << memSpec->tRAS << std::endl;
        std::cout << "tRP:     " << memSpec->tRP << std::endl;
        std::cout << "tRC:     " << memSpec->tRC << std::endl;
        std::cout << "tRRD_S:  " << memSpec->tRRD_S << std::endl;
        std::cout << "tRRD_L:  " << memSpec->tRRD_L << std::endl;
        std::cout << "tCCD_S:  " << memSpec->tCCD_S << std::endl;
        std::cout << "tCCD_L:  " << memSpec->tCCD_L << std::endl;
        std::cout << "tWTR_S:  " << memSpec->tWTR_S << std::endl;
        std::cout << "tWTR_L:  " << memSpec->tWTR_L << std::endl;
        std::cout << "tRTP:    " << memSpec->tRTP << std::endl;
        std::cout << "tWR:     " << memSpec->tWR << std::endl;
        std::cout << "tRFC:    " << memSpec->tRFC << std::endl;
        std::cout << "tFAW:    " << memSpec->tFAW << std::endl;
        std::cout << "==================================\n" << std::endl;
    }
    
    void verifyTiming(const std::string& testName, sc_time expected, sc_time actual)
    {
        bool passed = (actual >= expected);
        
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
        std::cout << "Expected >= " << expected << ", Got: " << actual << std::endl;
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
        cleanupPayloads();
    }
    
    //========================================================================
    // ACT命令测试
    //========================================================================
    void testACTConstraints()
    {
        std::cout << "\n--- ACT命令约束测试 ---" << std::endl;
        
        // 测试1: 同Bank的ACT到ACT (tRC)
        {
            auto* payload = createPayload(Rank(0), BankGroup(0), Bank(0));
            checker.insert(Command::ACT, *payload);
            sc_time firstACT = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::ACT, *payload);
            verifyTiming("ACT->ACT (同Bank, tRC)", firstACT + memSpec->tRC, earliest);
        }
        
        // 测试2: 同BankGroup不同Bank的ACT到ACT (tRRD_L)
        {
            auto* p1 = createPayload(Rank(0), BankGroup(0), Bank(0));
            auto* p2 = createPayload(Rank(0), BankGroup(0), Bank(1));
            checker.insert(Command::ACT, *p1);
            sc_time t1 = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::ACT, *p2);
            verifyTiming("ACT->ACT (同BankGroup, tRRD_L)", t1 + memSpec->tRRD_L, earliest);
        }
        
        // 测试3: 不同BankGroup的ACT到ACT (tRRD_S)
        {
            auto* p1 = createPayload(Rank(0), BankGroup(0), Bank(0));
            auto* p2 = createPayload(Rank(0), BankGroup(1), Bank(0));
            checker.insert(Command::ACT, *p1);
            sc_time t1 = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::ACT, *p2);
            verifyTiming("ACT->ACT (不同BankGroup, tRRD_S)", t1 + memSpec->tRRD_S, earliest);
        }
        
        // 测试4: PRE到ACT (tRP)
        {
            auto* payload = createPayload(Rank(0), BankGroup(0), Bank(0));
            checker.insert(Command::PREPB, *payload);
            sc_time preTime = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::ACT, *payload);
            verifyTiming("PRE->ACT (tRP)", preTime + memSpec->tRP, earliest);
        }
    }

    //========================================================================
    // RD命令测试
    //========================================================================
    void testRDConstraints()
    {
        std::cout << "\n--- RD命令约束测试 ---" << std::endl;
        
        // 测试1: ACT到RD (tRCD)
        {
            auto* payload = createPayload(Rank(0), BankGroup(0), Bank(0));
            checker.insert(Command::ACT, *payload);
            sc_time actTime = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::RD, *payload);
            verifyTiming("ACT->RD (tRCD)", actTime + memSpec->tRCD - memSpec->tAL, earliest);
        }
        
        // 测试2: 同BankGroup的RD到RD (tCCD_L)
        {
            auto* p1 = createPayload(Rank(0), BankGroup(0), Bank(0));
            auto* p2 = createPayload(Rank(0), BankGroup(0), Bank(1));
            
            checker.insert(Command::ACT, *p1);
            sc_start(memSpec->tRRD_L);
            checker.insert(Command::ACT, *p2);
            sc_start(memSpec->tRCD);
            
            checker.insert(Command::RD, *p1);
            sc_time rdTime = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::RD, *p2);
            verifyTiming("RD->RD (同BankGroup, tCCD_L)", rdTime + memSpec->tCCD_L, earliest);
        }
        
        // 测试3: 不同BankGroup的RD到RD (tCCD_S)
        {
            auto* p1 = createPayload(Rank(0), BankGroup(0), Bank(0));
            auto* p2 = createPayload(Rank(0), BankGroup(1), Bank(0));
            
            checker.insert(Command::ACT, *p1);
            sc_start(memSpec->tRRD_S);
            checker.insert(Command::ACT, *p2);
            sc_start(memSpec->tRCD);
            
            checker.insert(Command::RD, *p1);
            sc_time rdTime = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::RD, *p2);
            verifyTiming("RD->RD (不同BankGroup, tCCD_S)", rdTime + memSpec->tCCD_S, earliest);
        }
    }
    
    //========================================================================
    // WR命令测试
    //========================================================================
    void testWRConstraints()
    {
        std::cout << "\n--- WR命令约束测试 ---" << std::endl;
        
        // 测试1: ACT到WR (tRCD)
        {
            auto* payload = createPayload(Rank(0), BankGroup(0), Bank(0));
            checker.insert(Command::ACT, *payload);
            sc_time actTime = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::WR, *payload);
            verifyTiming("ACT->WR (tRCD)", actTime + memSpec->tRCD - memSpec->tAL, earliest);
        }
        
        // 测试2: 同BankGroup的WR到WR (tCCD_L)
        {
            auto* p1 = createPayload(Rank(0), BankGroup(0), Bank(0));
            auto* p2 = createPayload(Rank(0), BankGroup(0), Bank(1));
            
            checker.insert(Command::ACT, *p1);
            sc_start(memSpec->tRRD_L);
            checker.insert(Command::ACT, *p2);
            sc_start(memSpec->tRCD);
            
            checker.insert(Command::WR, *p1);
            sc_time wrTime = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::WR, *p2);
            verifyTiming("WR->WR (同BankGroup, tCCD_L)", wrTime + memSpec->tCCD_L, earliest);
        }
        
        // 测试3: 不同BankGroup的WR到WR (tCCD_S)
        {
            auto* p1 = createPayload(Rank(0), BankGroup(0), Bank(0));
            auto* p2 = createPayload(Rank(0), BankGroup(1), Bank(0));
            
            checker.insert(Command::ACT, *p1);
            sc_start(memSpec->tRRD_S);
            checker.insert(Command::ACT, *p2);
            sc_start(memSpec->tRCD);
            
            checker.insert(Command::WR, *p1);
            sc_time wrTime = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::WR, *p2);
            verifyTiming("WR->WR (不同BankGroup, tCCD_S)", wrTime + memSpec->tCCD_S, earliest);
        }
    }
    
    //========================================================================
    // PRE命令测试
    //========================================================================
    void testPREConstraints()
    {
        std::cout << "\n--- PRE命令约束测试 ---" << std::endl;
        
        // 测试1: ACT到PRE (tRAS)
        {
            auto* payload = createPayload(Rank(0), BankGroup(0), Bank(0));
            checker.insert(Command::ACT, *payload);
            sc_time actTime = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::PREPB, *payload);
            verifyTiming("ACT->PRE (tRAS)", actTime + memSpec->tRAS, earliest);
        }
        
        // 测试2: RD到PRE (tRTP)
        {
            auto* payload = createPayload(Rank(0), BankGroup(0), Bank(0));
            checker.insert(Command::ACT, *payload);
            sc_start(memSpec->tRCD);
            
            checker.insert(Command::RD, *payload);
            sc_time rdTime = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::PREPB, *payload);
            verifyTiming("RD->PRE (tRTP)", rdTime + memSpec->tAL + memSpec->tRTP, earliest);
        }
    }
    
    //========================================================================
    // REF命令测试
    //========================================================================
    void testREFConstraints()
    {
        std::cout << "\n--- REF命令约束测试 ---" << std::endl;
        
        // 测试1: PRE到REF (tRP)
        {
            auto* payload = createPayload(Rank(0), BankGroup(0), Bank(0));
            checker.insert(Command::PREAB, *payload);
            sc_time preTime = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::REFAB, *payload);
            verifyTiming("PRE->REF (tRP)", preTime + memSpec->tRP, earliest);
        }
        
        // 测试2: REF到REF (tRFC)
        {
            auto* payload = createPayload(Rank(0), BankGroup(0), Bank(0));
            checker.insert(Command::PREAB, *payload);
            sc_start(memSpec->tRP);
            
            checker.insert(Command::REFAB, *payload);
            sc_time refTime = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::REFAB, *payload);
            verifyTiming("REF->REF (tRFC)", refTime + memSpec->tRFC, earliest);
        }
        
        // 测试3: REF到ACT (tRFC)
        {
            auto* payload = createPayload(Rank(0), BankGroup(0), Bank(0));
            checker.insert(Command::PREAB, *payload);
            sc_start(memSpec->tRP);
            
            checker.insert(Command::REFAB, *payload);
            sc_time refTime = sc_time_stamp();
            sc_start(1, SC_NS);
            
            sc_time earliest = checker.timeToSatisfyConstraints(Command::ACT, *payload);
            verifyTiming("REF->ACT (tRFC)", refTime + memSpec->tRFC, earliest);
        }
    }
};

//============================================================================
// 公共接口函数
//============================================================================

bool run_ac_timing_tests(const Configuration& config)
{
    try
    {
        ACTimingTester tester(config);
        tester.runAllTests();
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "AC Timing测试异常: " << e.what() << std::endl;
        return false;
    }
}

bool run_ac_timing_tests(DRAMSys::DRAMSys* dramSys)
{
    if (dramSys == nullptr)
    {
        std::cerr << "Error: DRAMSys pointer is null!" << std::endl;
        return false;
    }
    return run_ac_timing_tests(dramSys->getConfig());
}
