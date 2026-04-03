#include "Common/StatisticExtension.hh"
#include <fstream>
#include <iostream>
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
    os << "Entering Port Time: " << m_entering_port_time.value()<<" ps" << std::endl;
    os << "Leaving Port Time: " << m_leaving_port_time.value()<<" ps" << std::endl;
    os << "Entering CAM Time: " << m_entering_cam_time.value()<<" ps" << std::endl;
    os << "Leaving CAM Time: " << m_leaveing_cam_time.value()<<" ps" << std::endl;
    os << "Command Times:" << std::endl;
    for (const auto& cmd_time : m_cmd_time_vec) {
        os << "  Time: " << cmd_time.first.value()<<" ps"
           << ", Command: " << (to_string(cmd_time.second)) << std::endl;
    }

    os << "UIF Data Begin Time: " << m_uif_data_begin_time.value() << " ps" << std::endl;
    os << "UIF Data End Time: " << m_uif_data_end_time.value() << " ps" << std::endl;
    os << "DFI Data Begin Time: " << m_dfi_data_begin_time.value() << " ps" << std::endl;
    os << "DFI Data End Time: " << m_dfi_data_end_time.value() << " ps" << std::endl;
    os << "DQ Data Begin Time: " << m_dq_data_begin_time.value() << " ps" << std::endl;
    os << "DQ Data End Time: " << m_dq_data_end_time.value() << " ps" << std::endl;

    os << "=================================" << std::endl;

}

} // namespace dmu