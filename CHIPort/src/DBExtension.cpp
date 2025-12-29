#include "CHIPort/DBExtension.h"


using namespace sc_core;
using namespace tlm;

namespace dmu{
DBIntfExtension::DBIntfExtension(uint16_t dbid,
                                 uint16_t rdatinfo_tag,
                                 uint8_t qos,
                                 uint16_t src_id,
                                 const sc_core::sc_time& timeOfGeneration):
    dbid(dbid),
    rdatinfo_tag(rdatinfo_tag),
    qos(qos),
    src_id(src_id),
    timeOfGeneration(timeOfGeneration)
{
}

void DBIntfExtension::setAutoExtension(tlm::tlm_generic_payload& trans,
                                       uint16_t dbid,
                                       uint16_t rdatinfo_tag,
                                       uint16_t src_id)
{
    auto* extension = trans.get_extension<DBIntfExtension>();

    if(extension != nullptr)
    {
        extension->dbid = dbid;
        extension->rdatinfo_tag = rdatinfo_tag;
        extension->qos = 0;
        extension->src_id = src_id;
        extension->timeOfGeneration = SC_ZERO_TIME;
    }
    else
    {
        extension = new DBIntfExtension(dbid, rdatinfo_tag, 0, src_id, SC_ZERO_TIME);
        trans.set_auto_extension(extension);
    }
}

void DBIntfExtension::setExtension(tlm::tlm_generic_payload& trans,
                                   uint16_t dbid,
                                   uint16_t rdatinfo_tag,
                                   uint8_t qos,
                                   uint16_t src_id,
                                   const sc_core::sc_time& timeOfGeneration)
{
    assert(trans.get_extension<DBIntfExtension>() == nullptr);
    auto* extension = new DBIntfExtension(dbid, rdatinfo_tag, qos, src_id, timeOfGeneration);
    trans.set_extension(extension);
}

tlm_extension_base* DBIntfExtension::clone() const
{
    return new DBIntfExtension(dbid, rdatinfo_tag, qos, src_id, timeOfGeneration);
}

void DBIntfExtension::copy_from(const tlm_extension_base& ext)
{
    const auto& cpyFrom = dynamic_cast<const DBIntfExtension&>(ext);
    dbid = cpyFrom.dbid;
    rdatinfo_tag = cpyFrom.rdatinfo_tag;
    qos = cpyFrom.qos;
    src_id = cpyFrom.src_id;
    timeOfGeneration = cpyFrom.timeOfGeneration;
}

uint16_t DBIntfExtension::getDBID() const
{
    return dbid;
}

uint16_t DBIntfExtension::getRdatInfoTag() const
{
    return rdatinfo_tag;
}

uint16_t DBIntfExtension::getSrcId() const
{
    return src_id;
}

uint8_t DBIntfExtension::getQoS() const
{
    return qos;
}

sc_core::sc_time DBIntfExtension::getTimeOfGeneration() const
{
    return timeOfGeneration;
}

const DBIntfExtension& DBIntfExtension::getExtension(const tlm::tlm_generic_payload& trans)
{
    return *trans.get_extension<DBIntfExtension>();
}

uint16_t DBIntfExtension::getDBID(const tlm::tlm_generic_payload& trans)
{
    return trans.get_extension<DBIntfExtension>()->dbid;
}

uint16_t DBIntfExtension::getRdatInfoTag(const tlm::tlm_generic_payload& trans)
{
    return trans.get_extension<DBIntfExtension>()->rdatinfo_tag;
}

uint16_t DBIntfExtension::getSrcId(const tlm::tlm_generic_payload& trans)
{
    return trans.get_extension<DBIntfExtension>()->src_id;
}

uint8_t DBIntfExtension::getQoS(const tlm::tlm_generic_payload& trans)
{
    return trans.get_extension<DBIntfExtension>()->qos;
}

sc_time DBIntfExtension::getTimeOfGeneration(const tlm::tlm_generic_payload& trans)
{
    return trans.get_extension<DBIntfExtension>()->timeOfGeneration;
}





} // dmu namespace