#ifndef __SDRAM_CONSTRAINT_HH__
#define __SDRAM_CONSTRAINT_HH__
#include <vector>
#include <deque>

#include "Configure/DDR5MemSpec3ds.hh"
#include "Controller/BankSlice.hh"

namespace dmu{
    namespace Controller{

    using BankIndex = unsigned;
    using BankGroupIndex= unsigned;
    using LrankIndex = unsigned;
    using PrankIndex = unsigned;
    using ChannelIndex = unsigned;

    class SdramConstraintIF
    {
        protected:
            SdramConstraintIF(const SdramConstraintIF&) = default;
            SdramConstraintIF(SdramConstraintIF&&) = default;
            SdramConstraintIF& operator=(const SdramConstraintIF&) = default;
            SdramConstraintIF& operator=(SdramConstraintIF&&) = default;
        public:
            SdramConstraintIF() = default;
            virtual ~SdramConstraintIF() = default;
            virtual sc_core::sc_time TimeToSatisfyConstraints(Command command,BankAddress address) const = 0;
            virtual void InsertCommand(Command command,BankAddress address) = 0;

    };


    class SdramConstraintDDR5_3ds: public SdramConstraintIF
    {

    public:
        explicit SdramConstraintDDR5_3ds(const DDR5MemSpec3ds& ddr5_memspec_3ds);
        ~SdramConstraintDDR5_3ds(){}
        sc_core::sc_time TimeToSatisfyConstraints(Command command,BankAddress address) const override;
        void InsertCommand(Command command,BankAddress address) override;
        std::vector<BankIndex> GetSameBgBankVec(BankAddress address) const;
        std::vector<BankGroupIndex> GetSameBgVec(BankAddress address) const;

    private:
        const DDR5MemSpec3ds& _ddr5_memspec_3ds;
        const bool is_x4_device;
        const sc_core::sc_time MaxTime = sc_core::sc_max_time();


        std::vector<std::vector<sc_core::sc_time>> previous_command_time4bank; // (banks,command)
        std::vector<std::vector<sc_core::sc_time>> previous_command_time4bg; // (bg, command)
        std::vector<std::vector<sc_core::sc_time>> previous_command_time4lrank; // (lranks, command)
        std::vector<std::vector<sc_core::sc_time>> previous_command_time4prank; // (pranks, command)

        std::vector<std::vector<std::vector<sc_core::sc_time>>> previous_command_time4prank_lrank; //(pranks, lranks, command)

        std::vector<std::vector<sc_core::sc_time>> previous_command_time4channel; // (channels, command)

        std::vector<std::vector<std::vector<sc_core::sc_time>>> previous_command_time4channel_prank; //(channels, pranks, command)

        std::vector<sc_core::sc_time> previous_command_time4channel_onbus;

        std::vector<std::deque<sc_core::sc_time>> last_4Activates4lrank;
        std::vector<std::deque<sc_core::sc_time>> last_4Activates4prank;

        std::vector<sc_core::sc_time> last_Activates4dimm;

    };


    } // Controller
} // dmu



#endif