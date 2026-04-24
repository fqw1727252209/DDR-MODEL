#include "CHIPort/RetryResourceManager.hh"
#include "CHIPort/P2cFifo.hh"
#include "CHIPort/PortCommon.hh"
#include "CHIPort/WdataBufferArray.hh"
#include "Common/Common.hh"
#include "sysc/utils/sc_report.h"
#include <cassert>
#include <cstddef>
#include <iostream>
#include <numeric>
namespace dmu{
    namespace Port{

RetryResourceManager::RetryResourceManager(const Configure& configure,const P2cFifo& p2c_fifo, const WdataBufferArray& wdata_buffer_array,const sc_core::sc_time port_clock_period)
: p2c_fifo(p2c_fifo)
, wdata_buffer_array(wdata_buffer_array)
, retry_gpr_expired_enable(configure.controller_config->RETRY_GPR_EXPIRED_ENABLE)
, retry_gpw_expired_enable(configure.controller_config->RETRY_GPW_EXPIRED_ENABLE)
, gpr_expired_threshold(configure.controller_config->GPR_EXPIRED_TIME * port_clock_period)
, gpw_expired_threshold(configure.controller_config->GPW_EXPIRED_TIME * port_clock_period)
{
    initialize_matrix();

    for(size_t i = 0; i < static_cast<size_t>(PortCmdType::Invalid); ++i)
    {
        type_src_id_arbit_result.emplace(static_cast<PortCmdType>(i),-1);
    }
}

// 实现初始化矩阵函数
void
RetryResourceManager::initialize_matrix()
{
    // 清空现有数据
    retry_matrix.clear();

    // 根据配置初始化矩阵大小
    retry_matrix.resize(static_cast<size_t>(RetryType::Invalid)); // 对应RetryType的数量 此处为3

    // 初始化Read类型矩阵 [READ_CMD_TYPES_NUM][SRC_ID_NUM]
    retry_matrix[static_cast<size_t>(RetryType::Read)].resize(READ_CMD_TYPES_NUM);
    for(size_t i = 0; i < READ_CMD_TYPES_NUM; ++i) {
        retry_matrix[static_cast<size_t>(RetryType::Read)][i].resize(SRC_ID_NUM, 0);
    }

    // 初始化Write类型矩阵 [WRITE_CMD_TYPES_NUM][SRC_ID_NUM]
    retry_matrix[static_cast<size_t>(RetryType::Write)].resize(WRITE_CMD_TYPES_NUM);
    for(size_t i = 0; i < WRITE_CMD_TYPES_NUM; ++i) {
        retry_matrix[static_cast<size_t>(RetryType::Write)][i].resize(SRC_ID_NUM, 0);
    }

    // 初始化CMO类型矩阵 [CMO_CMD_TYPES_NUM][SRC_ID_NUM]
    retry_matrix[static_cast<size_t>(RetryType::CMO)].resize(CMO_CMD_TYPES_NUM);
    for(size_t i = 0; i < CMO_CMD_TYPES_NUM; ++i) {
        retry_matrix[static_cast<size_t>(RetryType::CMO)][i].resize(SRC_ID_NUM, 0);
    }
}

void
RetryResourceManager::cnt_inc(RetryType type, unsigned cmd_type_idx, unsigned src_id)
{
    size_t type_idx = static_cast<size_t>(type);

    // 检查索引是否有效
    if (type_idx >= retry_matrix.size() ||
        cmd_type_idx >= retry_matrix[type_idx].size() ||
        src_id >= retry_matrix[type_idx][cmd_type_idx].size()) {
        return; // 索引超出范围，不做操作
    }

    assert(type_idx < retry_matrix.size() ||
        cmd_type_idx < retry_matrix[type_idx].size() || src_id < retry_matrix[type_idx][cmd_type_idx].size());

    retry_matrix[type_idx][cmd_type_idx][src_id]++;
}
void
RetryResourceManager::cnt_dec(RetryType type, unsigned cmd_type_idx, unsigned src_id)
{
    size_t type_idx = static_cast<size_t>(type);

    // 检查索引是否有效
    assert(type_idx < retry_matrix.size() ||
        cmd_type_idx < retry_matrix[type_idx].size() || src_id < retry_matrix[type_idx][cmd_type_idx].size());

    // 防止下溢出
    assert(retry_matrix[type_idx][cmd_type_idx][src_id] > 0);
    retry_matrix[type_idx][cmd_type_idx][src_id]--;
}

bool
RetryResourceManager::is_empty() const
{
    for(size_t type_idx = 0; type_idx < retry_matrix.size(); ++type_idx) {
        if (!is_type_empty(static_cast<RetryType>(type_idx))) {
            return false;
        }
    }
    return true;
}

// 判断指定RetryType是否为空
bool
RetryResourceManager::is_type_empty(RetryType type) const
{
    size_t type_idx = static_cast<size_t>(type);

    for(size_t cmd_type_idx = 0; cmd_type_idx < retry_matrix[type_idx].size(); ++cmd_type_idx) {
        if (!is_cmd_type_empty(type, cmd_type_idx)) {
            return false;
        }
    }
    return true;
}
bool
RetryResourceManager::is_cmd_type_empty(RetryType type, unsigned cmd_type_idx) const
{
    size_t type_idx = static_cast<size_t>(type);

    if (type_idx >= retry_matrix.size() ||
        cmd_type_idx >= retry_matrix[type_idx].size()) {
        return true; // 超出范围视为为空
    }

    const auto& src_vector = retry_matrix[type_idx][cmd_type_idx];

    // 使用std::accumulate来检查该向量中的总和是否为0
    unsigned sum = std::accumulate(src_vector.begin(), src_vector.end(), 0U);
    return sum == 0;
}

bool
RetryResourceManager::has_retry_cmd(PriorityClass qos_level) const
{
    if(qos_level == PriorityClass::HPR)
    {
        return !is_cmd_type_empty(RetryType::Read, static_cast<unsigned>(ReadCmdType::HPR));
    }
    else if(qos_level == PriorityClass::LPR)
    {
        return !is_cmd_type_empty(RetryType::Read, static_cast<unsigned>(ReadCmdType::LPR));
    }
    else if(qos_level == PriorityClass::GPR)
    {
        return !is_cmd_type_empty(RetryType::Read, static_cast<unsigned>(ReadCmdType::GPR));
    }
    else if(qos_level == PriorityClass::TPW)
    {
        return !is_cmd_type_empty(RetryType::Write, static_cast<unsigned>(WriteCmdType::TPW));
    }
    else if(qos_level == PriorityClass::GPW)
    {
        return !is_cmd_type_empty(RetryType::Write, static_cast<unsigned>(WriteCmdType::GPW));
    }
    else{
        SC_REPORT_ERROR("RetryResourceManager", "Invalid priority class in has_retry_cmd()");
        return false;
    }
}

void
RetryResourceManager::send_upstream_p_credit_inc(PortCmdType cmd_type)
{
    if(cmd_type == PortCmdType::HPR){
        hpr_send_upstream_p_credit_inc();
    }
    else if(cmd_type == PortCmdType::LPR || cmd_type == PortCmdType::GPR){
        lgpr_send_upstream_p_credit_inc();
    }
    else if(cmd_type == PortCmdType::TPW || cmd_type == PortCmdType::GPW){
        tpw_send_upstream_p_credit_inc();
    }
    else if(cmd_type == PortCmdType::CMO){
        cmo_send_upstream_p_credit_inc();
    }
    else{
        SC_REPORT_ERROR("RetryResourceManager", "Invalid command type in send_upstream_p_credit_inc()");
    }
}
void
RetryResourceManager::send_upstream_p_credit_dec(PortCmdType cmd_type)
{
    if(cmd_type == PortCmdType::HPR){
        hpr_send_upstream_p_credit_dec();
    }
    else if(cmd_type == PortCmdType::LPR || cmd_type == PortCmdType::GPR){
        lgpr_send_upstream_p_credit_dec();
    }
    else if(cmd_type == PortCmdType::TPW || cmd_type == PortCmdType::GPW){
        tpw_send_upstream_p_credit_dec();
    }
    else if(cmd_type == PortCmdType::CMO){
        cmo_send_upstream_p_credit_dec();
    }
    else{
        SC_REPORT_ERROR("RetryResourceManager", "Invalid command type in send_upstream_p_credit_dec()");
    }
}

PortCmdType
RetryResourceManager::select_type_cmd_type()
{
    if(is_gpr_expired() && has_retry_cmd(PriorityClass::GPR))
    {
        return PortCmdType::GPR;
    }
    if(is_gpw_expired() && has_retry_cmd(PriorityClass::GPW))
    {
        return PortCmdType::GPW;
    }

    // 一级仲裁: LPR和GPR进行轮询仲裁
    PortCmdType lgpr_winner = PortCmdType::Invalid;
    bool lpr_available = has_retry_cmd(PriorityClass::LPR);
    bool gpr_available = has_retry_cmd(PriorityClass::GPR);

    if(lpr_available && gpr_available)
    {
        if(lpr_gpr_arbit_result == 1){
            lgpr_winner = PortCmdType::LPR;
        }
        else if(lpr_gpr_arbit_result == 0){
            lgpr_winner = PortCmdType::GPR;
        }
        else{

        }
    }
    else if(lpr_available){
        lgpr_winner = PortCmdType::LPR;
    }
    else if(gpr_available){
        lgpr_winner = PortCmdType::GPR;
    }
    else{

    }

    // 一级仲裁: TPW和GPW进行轮询仲裁
    PortCmdType tpw_gpw_winner = PortCmdType::Invalid;
    bool tpw_available = has_retry_cmd(PriorityClass::TPW);
    bool gpw_available = has_retry_cmd(PriorityClass::GPW);
    if(tpw_available && gpw_available)
    {
        if(tpw_gpw_arbit_result == 1){
            tpw_gpw_winner = PortCmdType::TPW;
        }
        else if(tpw_gpw_arbit_result == 0){
            tpw_gpw_winner = PortCmdType::GPW;
        }
    }
    else if(tpw_available){
        tpw_gpw_winner = PortCmdType::TPW;
    }
    else if(gpw_available){
        tpw_gpw_winner = PortCmdType::GPW;
    }
    else{

    }

    // 二级仲裁: lgpr, tgpw, cmo, hpr进行轮询
    std::vector<PortCmdType> candidates;
    if(lgpr_winner != PortCmdType::Invalid){
        candidates.push_back(lgpr_winner);
    }
    else{
        candidates.push_back(PortCmdType::Invalid);
    }
    if(tpw_gpw_winner != PortCmdType::Invalid){
        candidates.push_back(tpw_gpw_winner);
    }
    else{
        candidates.push_back(PortCmdType::Invalid);
    }
    if(has_retry_cmd(PriorityClass::HPR)){
        candidates.push_back(PortCmdType::HPR);
    }
    else{
        candidates.push_back(PortCmdType::Invalid);
    }
    if(!is_type_empty(RetryType::CMO)){
        candidates.push_back(PortCmdType::CMO);
    }
    else{
        candidates.push_back(PortCmdType::Invalid);
    }

    // if(candidates.empty()){
    //     std::cerr << "No available retry commands" << std::endl;
    //     std::abort();
    // }
    PortCmdType final_winner = PortCmdType::Invalid;
    for(unsigned i = 0; i < candidates.size(); i++)
    {
        unsigned index = (i + next_arbit_search_pos + 1) % candidates.size();
        if(candidates[index] != PortCmdType::Invalid){
            final_winner = candidates[index];
            next_arbit_search_pos = index;
            break;
        }
    }

    // 轮询选择最终结果,以此判断是否需要锁定一级的仲裁结果
    // in this function final_winner must not be Invalid, otherwise its wrong
    // when final_winner is not equal to lgpr_winner
    // 1. if lgpr is invalid, then final_winner wont equal to lgpr_winner, so no need to update lgpr_winner arbit
    // 2. if lgpr is valid, then if lpr and gpr is valid , so lgpr_winner should not update the lgpr arbit result to lock arbit result
    // 3. if lgpr is valid, then if only lpr or gpr is valid, so lgpr_winner should update the lgpr arbit
    if(final_winner != lgpr_winner && lgpr_winner != PortCmdType::Invalid){
        if(lpr_available && gpr_available)
        {
            // 不更新，锁定结果
        }
        else{
            lpr_gpr_arbit_result = (lgpr_winner == PortCmdType::LPR) ? 0 : 1;
        }
    }
    else if(final_winner == lgpr_winner && lgpr_winner != PortCmdType::Invalid){
        lpr_gpr_arbit_result = (lgpr_winner == PortCmdType::LPR) ? 0 : 1;
    }

    if(final_winner != tpw_gpw_winner && tpw_gpw_winner != PortCmdType::Invalid){
        if(tpw_available && gpw_available)
        {
            // 不更新，锁定结果
        }
        else{
            tpw_gpw_arbit_result = (tpw_gpw_winner == PortCmdType::TPW) ? 0 : 1;
        }
    }
    else if(final_winner == tpw_gpw_winner && tpw_gpw_winner != PortCmdType::Invalid){
        tpw_gpw_arbit_result = (tpw_gpw_winner == PortCmdType::TPW) ? 0 : 1;
    }

    return final_winner;
}

unsigned
RetryResourceManager::select_src_id(PortCmdType cmd_type)
{
    RetryType type;
    unsigned cmd_type_index;
    auto src_id_last_result = type_src_id_arbit_result.at(cmd_type);
    unsigned arbit_src_id;
    if(cmd_type == PortCmdType::HPR){
        type = RetryType::Read;
        cmd_type_index = static_cast<unsigned>(ReadCmdType::HPR);
    }
    else if(cmd_type == PortCmdType::LPR){
        type = RetryType::Read;
        cmd_type_index = static_cast<unsigned>(ReadCmdType::LPR);
    }
    else if(cmd_type == PortCmdType::GPR){
        type = RetryType::Read;
        cmd_type_index = static_cast<unsigned>(ReadCmdType::GPR);
    }
    else if(cmd_type == PortCmdType::TPW){
        type = RetryType::Write;
        cmd_type_index = static_cast<unsigned>(WriteCmdType::TPW);
    }
    else if(cmd_type == PortCmdType::GPW){
        type = RetryType::Write;
        cmd_type_index = static_cast<unsigned>(WriteCmdType::GPW);
    }
    else if(cmd_type == PortCmdType::CMO){
        type = RetryType::CMO;
        cmd_type_index = static_cast<unsigned>(CMOCmdType::CMO);
    }
    else{
        std::cerr << "Invalid cmd type" << std::endl;
        std::abort();
    }

    const auto& src_vector = retry_matrix.at(static_cast<size_t>(type)).at(cmd_type_index);

    for(unsigned i = 0; i < src_vector.size(); i++)
    {
        unsigned index = (i + src_id_last_result + 1) % src_vector.size();
        if(src_vector[index] != 0){
            arbit_src_id = index;
            type_src_id_arbit_result[cmd_type] = index;
            break;
        }
    }
    cnt_dec(type, cmd_type_index, arbit_src_id);
    return arbit_src_id;
}

void
RetryResourceManager::cnt_dec(PortCmdType cmd_type, unsigned src_id)
{
    RetryType type;
    unsigned cmd_type_index;
    auto src_id_last_result = type_src_id_arbit_result.at(cmd_type);
    unsigned arbit_src_id;
    if(cmd_type == PortCmdType::HPR){
        type = RetryType::Read;
        cmd_type_index = static_cast<unsigned>(ReadCmdType::HPR);
    }
    else if(cmd_type == PortCmdType::LPR){
        type = RetryType::Read;
        cmd_type_index = static_cast<unsigned>(ReadCmdType::LPR);
    }
    else if(cmd_type == PortCmdType::GPR){
        type = RetryType::Read;
        cmd_type_index = static_cast<unsigned>(ReadCmdType::GPR);
    }
    else if(cmd_type == PortCmdType::TPW){
        type = RetryType::Write;
        cmd_type_index = static_cast<unsigned>(WriteCmdType::TPW);
    }
    else if(cmd_type == PortCmdType::GPW){
        type = RetryType::Write;
        cmd_type_index = static_cast<unsigned>(WriteCmdType::GPW);
    }
    else if(cmd_type == PortCmdType::CMO){
        type = RetryType::CMO;
        cmd_type_index = static_cast<unsigned>(CMOCmdType::CMO);
    }
    else{
        std::cerr << "Invalid cmd type" << std::endl;
        std::abort();
    }
    // cnt_dec(type, cmd_type_index, src_id);
}


    } // namespace Port
} // namespace dmu