#include "Common/StatisticExtension.hh"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>

namespace dmu {

// StatisticExtension构造函数
StatisticExtension::StatisticExtension()
{
    // 默认构造函数实现
}

// 将统计信息打印到指定文件
void StatisticExtension::PrintStatistics(const std::string& filename) const
{
    std::ofstream file(filename, std::ios::app);
    if (file.is_open()) {
        // PrintStatistics(file);
    } else {
        std::cerr << "Unable to open file: " << filename << std::endl;
        // 文件打开失败时终止程序，因为程序运行没有意义
        std::abort();
    }
}

// 将统计信息打印到指定输出流
void StatisticExtension::PrintStatistics(std::ostream& os) const
{
    os << "===== Transaction Statistics =====" << std::endl;
    os << "Transaction ID: " << transaction_id << std::endl;
    os << "Transaction Sdram Address: " << m_decoded_address << std::endl;
    os << "Entering Port Time: " << m_entering_port_time.value() << " ps" << std::endl;
    os << "Leaving Port Time: " << m_leaving_port_time.value() <<" ps" << std::endl;
    os << "Entering CAM Time: " << m_entering_cam_time.value() <<" ps" << std::endl;
    os << "Leaving CAM Time: " << m_leaveing_cam_time.value() <<" ps" << std::endl;
    os << "Command Times:" << std::endl;
    for (const auto& cmd_time : m_cmd_time_vec) {
        os << "  Time: " << cmd_time.first.value() <<" ps"
           << ", Command: " << to_string(cmd_time.second) << std::endl;
    }
    os << "UIF Data Begin Time: " << m_uif_data_begin_time.value() << " ps" << std::endl;
    os << "UIF Data End Time: " << m_uif_data_end_time.value() << " ps" << std::endl;
    os << "DFI Data Begin Time: " << m_dfi_data_begin_time.value() << " ps" << std::endl;
    os << "DFI Data End Time: " << m_dfi_data_end_time.value() << " ps" << std::endl;
    os << "DQ Data Begin Time: " << m_dq_data_begin_time.value() << " ps" << std::endl;
    os << "DQ Data End Time: " << m_dq_data_end_time.value() << " ps" << std::endl;

    os << "================================" << std::endl;
}

// 使用指定名称的Logger输出统计信息
void StatisticExtension::PrintStatisticsToLogger(const std::string& logger_name) const
{
    Logger& logger = Logger::getInstance(logger_name);
    PrintStatisticsToLogger(logger);
}

// 使用指定的logger输出统计信息
void StatisticExtension::PrintStatisticsToLogger(Logger& logger) const
{
    std::ostringstream oss;
    oss << "\n===== Transaction Statistics =====\n";
    oss << "ID: " << transaction_id
        << "  Addr: " << m_decoded_address << "\n";
    oss << "Port:  Enter=" << m_entering_port_time.value() << "ps"
        << "  Leave=" << m_leaving_port_time.value() << "ps\n";
    oss << "CAM:   Enter=" << m_entering_cam_time.value() << "ps"
        << "  Leave=" << m_leaveing_cam_time.value() << "ps\n";
    oss << "UIF:   Begin=" << m_uif_data_begin_time.value() << "ps"
        << "  End=" << m_uif_data_end_time.value() << "ps\n";
    oss << "DFI:   Begin=" << m_dfi_data_begin_time.value() << "ps"
        << "  End=" << m_dfi_data_end_time.value() << "ps\n";
    oss << "DQ:    Begin=" << m_dq_data_begin_time.value() << "ps"
        << "  End=" << m_dq_data_end_time.value() << "ps\n";
    oss << "CMDs:\n";
    for (const auto& cmd_time : m_cmd_time_vec) {
        oss << "  Time=" << cmd_time.first.value() << "ps"
            << "  Cmd=" << to_string(cmd_time.second) << "\n";
    }
    oss << "================================";
    logger.write(LogLevel::INFO, oss.str());
}

} // namespace dmu