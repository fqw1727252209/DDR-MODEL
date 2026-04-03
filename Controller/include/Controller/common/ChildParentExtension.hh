#ifndef __CHILD_PARENT_EXTENSION_HH__
#define __CHILD_PARENT_EXTENSION_HH__

#include <vector>

#include <tlm>

namespace dmu{
    namespace Controller{

class ChildExtension: public tlm::tlm_extension<ChildExtension>
{
private:
    tlm::tlm_generic_payload* _parent_trans;
    explicit ChildExtension(tlm::tlm_generic_payload& parentTrans) : _parent_trans(&parentTrans){}

public:
    tlm::tlm_extension_base* clone() const override;
    void copy_from(const tlm::tlm_extension_base& ext) override;
    tlm::tlm_generic_payload& GetParentTrans();

    static tlm::tlm_generic_payload& GetParentTrans(tlm::tlm_generic_payload& childTrans);
    static void SetExtension(tlm::tlm_generic_payload& childTrans,
                             tlm::tlm_generic_payload& parentTrans);
    static bool IsChildTrans(const tlm::tlm_generic_payload& trans);
};

class ParentExtension: public tlm::tlm_extension<ParentExtension>
{
private:
    std::vector<tlm::tlm_generic_payload*> _child_transes;
    unsigned completed_child_transes = 0;
    explicit ParentExtension(std::vector<tlm::tlm_generic_payload*> child_transes):
        _child_transes(std::move(child_transes))
    {}

public:
    ParentExtension() = delete;
    tlm::tlm_extension_base* clone() const override;
    void copy_from(const tlm::tlm_extension_base& ext) override;

    static void SetExtension(tlm::tlm_generic_payload& parentTrans,
                             std::vector<tlm::tlm_generic_payload*> childTrans);

    const std::vector<tlm::tlm_generic_payload*>& GetChildTrans();
    bool NotifyChildTransCompletion();

    static bool NotifyChildTransCompletion(tlm::tlm_generic_payload& trans);

};

    } // Controller
} // dmu
#endif