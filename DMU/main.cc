#include <iostream>
#include <fstream>
#include <ctime>
#include <sstream>
#include <iomanip>

#include "CHIPort/CHITrafficGenerator.h"
#include "CHIPort/CHIMonitor.h"


#include "DMU/DramMannageUnit.h"
#include "DMU/d4_ac_timing_test.h"  // DDR4 AC Timing测试头文件
#include "lp5_ac_timing_test.h"     // LPDDR5 AC Timing测试头文件
#include "lp5_freq_ratio_test.h"    // LPDDR5 频率比测试头文件

#include <DRAMSys/config/DRAMSysConfiguration.h>
#include <DRAMSys/configuration/memspec/MemSpec.h>

#include <filesystem>

#include <random>
#include <cmath>
#include <DRAMSys/simulation/AddressDecoder.h>

//============================================================================
// 流量模式枚举
//============================================================================
enum class TrafficPattern
{
    SEQUENTIAL,          // 连续地址访问（Row Buffer友好）
    RANDOM,              // 随机地址访问
    SAME_BANK_DIFF_ROW,  // 同Bank不同Row（测试Row切换）
    DIFF_BANK_SAME_ROW,  // 不同Bank同Row（测试Bank并行）
    STRIDED,             // 固定步长访问
    MIXED_RW,            // 读写混合
    AC_TIMING_TEST,      // AC Timing约束测试
    FREQ_RATIO_TEST      // 频率比测试
};

// 获取Pattern名称的全局函数
const char* get_pattern_name(TrafficPattern p)
{
    switch (p)
    {
        case TrafficPattern::SEQUENTIAL:         return "SEQUENTIAL";
        case TrafficPattern::RANDOM:             return "RANDOM";
        case TrafficPattern::SAME_BANK_DIFF_ROW: return "SAME_BANK_DIFF_ROW";
        case TrafficPattern::DIFF_BANK_SAME_ROW: return "DIFF_BANK_SAME_ROW";
        case TrafficPattern::STRIDED:            return "STRIDED";
        case TrafficPattern::MIXED_RW:           return "MIXED_RW";
        case TrafficPattern::AC_TIMING_TEST:     return "AC_TIMING_TEST";
        case TrafficPattern::FREQ_RATIO_TEST:    return "FREQ_RATIO_TEST";
        default: return "UNKNOWN";
    }
}

//============================================================================
// 动态地址映射参数 (从DRAMSys配置自动获取)
// 支持DDR4、LPDDR4等不同内存类型
//============================================================================
struct AddressMappingParams
{
    uint64_t baseAddr = 0;
    unsigned byteBits = 0;
    unsigned columnBits = 0;
    unsigned rowBits = 0;
    unsigned bankBits = 0;
    unsigned bankGroupBits = 0;
    unsigned rankBits = 0;
    
    // 计算得出的偏移量和大小
    uint64_t pageSize = 0;      // 一个Row的大小
    uint64_t bankSize = 0;      // 一个Bank的大小
    unsigned numBanks = 0;
    unsigned numBankGroups = 0;
    unsigned numRows = 0;
    unsigned numColumns = 0;
    
    // 用于地址生成的掩码
    unsigned rowMask = 0;
    unsigned columnMask = 0;
    unsigned bankMask = 0;
    unsigned bankGroupMask = 0;
    
    // 从MemSpec初始化
    void initFromMemSpec(const DRAMSys::MemSpec& memSpec)
    {
        // 从memSpec获取位宽信息
        numBanks = memSpec.banksPerGroup;
        numBankGroups = memSpec.groupsPerRank;
        numRows = memSpec.rowsPerBank;
        numColumns = memSpec.columnsPerRow;
        
        // 计算各字段的位数
        byteBits = static_cast<unsigned>(std::log2(memSpec.bytesPerBeat));
        columnBits = static_cast<unsigned>(std::log2(numColumns));
        rowBits = static_cast<unsigned>(std::log2(numRows));
        bankBits = static_cast<unsigned>(std::log2(numBanks));
        bankGroupBits = static_cast<unsigned>(std::log2(numBankGroups));
        
        // 计算大小
        pageSize = static_cast<uint64_t>(memSpec.bytesPerBeat) * numColumns;
        bankSize = pageSize * numRows;
        
        // 计算掩码
        rowMask = (1U << rowBits) - 1;
        columnMask = (1U << columnBits) - 1;
        bankMask = (1U << bankBits) - 1;
        bankGroupMask = (1U << bankGroupBits) - 1;
    }
    
    void print() const
    {
        std::cout << "\n========== Address Mapping Parameters ==========" << std::endl;
        std::cout << "  Byte bits:       " << byteBits << std::endl;
        std::cout << "  Column bits:     " << columnBits << " (columns: " << numColumns << ")" << std::endl;
        std::cout << "  Row bits:        " << rowBits << " (rows: " << numRows << ")" << std::endl;
        std::cout << "  Bank bits:       " << bankBits << " (banks/group: " << numBanks << ")" << std::endl;
        std::cout << "  BankGroup bits:  " << bankGroupBits << " (groups: " << numBankGroups << ")" << std::endl;
        std::cout << "  Page size:       " << pageSize << " bytes (" << (pageSize / 1024) << " KB)" << std::endl;
        std::cout << "  Bank size:       " << bankSize << " bytes (" << (bankSize / (1024*1024)) << " MB)" << std::endl;
        std::cout << "================================================\n" << std::endl;
    }
};

// 全局地址映射参数（在sc_main中初始化）
static AddressMappingParams g_addrParams;

//============================================================================
// 地址生成辅助函数 - 使用AddressDecoder的encodeAddress
//============================================================================
uint64_t make_address(const DRAMSys::AddressDecoder& decoder,
                      unsigned bank_group, unsigned bank, unsigned row, unsigned column, unsigned byte_offset = 0)
{
    DRAMSys::DecodedAddress decoded;
    decoded.channel = 0;
    decoded.rank = 0;
    decoded.bankgroup = bank_group;
    decoded.bank = bank;
    decoded.row = row;
    decoded.column = column;
    decoded.byte = byte_offset;
    
    return decoder.encodeAddress(decoded);
}

// 兼容旧接口的简化版本（使用全局参数，BRC顺序）
uint64_t make_address_simple(unsigned bank_group, unsigned bank, unsigned row, unsigned column, unsigned byte_offset = 0)
{
    // 使用BRC映射顺序: Byte - Column - Row - Bank - BankGroup
    uint64_t addr = byte_offset & ((1ULL << g_addrParams.byteBits) - 1);
    addr |= static_cast<uint64_t>(column & g_addrParams.columnMask) << g_addrParams.byteBits;
    addr |= static_cast<uint64_t>(row & g_addrParams.rowMask) << (g_addrParams.byteBits + g_addrParams.columnBits);
    addr |= static_cast<uint64_t>(bank & g_addrParams.bankMask) << (g_addrParams.byteBits + g_addrParams.columnBits + g_addrParams.rowBits);
    addr |= static_cast<uint64_t>(bank_group & g_addrParams.bankGroupMask) << (g_addrParams.byteBits + g_addrParams.columnBits + g_addrParams.rowBits + g_addrParams.bankBits);
    return addr;
}

//============================================================================
// 流量生成器类 - 使用动态地址映射参数
//============================================================================
class TrafficPatternGenerator
{
public:
    TrafficPatternGenerator(unsigned seed = 42) : rng(seed), decoder(nullptr) {}
    
    // 设置AddressDecoder用于精确地址编码
    void setAddressDecoder(const DRAMSys::AddressDecoder* dec) { decoder = dec; }
    
    void generate(CHITrafficGenerator& tg, TrafficPattern pattern, 
                  unsigned num_requests, bool read_only = true)
    {
        current_pattern = pattern;
        std::cout << "\n[Traffic Pattern] " << pattern_name(pattern) 
                  << " - " << num_requests << " requests" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        switch (pattern)
        {
            case TrafficPattern::SEQUENTIAL:
                generate_sequential(tg, num_requests, read_only);
                break;
            case TrafficPattern::RANDOM:
                generate_random(tg, num_requests, read_only);
                break;
            case TrafficPattern::SAME_BANK_DIFF_ROW:
                generate_same_bank_diff_row(tg, num_requests, read_only);
                break;
            case TrafficPattern::DIFF_BANK_SAME_ROW:
                generate_diff_bank_same_row(tg, num_requests, read_only);
                break;
            case TrafficPattern::STRIDED:
                generate_strided(tg, num_requests, read_only);
                break;
            case TrafficPattern::MIXED_RW:
                generate_mixed_rw(tg, num_requests);
                break;
        }
    }
    
private:
    std::mt19937_64 rng;
    TrafficPattern current_pattern;
    const DRAMSys::AddressDecoder* decoder;
    
    const char* pattern_name(TrafficPattern p)
    {
        return get_pattern_name(p);
    }
    
    ARM::CHI::ReqOpcode get_opcode(bool is_read)
    {
        return is_read ? ARM::CHI::REQ_OPCODE_READ_NO_SNP 
                       : ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_PTL;
    }
    
    // 使用AddressDecoder生成地址（如果可用）
    uint64_t encode_address(unsigned bank_group, unsigned bank, unsigned row, unsigned column, unsigned byte = 0)
    {
        if (decoder)
        {
            return make_address(*decoder, bank_group, bank, row, column, byte);
        }
        return make_address_simple(bank_group, bank, row, column, byte);
    }
    
    // Pattern 1: 连续地址访问 - Row Buffer命中率高
    void generate_sequential(CHITrafficGenerator& tg, unsigned num_requests, bool read_only)
    {
        uint64_t base_addr = 0;
        const uint64_t step = 64; // cache line size
        
        for (unsigned i = 0; i < num_requests; ++i)
        {
            uint64_t addr = base_addr + (i * step);
            tg.add_payload(get_opcode(read_only), addr, ARM::CHI::SIZE_64);
        }
        std::cout << "  Base: 0x" << std::hex << base_addr 
                  << ", Step: " << std::dec << step << " bytes" << std::endl;
    }
    
    // Pattern 2: 随机地址访问 - Row Buffer命中率低
    void generate_random(CHITrafficGenerator& tg, unsigned num_requests, bool read_only)
    {
        uint64_t max_addr = g_addrParams.bankSize * g_addrParams.numBanks * g_addrParams.numBankGroups;
        std::uniform_int_distribution<uint64_t> addr_dist(0, max_addr - 1);
        
        for (unsigned i = 0; i < num_requests; ++i)
        {
            uint64_t addr = (addr_dist(rng) >> 6) << 6; // align to 64 bytes
            tg.add_payload(get_opcode(read_only), addr, ARM::CHI::SIZE_64);
        }
        std::cout << "  Random addresses across entire memory space (max: 0x" 
                  << std::hex << max_addr << std::dec << ")" << std::endl;
    }
    
    // Pattern 3: 同Bank不同Row - 频繁触发Row切换
    void generate_same_bank_diff_row(CHITrafficGenerator& tg, unsigned num_requests, bool read_only)
    {
        const unsigned target_bank = 0;
        const unsigned target_bank_group = 0;
        
        for (unsigned i = 0; i < num_requests; ++i)
        {
            unsigned row = i % g_addrParams.numRows;
            unsigned col = 0;
            uint64_t addr = encode_address(target_bank_group, target_bank, row, col);
            tg.add_payload(get_opcode(read_only), addr, ARM::CHI::SIZE_64);
        }
        std::cout << "  Fixed Bank: " << target_bank 
                  << ", BankGroup: " << target_bank_group
                  << ", Rotating Rows (max: " << g_addrParams.numRows << ")" << std::endl;
    }
    
    // Pattern 4: 不同Bank同Row - 利用Bank并行性
    void generate_diff_bank_same_row(CHITrafficGenerator& tg, unsigned num_requests, bool read_only)
    {
        const unsigned target_row = 100;
        
        for (unsigned i = 0; i < num_requests; ++i)
        {
            unsigned bank = i % g_addrParams.numBanks;
            unsigned bank_group = (i / g_addrParams.numBanks) % g_addrParams.numBankGroups;
            unsigned col = (i / (g_addrParams.numBanks * g_addrParams.numBankGroups)) % g_addrParams.numColumns;
            uint64_t addr = encode_address(bank_group, bank, target_row, col);
            tg.add_payload(get_opcode(read_only), addr, ARM::CHI::SIZE_64);
        }
        std::cout << "  Fixed Row: " << target_row 
                  << ", Rotating across " << (g_addrParams.numBanks * g_addrParams.numBankGroups) << " banks" << std::endl;
    }
    
    // Pattern 5: 固定步长访问
    void generate_strided(CHITrafficGenerator& tg, unsigned num_requests, bool read_only)
    {
        const uint64_t stride = g_addrParams.pageSize; // 每次跳过一个Page（触发Row切换）
        uint64_t addr = 0;
        
        for (unsigned i = 0; i < num_requests; ++i)
        {
            tg.add_payload(get_opcode(read_only), addr, ARM::CHI::SIZE_64);
            addr += stride;
        }
        std::cout << "  Stride: 0x" << std::hex << stride << std::dec 
                  << " bytes (" << (stride / 1024) << " KB)" << std::endl;
    }
    
    // Pattern 6: 读写混合
    void generate_mixed_rw(CHITrafficGenerator& tg, unsigned num_requests)
    {
        std::uniform_int_distribution<unsigned> rw_dist(0, 1);
        uint64_t max_addr = g_addrParams.bankSize * g_addrParams.numBanks * g_addrParams.numBankGroups / 4;
        std::uniform_int_distribution<uint64_t> addr_dist(0, max_addr - 1);
        
        unsigned read_count = 0, write_count = 0;
        
        for (unsigned i = 0; i < num_requests; ++i)
        {
            bool is_read = rw_dist(rng) == 0;
            uint64_t addr = (addr_dist(rng) >> 6) << 6;
            tg.add_payload(get_opcode(is_read), addr, ARM::CHI::SIZE_64);
            
            if (is_read) ++read_count;
            else ++write_count;
        }
        std::cout << "  Reads: " << read_count << ", Writes: " << write_count << std::endl;
    }
};

// 生成时间戳字符串
std::string get_timestamp_string()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// 同时输出到控制台和文件的流缓冲
class TeeBuffer : public std::streambuf
{
public:
    TeeBuffer(std::streambuf* console, std::streambuf* file)
        : console_buf(console), file_buf(file) {}
    
protected:
    int overflow(int c) override
    {
        if (c == EOF) return !EOF;
        int r1 = console_buf->sputc(c);
        int r2 = file_buf->sputc(c);
        return (r1 == EOF || r2 == EOF) ? EOF : c;
    }
    
    int sync() override
    {
        int r1 = console_buf->pubsync();
        int r2 = file_buf->pubsync();
        return (r1 == 0 && r2 == 0) ? 0 : -1;
    }
    
private:
    std::streambuf* console_buf;
    std::streambuf* file_buf;
};




// 解析命令行参数获取Pattern
TrafficPattern parse_pattern(const char* pattern_arg)
{
    std::string p(pattern_arg);
    if (p == "SEQUENTIAL" || p == "0") return TrafficPattern::SEQUENTIAL;
    if (p == "RANDOM" || p == "1") return TrafficPattern::RANDOM;
    if (p == "SAME_BANK_DIFF_ROW" || p == "2") return TrafficPattern::SAME_BANK_DIFF_ROW;
    if (p == "DIFF_BANK_SAME_ROW" || p == "3") return TrafficPattern::DIFF_BANK_SAME_ROW;
    if (p == "STRIDED" || p == "4") return TrafficPattern::STRIDED;
    if (p == "MIXED_RW" || p == "5") return TrafficPattern::MIXED_RW;
    if (p == "AC_TIMING_TEST" || p == "6") return TrafficPattern::AC_TIMING_TEST;
    if (p == "FREQ_RATIO_TEST" || p == "7") return TrafficPattern::FREQ_RATIO_TEST;
    return TrafficPattern::SEQUENTIAL; // default
}

void print_usage()
{
    std::cout << "Usage: dmutest [pattern] [config_file] [resource_dir]" << std::endl;
    std::cout << "Patterns:" << std::endl;
    std::cout << "  0 or SEQUENTIAL        - 连续地址访问" << std::endl;
    std::cout << "  1 or RANDOM            - 随机地址访问" << std::endl;
    std::cout << "  2 or SAME_BANK_DIFF_ROW - 同Bank不同Row" << std::endl;
    std::cout << "  3 or DIFF_BANK_SAME_ROW - 不同Bank同Row" << std::endl;
    std::cout << "  4 or STRIDED           - 步长访问" << std::endl;
    std::cout << "  5 or MIXED_RW          - 读写混合" << std::endl;
    std::cout << "  6 or AC_TIMING_TEST    - AC Timing约束测试" << std::endl;
    std::cout << "  7 or FREQ_RATIO_TEST   - 频率比测试（1:1, 1:2, 1:4）" << std::endl;
}

int sc_main(int argc, char** argv)
{
    try
    {
    //========================================================================
    // 解析命令行参数选择流量模式
    //========================================================================
    TrafficPattern selected_pattern = TrafficPattern::SEQUENTIAL;
    const unsigned num_requests = 20000;
    
    // 第一个参数可以是pattern类型
    if (argc >= 2)
    {
        std::string first_arg(argv[1]);
        // 如果第一个参数是数字或pattern名称，则解析为pattern
        if (first_arg.length() <= 20 && (isdigit(first_arg[0]) || isupper(first_arg[0])))
        {
            selected_pattern = parse_pattern(argv[1]);
        }
    }
    //========================================================================
    
    // 创建logs文件夹
    std::filesystem::path logDir = std::filesystem::current_path() / "logs";
    if (!std::filesystem::exists(logDir))
    {
        std::filesystem::create_directories(logDir);
    }
    
    // 生成带时间戳和pattern名称的日志文件名
    std::string timestamp = get_timestamp_string();
    std::string pattern_str = get_pattern_name(selected_pattern);
    std::filesystem::path logFile = logDir / ("sim_" + pattern_str + "_" + timestamp + ".log");
    
    // 打开日志文件
    std::ofstream logStream(logFile);
    if (!logStream.is_open())
    {
        std::cerr << "Failed to open log file: " << logFile << std::endl;
        return 1;
    }
    
    // 设置同时输出到控制台和文件
    std::streambuf* console_buf = std::cout.rdbuf();
    TeeBuffer tee_buf(console_buf, logStream.rdbuf());
    std::cout.rdbuf(&tee_buf);
    
    // Store the starting of the simulation in wall-clock time:
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "========================================" << std::endl;
    std::cout << "Log file: " << logFile << std::endl;
    std::cout << "Timestamp: " << timestamp << std::endl;
    std::cout << "Pattern: " << pattern_str << std::endl;
    std::cout << "Requests: " << num_requests << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "this is DRAMsys test" << std::endl;

    /* Configure the configure file directory and bus width */
    std::filesystem::path resourceDirectory = DRAMSYS_RESOURCE_DIR;
    // std::filesystem::path baseConfig = resourceDirectory / "lpddr4-example.json";
    std::filesystem::path baseConfig = resourceDirectory / "ddr4-example.json";
    
    
    // 解析命令行参数：第一个可以是Pattern，第二个可以是配置文件，第三个可以是资源目录
    int config_arg_offset = 1; // 配置文件参数的位置
    
    // 如果第一个参数是Pattern，则配置文件从第二个参数开始
    if (argc >= 2)
    {
        std::string first_arg(argv[1]);
        if (first_arg.length() <= 20 && (isdigit(first_arg[0]) || isupper(first_arg[0])))
        {
            config_arg_offset = 2; // Pattern在argv[1]，配置从avgv[2]开始
        }
    }
    
    if (argc >= config_arg_offset + 1)
    {
        baseConfig = argv[config_arg_offset];
    }
    if (argc >= config_arg_offset + 2)
    {
        resourceDirectory = argv[config_arg_offset + 1];
    }

    DRAMSys::Config::Configuration configuration = 
        DRAMSys::Config::from_path(baseConfig.c_str(), resourceDirectory.c_str());
    static const unsigned data_width_bits = 256;



    dmu::DramMannageUnit dmu(std::move(configuration), std::move(resourceDirectory), data_width_bits);
    
    // 从DRAMSys获取MemSpec并初始化地址映射参数
    const auto& memSpec = *dmu.dramSys->getConfig().memSpec;
    g_addrParams.initFromMemSpec(memSpec);
    g_addrParams.print();
    
    // 获取AddressDecoder用于精确地址编码
    const auto& addressDecoder = dmu.dramSys->getAddressDecoder();
    
    sc_core::sc_clock clk("clk", 2, sc_core::SC_NS, 0.5);
    CHITrafficGenerator tg("tg", data_width_bits);
    CHIMonitor mon("mon", data_width_bits);

    tg.clock.bind(clk);
    dmu.chi_port->clock.bind(clk);

    tg.initiator.bind(mon.target);
    mon.initiator.bind(dmu.chi_port->target);

    // 生成流量或运行AC Timing测试
    if (selected_pattern == TrafficPattern::AC_TIMING_TEST)
    {
        // 检测内存类型并运行相应的AC Timing测试
        const auto& memSpec = *dmu.dramSys->getConfig().memSpec;
        bool test_passed = false;
        
        if (memSpec.memoryType == DRAMSys::MemSpec::MemoryType::LPDDR5)
        {
            std::cout << "\n========================================" << std::endl;
            std::cout << "运行 LPDDR5 AC Timing 约束测试" << std::endl;
            std::cout << "========================================\n" << std::endl;
            
            test_passed = run_lp5_ac_timing_tests(dmu.dramSys.get());
        }
        else
        {
            std::cout << "\n========================================" << std::endl;
            std::cout << "运行 DDR4 AC Timing 约束测试" << std::endl;
            std::cout << "========================================\n" << std::endl;
            
            test_passed = run_ac_timing_tests(dmu.dramSys.get());
        }
        
        if (test_passed)
        {
            std::cout << "\nAC Timing测试完成!" << std::endl;
        }
        else
        {
            std::cout << "\nAC Timing测试失败!" << std::endl;
        }
        
        // AC Timing测试不需要运行仿真
        sc_core::sc_stop();
    }
    else if (selected_pattern == TrafficPattern::FREQ_RATIO_TEST)
    {
        // 运行频率比测试
        std::cout << "\n========================================" << std::endl;
        std::cout << "运行 LPDDR5 频率比测试" << std::endl;
        std::cout << "测试 1:1, 1:2, 1:4 频率比配置" << std::endl;
        std::cout << "========================================\n" << std::endl;
        
        // 检查当前配置是否为LPDDR5
        const auto& memSpec = *dmu.dramSys->getConfig().memSpec;
        if (memSpec.memoryType != DRAMSys::MemSpec::MemoryType::LPDDR5)
        {
            std::cout << "\n❌ 错误: 频率比测试仅支持LPDDR5配置！" << std::endl;
            std::cout << "请使用LPDDR5配置文件运行测试" << std::endl;
            sc_core::sc_stop();
        }
        else
        {
            // 运行当前配置的AC Timing测试
            std::cout << "\n测试配置: " << baseConfig << std::endl;
            bool test_passed = run_lp5_ac_timing_tests(dmu.dramSys.get());
            
            if (test_passed)
            {
                std::cout << "\n✅ 频率比测试通过!" << std::endl;
            }
            else
            {
                std::cout << "\n❌ 频率比测试失败!" << std::endl;
            }
            
            // 频率比测试不需要运行仿真
            sc_core::sc_stop();
        }
    }
    else
    {
        // 生成流量
        TrafficPatternGenerator pattern_gen(42); // seed for reproducibility
        pattern_gen.setAddressDecoder(&addressDecoder);  // 使用精确的地址编码
        pattern_gen.generate(tg, selected_pattern, num_requests, true);
        
        sc_core::sc_start(200, sc_core::SC_US);
    }
    // sc_core::sc_pause();
    // // tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00001000, ARM::CHI::SIZE_32);
    // // tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x40001000, ARM::CHI::SIZE_32);
    // // add_payloads_to_tg(tg);
    // sc_core::sc_start(500, sc_core::SC_NS);
    if(!sc_core::sc_end_of_simulation_invoked())
    {
        SC_REPORT_WARNING("Simulator", "Simulation stopped without explicit sc_stop()");
        sc_core::sc_stop();
    }
    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = finish - start;
    std::cout << "Simulation took " + std::to_string(elapsed.count()) + " seconds." << std::endl;

    ARM::CHI::Payload::debug_payload_pool(std::cout);

    std::cout << "========================================" << std::endl;
    std::cout << "Log saved to: " << logFile << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 恢复标准输出
    std::cout.rdbuf(console_buf);
    logStream.close();

    return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n[FATAL ERROR] Exception caught in sc_main: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "\n[FATAL ERROR] Unknown exception caught in sc_main!" << std::endl;
        return 1;
    }
}