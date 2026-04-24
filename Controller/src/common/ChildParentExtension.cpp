#include <vector>

#include <tlm>

#include "Controller/common/ChildParentExtension.hh"

namespace dmu{
    namespace Controller{


tlm::tlm_extension_base*
ChildExtension::clone() const
{
    return new ChildExtension(*_parent_trans);
}

void
ChildExtension::copy_from(const tlm::tlm_extension_base& ext)
{
    const auto& cpyFrom = dynamic_cast<const ChildExtension&>(ext);
    _parent_trans = cpyFrom._parent_trans;
}

tlm::tlm_generic_payload&
ChildExtension::GetParentTrans()
{
    return *_parent_trans;
}

tlm::tlm_generic_payload&
ChildExtension::GetParentTrans(tlm::tlm_generic_payload& childTrans)
{
    return childTrans.get_extension<ChildExtension>()->GetParentTrans();
}

void
ChildExtension::SetExtension(tlm::tlm_generic_payload& childTrans,
                             tlm::tlm_generic_payload& _parent_trans)
{
    auto* extension = childTrans.get_extension<ChildExtension>();

    if (extension != nullptr)
    {
        extension->_parent_trans = &_parent_trans;
    }
    else
    {
        extension = new ChildExtension(_parent_trans);
        childTrans.set_auto_extension(extension);
    }
}

bool
ChildExtension::IsChildTrans(const tlm::tlm_generic_payload& trans)
{
    return trans.get_extension<ChildExtension>() != nullptr;
}

tlm::tlm_extension_base*
ParentExtension::clone() const
{
    return new ParentExtension(_child_transes);
}

void
ParentExtension::copy_from(const tlm_extension_base& ext)
{
    const auto& cpyFrom = dynamic_cast<const ParentExtension&>(ext);
    _child_transes = cpyFrom._child_transes;
}

void
ParentExtension::SetExtension(tlm::tlm_generic_payload& _parent_trans,
                              std::vector<tlm::tlm_generic_payload*> _child_transes)
{
    auto* extension = _parent_trans.get_extension<ParentExtension>();

    if (extension != nullptr)
    {
        extension->_child_transes = std::move(_child_transes);
        extension->completed_child_transes = 0;
    }
    else
    {
        extension = new ParentExtension(std::move(_child_transes));
        _parent_trans.set_auto_extension(extension);
    }
}

const std::vector<tlm::tlm_generic_payload*>&
ParentExtension::GetChildTrans()
{
    return _child_transes;
}

bool
ParentExtension::NotifyChildTransCompletion()
{
    completed_child_transes++;
    if (completed_child_transes == _child_transes.size())
    {
        std::for_each(_child_transes.begin(),
                 _child_transes.end(),
                 [](tlm::tlm_generic_payload* childTrans) { childTrans->release(); });
        _child_transes.clear();
        return true;
    }

    return false;
}

bool
ParentExtension::NotifyChildTransCompletion(tlm::tlm_generic_payload& trans)
{
    return trans.get_extension<ParentExtension>()->NotifyChildTransCompletion();
}

    } // Controller
} // dmu