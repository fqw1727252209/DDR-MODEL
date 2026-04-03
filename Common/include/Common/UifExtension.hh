#ifndef __UIF_EXTENSION_HH__
#define __UIF_EXTENSION_HH__

#include <systemc>
#include <tlm>

#include "Common/Common.hh"
#include "sysc/kernel/sc_simcontext.h"

namespace dmu{

struct UifInfo{
    
    //Port -> DDRC
    bool is_rmw{false};
    bool is_burst_chop{false};
    unsigned cmd_id{0}; // for rd or wr transaction
    sc_core::sc_time expired_time{sc_core::sc_max_time()};
    Qos qos{0,true};
    CmdType cmd_type{CmdType::Invalid};

    // DDRC -> Port
    unsigned wr_cam_index{0};
    unsigned rd_cmd_id{0};
    unsigned wr_cmd_id{0}; //only for write transaction complete

};


struct UifSideBandInfo
{
    //Port -> DDRC
    bool uif_gpr_go2critical{false};
    bool uif_gpw_go2critical{false};
    //DDRC -> Port
    // unsigned tpw_credit{0};
    // unsigned lpr_credit{0};
    // unsigned hpr_credit{0};
    //DDRC -> Port
    bool tpw_credit_valid{false};
    bool lpr_credit_valid{false};
    bool hpr_credit_valid{false};
};

class UifSideBandExtension: public tlm::tlm_extension<UifSideBandExtension>
{

public:
    explicit UifSideBandExtension(const UifSideBandInfo& uif_side_band_info)
    : _uif_side_band_info(uif_side_band_info)
    {
    }
    tlm::tlm_extension_base* clone() const override
    {
        UifSideBandExtension* ext = new UifSideBandExtension(this->_uif_side_band_info);

        return ext;
    }

    void copy_from(const tlm::tlm_extension_base& ext) override
    {
        ;
    }

public:
    UifSideBandInfo _uif_side_band_info;
};

class UifExtension: public tlm::tlm_extension<UifExtension>
{

public:
    explicit UifExtension(const UifInfo& uif_info)
    : _uif_info(uif_info)
    {
    }
    tlm::tlm_extension_base* clone() const override
    {
        UifExtension* ext = new UifExtension(this->_uif_info);

        return ext;
    }

    void copy_from(const tlm::tlm_extension_base& ext) override
    {
        ;
    }

    PriorityClass GetQosLevel()
    {
        return _uif_info.qos.GetQosLevel();
    }

    void SetWrDatRequestIndex(unsigned index)
    {
        _uif_info.wr_cam_index = index;
    }
public:
    UifInfo _uif_info;
};


} // dmu

#endif