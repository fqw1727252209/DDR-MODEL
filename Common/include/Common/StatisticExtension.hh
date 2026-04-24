#ifndef STATISTIC_EXTENSION_HH__
#define STATISTIC_EXTENSION_HH__


#include <vector>

#include "Common/Common.hh"
#include "Configure/AddressDecoder.hh"
#include "sysc/kernel/sc_time.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include <systemc>
#include <tlm>

// #include "Controller/common/Command.hh"
#include "Common/Common.hh"

namespace dmu{

class StatisticExtension: public tlm::tlm_extension<StatisticExtension>
{

    public:
        explicit StatisticExtension();
        ~StatisticExtension() = default;

        tlm::tlm_extension_base* clone() const override
        {
            return new StatisticExtension(*this);
        }

        void copy_from(const tlm::tlm_extension_base& ext) override
        {
            const auto& other = static_cast<const StatisticExtension&>(ext);
            transaction_id = other.transaction_id;
            m_entering_port_time = other.m_entering_port_time;
            m_leaving_port_time = other.m_leaving_port_time;
            m_entering_cam_time = other.m_entering_cam_time;
            m_leaveing_cam_time = other.m_leaveing_cam_time;
            m_cmd_time_vec = other.m_cmd_time_vec;
            m_uif_data_begin_time = other.m_uif_data_begin_time;
            m_uif_data_end_time = other.m_uif_data_end_time;
            m_dfi_data_begin_time = other.m_dfi_data_begin_time;
            m_dfi_data_end_time = other.m_dfi_data_end_time;
            m_dq_data_begin_time = other.m_dq_data_begin_time;
            m_dq_data_end_time = other.m_dq_data_end_time;
            m_decoded_address = other.m_decoded_address;
        }

        inline void RecordInPortTime(sc_core::sc_time record_time){ m_entering_port_time = record_time;}
        inline void RecordOutPortTime(sc_core::sc_time record_time){ m_leaving_port_time = record_time;}

        inline void RecordDecodedAddress(DecodedAddress decoded_address){ m_decoded_address = decoded_address;}

        inline void RecordInCamTime(sc_core::sc_time record_time){ m_entering_cam_time = record_time;}
        inline void RecordOutCamTime(sc_core::sc_time record_time){ m_leaveing_cam_time = record_time;}

        inline void RecordCmdTime(sc_core::sc_time record_time,DramCommand cmd){m_cmd_time_vec.emplace_back(record_time,cmd);}// need to record cmd type?

        // 添加获取记录时间的方法
        inline sc_core::sc_time GetInPortTime() const { return m_entering_port_time; }
        inline sc_core::sc_time GetOutPortTime() const { return m_leaving_port_time; }
        inline sc_core::sc_time GetInCamTime() const { return m_entering_cam_time; }
        inline sc_core::sc_time GetOutCamTime() const { return m_leaveing_cam_time; }
        inline const std::vector<std::pair<sc_core::sc_time, DramCommand>>& GetCmdTimeVec() const { return m_cmd_time_vec; }

        // 新增数据传输相关时间的getter和setter
        inline sc_core::sc_time GetUiFDataBeginTime() const { return m_uif_data_begin_time; }
        inline sc_core::sc_time GetUiFDataEndTime() const { return m_uif_data_end_time; }
        inline void RecordUiFDataBeginTime(sc_core::sc_time time) { m_uif_data_begin_time = time; }
        inline void RecordUiFDataEndTime(sc_core::sc_time time) { m_uif_data_end_time = time; }

        inline sc_core::sc_time GetDfiDataBeginTime() const { return m_dfi_data_begin_time; }
        inline sc_core::sc_time GetDfiDataEndTime() const { return m_dfi_data_end_time; }
        inline void RecordDfiDataBeginTime(sc_core::sc_time time) { m_dfi_data_begin_time = time; }
        inline void RecordDfiDataEndTime(sc_core::sc_time time) { m_dfi_data_end_time = time; }

        inline sc_core::sc_time GetDqDataBeginTime() const { return m_dq_data_begin_time; }
        inline sc_core::sc_time GetDqDataEndTime() const { return m_dq_data_end_time; }
        inline void RecordDqDataBeginTime(sc_core::sc_time time) { m_dq_data_begin_time = time; }
        inline void RecordDqDataEndTime(sc_core::sc_time time) { m_dq_data_end_time = time; }
        inline unsigned GetTransactionId() const { return transaction_id; }
        inline const DecodedAddress& GetDecodedAddress() const { return m_decoded_address; }
        inline void RecordTransactionId(unsigned id) { transaction_id = id; }


        // 添加打印统计信息到指定文件的功能
        void PrintStatistics(const std::string& filename) const;
        void PrintStatistics(std::ostream& os) const;


    private:
        unsigned transaction_id{0};

        sc_core::sc_time m_entering_port_time{sc_core::SC_ZERO_TIME};
        sc_core::sc_time m_leaving_port_time{sc_core::SC_ZERO_TIME};

        sc_core::sc_time m_entering_cam_time{sc_core::SC_ZERO_TIME};
        sc_core::sc_time m_leaveing_cam_time{sc_core::SC_ZERO_TIME};

        std::vector<std::pair<sc_core::sc_time,DramCommand>> m_cmd_time_vec; // record this cmd do the ddr cmd sending

        sc_core::sc_time m_uif_data_begin_time{sc_core::SC_ZERO_TIME};
        sc_core::sc_time m_uif_data_end_time{sc_core::SC_ZERO_TIME};

        sc_core::sc_time m_dfi_data_begin_time{sc_core::SC_ZERO_TIME};
        sc_core::sc_time m_dfi_data_end_time{sc_core::SC_ZERO_TIME};

        sc_core::sc_time m_dq_data_begin_time{sc_core::SC_ZERO_TIME};
        sc_core::sc_time m_dq_data_end_time{sc_core::SC_ZERO_TIME};

        DecodedAddress m_decoded_address;





};



}


#endif