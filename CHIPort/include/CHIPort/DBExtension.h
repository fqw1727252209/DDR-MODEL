#ifndef __DB_EXTENTION_H
#define __DB_EXTENTION_H


#include <systemc>
#include <tlm>
namespace dmu{

class DBIntfExtension : public tlm::tlm_extension<DBIntfExtension>
{
public:
    static void setAutoExtension(tlm::tlm_generic_payload& trans,
                                 uint16_t dbid,
                                 uint16_t rdatinfo_tag,
                                 uint16_t src_id);
    static void setExtension(tlm::tlm_generic_payload& trans,
                             uint16_t dbid,
                             uint16_t rdatinfo_tag,
                             uint8_t qos,
                             uint16_t src_id,
                             const sc_core::sc_time& timeOfGeneration);
    [[nodiscard]] tlm::tlm_extension_base* clone() const override;
    void copy_from(const tlm::tlm_extension_base& ext) override;

    [[nodiscard]] uint16_t getDBID() const;
    [[nodiscard]] uint16_t getRdatInfoTag() const;
    [[nodiscard]] uint16_t getSrcId() const;
    [[nodiscard]] uint8_t getQoS() const;
    [[nodiscard]] sc_core::sc_time getTimeOfGeneration() const;

    static const DBIntfExtension& getExtension(const tlm::tlm_generic_payload& trans);

    static uint16_t getDBID(const tlm::tlm_generic_payload& trans);
    static uint16_t getRdatInfoTag(const tlm::tlm_generic_payload& trans);
    static uint16_t getSrcId(const tlm::tlm_generic_payload& trans);
    static uint8_t getQoS(const tlm::tlm_generic_payload& trans);
    static sc_core::sc_time getTimeOfGeneration(const tlm::tlm_generic_payload& trans);

private:
    DBIntfExtension(uint16_t dbid,
                    uint16_t rdatinfo_tag,
                    uint8_t qos,
                    uint16_t src_id,
                    const sc_core::sc_time& timeOfGeneration);
    uint16_t dbid;
    uint16_t rdatinfo_tag;
    uint8_t qos;
    uint16_t src_id;
    sc_core::sc_time timeOfGeneration;
};

} // namespace dmu

#endif