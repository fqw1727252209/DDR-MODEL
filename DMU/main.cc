#include <iostream>
#include <fstream>
#include <ctime>
#include <sstream>
#include <iomanip>

#include "CHIPort/CHITrafficGenerator.h"
#include "CHIPort/CHIMonitor.h"


#include "DMU/DramMannageUnit.h"

#include <DRAMSys/config/DRAMSysConfiguration.h>

#include <filesystem>

#include <random>

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
    MIXED_RW             // 读写混合
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
        default: return "UNKNOWN";
    }
}

//============================================================================
// DDR4地址映射常量 (based on am_ddr4_8x4Gbx8_dimm_p1KB_brc.json)
// BRC映射: Bank[31:28] - Row[27:13] - Column[12:3] - Byte[2:0]
//============================================================================
constexpr uint64_t TG_BASE_ADDR      = 0x00000000;
constexpr unsigned TG_BYTE_BITS      = 3;   // bits 0-2
constexpr unsigned TG_COLUMN_BITS    = 10;  // bits 3-12
constexpr unsigned TG_ROW_BITS       = 15;  // bits 13-27
constexpr unsigned TG_BANK_BITS      = 2;   // bits 28-29 (bank within group)
constexpr unsigned TG_BANK_GROUP_BITS = 2;  // bits 30-31

constexpr uint64_t TG_COLUMN_OFFSET     = TG_BYTE_BITS;                              // 3
constexpr uint64_t TG_ROW_OFFSET        = TG_COLUMN_OFFSET + TG_COLUMN_BITS;         // 13
constexpr uint64_t TG_BANK_OFFSET       = TG_ROW_OFFSET + TG_ROW_BITS;               // 28
constexpr uint64_t TG_BANK_GROUP_OFFSET = TG_BANK_OFFSET + TG_BANK_BITS;             // 30

constexpr uint64_t TG_PAGE_SIZE      = 1ULL << (TG_COLUMN_OFFSET + TG_COLUMN_BITS);  // 8KB (2^13)
constexpr uint64_t TG_ROW_SIZE       = TG_PAGE_SIZE;                                 // Same as page
constexpr uint64_t TG_BANK_SIZE      = 1ULL << (TG_ROW_OFFSET + TG_ROW_BITS);        // 256MB per bank

//============================================================================
// 地址生成辅助函数
//============================================================================
uint64_t make_address(unsigned bank_group, unsigned bank, unsigned row, unsigned column, unsigned byte_offset = 0)
{
    return TG_BASE_ADDR
         | (static_cast<uint64_t>(bank_group & 0x3) << TG_BANK_GROUP_OFFSET)
         | (static_cast<uint64_t>(bank & 0x3) << TG_BANK_OFFSET)
         | (static_cast<uint64_t>(row & 0x7FFF) << TG_ROW_OFFSET)
         | (static_cast<uint64_t>(column & 0x3FF) << TG_COLUMN_OFFSET)
         | (byte_offset & 0x7);
}

//============================================================================
// 流量生成器类
//============================================================================
class TrafficPatternGenerator
{
public:
    TrafficPatternGenerator(unsigned seed = 42) : rng(seed) {}
    
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
    
    const char* pattern_name(TrafficPattern p)
    {
        return get_pattern_name(p);
    }
    
    ARM::CHI::ReqOpcode get_opcode(bool is_read)
    {
        return is_read ? ARM::CHI::REQ_OPCODE_READ_NO_SNP 
                       : ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_PTL;
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
        std::uniform_int_distribution<uint64_t> addr_dist(0, TG_BANK_SIZE * 16 - 1);
        
        for (unsigned i = 0; i < num_requests; ++i)
        {
            uint64_t addr = (addr_dist(rng) >> 6) << 6; // align to 64 bytes
            tg.add_payload(get_opcode(read_only), addr, ARM::CHI::SIZE_64);
        }
        std::cout << "  Random addresses across entire memory space" << std::endl;
    }
    
    // Pattern 3: 同Bank不同Row - 频繁触发Row切换
    void generate_same_bank_diff_row(CHITrafficGenerator& tg, unsigned num_requests, bool read_only)
    {
        const unsigned target_bank = 0;
        const unsigned target_bank_group = 0;
        
        for (unsigned i = 0; i < num_requests; ++i)
        {
            unsigned row = i % (1 << TG_ROW_BITS);
            unsigned col = 0;
            uint64_t addr = make_address(target_bank_group, target_bank, row, col);
            tg.add_payload(get_opcode(read_only), addr, ARM::CHI::SIZE_64);
        }
        std::cout << "  Fixed Bank: " << target_bank 
                  << ", BankGroup: " << target_bank_group
                  << ", Rotating Rows" << std::endl;
    }
    
    // Pattern 4: 不同Bank同Row - 利用Bank并行性
    void generate_diff_bank_same_row(CHITrafficGenerator& tg, unsigned num_requests, bool read_only)
    {
        const unsigned target_row = 100;
        const unsigned num_banks = 4;
        const unsigned num_bank_groups = 4;
        
        for (unsigned i = 0; i < num_requests; ++i)
        {
            unsigned bank = i % num_banks;
            unsigned bank_group = (i / num_banks) % num_bank_groups;
            unsigned col = (i / (num_banks * num_bank_groups)) % (1 << TG_COLUMN_BITS);
            uint64_t addr = make_address(bank_group, bank, target_row, col);
            tg.add_payload(get_opcode(read_only), addr, ARM::CHI::SIZE_64);
        }
        std::cout << "  Fixed Row: " << target_row 
                  << ", Rotating across " << (num_banks * num_bank_groups) << " banks" << std::endl;
    }
    
    // Pattern 5: 固定步长访问
    void generate_strided(CHITrafficGenerator& tg, unsigned num_requests, bool read_only)
    {
        const uint64_t stride = TG_PAGE_SIZE; // 每次跳过一个Page（触发Row切换）
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
        std::uniform_int_distribution<uint64_t> addr_dist(0, TG_BANK_SIZE * 4 - 1);
        
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
}

int sc_main(int argc, char** argv)
{
    //========================================================================
    // 解析命令行参数选择流量模式
    //========================================================================
    TrafficPattern selected_pattern = TrafficPattern::SEQUENTIAL;
    const unsigned num_requests = 60;
    
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
    sc_core::sc_clock clk("clk", 2, sc_core::SC_NS, 0.5);
    CHITrafficGenerator tg("tg", data_width_bits);
    CHIMonitor mon("mon", data_width_bits);

    tg.clock.bind(clk);
    dmu.chi_port->clock.bind(clk);

    tg.initiator.bind(mon.target);
    mon.initiator.bind(dmu.chi_port->target);

    // 生成流量
    TrafficPatternGenerator pattern_gen(42); // seed for reproducibility
    pattern_gen.generate(tg, selected_pattern, num_requests, true);
    
    sc_core::sc_start(10, sc_core::SC_US);
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