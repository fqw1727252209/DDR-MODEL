#ifndef __CHI_RETRY_RESOURCE_MANAGER_HH__
#define __CHI_RETRY_RESOURCE_MANAGER_HH__

#include <cstddef>
#include <vector>
#include <map>

#include <systemc>


#include "CHIPort/PortCommon.hh"
#include "CHIPort/CHIUtilities.h"
#include "CHIPort/PortCommon.hh"
#include "CHIPort/WdataBufferArray.hh"
#include "Common/Common.hh"
#include "P2cFifo.hh"
#include "sysc/kernel/sc_simcontext.h"


namespace dmu{
    namespace Port{
class CHIPort;


/*
Retry通路存在与主通路相同类型的请求时，主通路的请求一定会被retry，从维序上考虑先到的都retry，后到的也要retry
由于gpr和hpr在rcq队列只剩一个时，会发生竞争，如果此时主通路存在hpr请求，retry模块存在超时的gpr请求，此时应当将
资源赋予给超时的gpr请求

1. lpr 和 gpr 先进行仲裁，tpw和gpw先进行仲裁
2. lgpr, tgpw, cmo, hpr进行轮询仲裁，如果二级仲裁未仲裁到一级仲裁的结果，则二级仲裁的结果保持不变，下一次仲裁还是一级仲裁的结果

1.进行cmo, hpr, lpr, gpr, tpw, gpw的轮询仲裁机制，在记录矩阵中，对应的命令类型不为空 并且Port有资源可以为对应命令类型服务时，进行轮询仲裁；lpr和gpr，tpw和gpw先进行一级轮询仲裁，
仲裁出来的结果与hpr, tpw, cmo进行二级轮询仲裁，如果二级轮询仲裁的结果，不是一级轮询仲裁的结果，则一级轮询仲裁的结果保持不变
2.添加gpw，gpr的队列超时机制，当存在gpr或者gpw发生超时，使用sc_core::sc_time，则仲裁结果强制为gpr或者gpw，优先gpr
3.对于gpr或者gpw，当有被记录到矩阵时设定超时的时刻expired_time，使用sc_core::sc_time_stamp()判断达到对应的时刻后，判断为超时，当仲裁出gpr或者gpw命令类型时，更新下一次的expired time
3.当仲裁出命令类型后，进行src id选择时，使用轮询仲裁机制选择src id

4.在生成对应的PcrdGrant响应时，参与轮询仲裁的请求需要考虑当前的队列是否有资源
*/


class RetryResourceManager {
    typedef unsigned SrcId;
    typedef std::vector<SrcId> SrcIdMatrix; // srcid matrix, record the srcid upstream retry trans num 1-N
    typedef std::vector<SrcIdMatrix> TypeRetryMatrix; // 2-N, cmd type: read: hpr, gpr, lpr
    typedef std::vector<TypeRetryMatrix> RetryMatrix; // 3-N

    RetryMatrix retry_matrix;




    // TODO: 后续优化方向，将矩阵给拆开进行，实现6种不同命令类型的矩阵实现
    // struct CmdTypeMatrix{
    //     SrcidMatrix src_id_matrix;
    //     unsigned last_src_id{0};
    //     unsigned sent_to_upstream_p_credit{0};
         // bool is_type_empty() const { return src_id_matrix.empty(); }

    // };
private:
    std::map<PortCmdType,int> type_src_id_arbit_result;
public:
    explicit RetryResourceManager(const Configure& configure,const P2cFifo& p2c_fifo, const WdataBufferArray& wdata_buffer_array,const sc_core::sc_time port_clock_period);
    ~RetryResourceManager() = default;

    void cnt_inc(RetryType type, unsigned cmd_type_idx, unsigned src_id);
    void cnt_dec(RetryType type, unsigned cmd_type_idx, unsigned src_id);

    
    void cnt_dec(PortCmdType cmd_type, unsigned src_id);
    // unsigned get_cnt(RetryType type, unsigned cmd_type_idx, unsigned src_id) const;

    // 方便使用的特定命令类型的增加函数
    inline void inc_read_hpr(unsigned src_id) { cnt_inc(RetryType::Read, static_cast<unsigned>(ReadCmdType::HPR), src_id); }
    inline void inc_read_lpr(unsigned src_id) { cnt_inc(RetryType::Read, static_cast<unsigned>(ReadCmdType::LPR), src_id); }
    inline void inc_read_gpr(unsigned src_id) { cnt_inc(RetryType::Read, static_cast<unsigned>(ReadCmdType::GPR), src_id); }

    inline void inc_write_tpw(unsigned src_id) { cnt_inc(RetryType::Write, static_cast<unsigned>(WriteCmdType::TPW), src_id); }
    inline void inc_write_gpw(unsigned src_id) { cnt_inc(RetryType::Write, static_cast<unsigned>(WriteCmdType::GPW), src_id); }

    inline void inc_cmo(unsigned src_id) { cnt_inc(RetryType::CMO, static_cast<unsigned>(CMOCmdType::CMO), src_id); }

    // 方便使用的特定命令类型的减少函数
    inline void dec_read_hpr(unsigned src_id) { cnt_dec(RetryType::Read, static_cast<unsigned>(ReadCmdType::HPR), src_id); }
    inline void dec_read_lpr(unsigned src_id) { cnt_dec(RetryType::Read, static_cast<unsigned>(ReadCmdType::LPR), src_id); }
    inline void dec_read_gpr(unsigned src_id) { cnt_dec(RetryType::Read, static_cast<unsigned>(ReadCmdType::GPR), src_id); }

    inline void dec_write_tpw(unsigned src_id) { cnt_dec(RetryType::Write, static_cast<unsigned>(WriteCmdType::TPW), src_id); }
    inline void dec_write_gpw(unsigned src_id) { cnt_dec(RetryType::Write, static_cast<unsigned>(WriteCmdType::GPW), src_id); }

    inline void dec_cmo(unsigned src_id) { cnt_dec(RetryType::CMO, static_cast<unsigned>(CMOCmdType::CMO), src_id); }


    // 判断gpw,gpr 队列是否超时
    bool is_gpr_expired() const { return retry_gpr_expired_enable && sc_core::sc_time_stamp() >= gpr_expire_time;}
    bool is_gpw_expired() const { return retry_gpw_expired_enable && sc_core::sc_time_stamp() >= gpw_expire_time;}

    // 仲裁逻辑
    PortCmdType select_type_cmd_type();
    unsigned select_src_id(PortCmdType cmd_type);


    // 判断函数
    bool is_empty() const;                                      // 判断整个矩阵是否为空
    bool is_type_empty(RetryType type) const;                   // 判断指定RetryType是否为空
    bool is_cmd_type_empty(RetryType type, unsigned cmd_type_idx) const; // 判断指定RetryType和cmd_type_idx对应的矩阵是否为空

    bool has_retry_cmd(PriorityClass qos_level) const;
    bool has_cmo_retry_cmd() const { return !is_type_empty(RetryType::CMO);}

    // sending to upstream p -credit function
    inline void lgpr_send_upstream_p_credit_inc() { lgpr_send_upstream_p_credit ++; }
    inline void lgpr_send_upstream_p_credit_dec() { lgpr_send_upstream_p_credit --; }

    inline void hpr_send_upstream_p_credit_inc() { hpr_send_upstream_p_credit ++; }
    inline void hpr_send_upstream_p_credit_dec() { hpr_send_upstream_p_credit --; }

    inline void tpw_send_upstream_p_credit_inc() { tpw_send_upstream_p_credit ++; }
    inline void tpw_send_upstream_p_credit_dec() { tpw_send_upstream_p_credit --; }

    inline void cmo_send_upstream_p_credit_inc() { cmo_send_upstream_p_credit ++; }
    inline void cmo_send_upstream_p_credit_dec() { cmo_send_upstream_p_credit --; }

    void send_upstream_p_credit_inc(PortCmdType cmd_type);
    void send_upstream_p_credit_dec(PortCmdType cmd_type);


    // 判断队列是否为满
    bool is_hpr_full() const      //
    { return p2c_fifo.IsHprQueueFull() || p2c_fifo.hpr_queue->GetQueueSize() + hpr_send_upstream_p_credit >= p2c_fifo.hpr_queue->GetMaxQueueDepth(); }
    bool is_lgpr_full() const
    { return p2c_fifo.IsLprQueueFull() || p2c_fifo.lpr_queue->GetQueueSize() + lgpr_send_upstream_p_credit >= p2c_fifo.lpr_queue->GetMaxQueueDepth();  }
    bool is_cmo_full() const { return false;}    // TODO: Need to add the CMO Queue and resource allocation
    bool is_tpw_full() const { return wdata_buffer_array.IsArrayFull() || p2c_fifo.IsTpwQueueFull() || p2c_fifo.tpw_queue->GetQueueSize() + tpw_send_upstream_p_credit >= p2c_fifo.tpw_queue->GetMaxQueueDepth(); }  //

    bool is_gpr_expired_and_rd_queue_only_one_space() const {   return p2c_fifo.IsRdQueueRemainOneSpace() && is_gpr_expired();}

    // 需要本周期进行是否生成PcrdGrant响应的函数
    bool is_need_to_send_pcrd_grant() const
    { return !is_hpr_full() && has_retry_cmd(PriorityClass::HPR)
        || !is_lgpr_full() && ( has_retry_cmd(PriorityClass::LPR) || has_retry_cmd(PriorityClass::GPR) )
        || !is_tpw_full() && (has_retry_cmd(PriorityClass::TPW) || has_retry_cmd(PriorityClass::GPW))
        || !is_cmo_full() && has_cmo_retry_cmd(); }





private:
    //初始化矩阵
    void    initialize_matrix();
    const   P2cFifo& p2c_fifo;
    const   WdataBufferArray& wdata_buffer_array;

    unsigned lgpr_send_upstream_p_credit{0};
    unsigned hpr_send_upstream_p_credit{0};
    unsigned tpw_send_upstream_p_credit{0};
    unsigned cmo_send_upstream_p_credit{0};

    const sc_core::sc_time gpr_expired_threshold;
    const sc_core::sc_time gpw_expired_threshold;

    const bool retry_gpr_expired_enable;
    const bool retry_gpw_expired_enable;

    sc_core::sc_time gpr_expire_time;
    sc_core::sc_time gpw_expire_time;

    unsigned lpr_gpr_arbit_result{0}; // 1-level ariter
    unsigned tpw_gpw_arbit_result{0}; // 1-level arbiter
    int next_arbit_search_pos{-1}; // 2-level arbiter




};








} //namespace Port
} // namespace dmu

#endif