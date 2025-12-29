#ifndef _CHI_PORTSTRUCT__H
#define _CHI_PORTSTRUCT__H

#include "CHIPort/CHIUtilities.h"
#include "CHIPort/PortUtilities.h"
#include "CHIPort.h"
#include <systemc>

#include <optional>
#include <set>
#include <deque>
#include <vector>
#include <list>
#include <cstdint>
#include <unordered_map>
#include <map>
#include <optional>
#include <memory>
#include <cassert>


namespace dmu
{

    struct DBField
    {
        uint64_t db_addr; // request addr
        uint64_t offset; // request addr offset, for prefetch tgt this field set zero
        uint16_t numb_bytes; //2^size
        bool is_pcmo {false}; // is pcmo operation?
        bool is_wrnosnp0 {false}; // is WriteNoSnpZero transaction type
        bool is_dateless{false}; // is dateless request (pcmo or cmo)
        bool is_flush_access {false}; // is flush access, indicate data wont write into dimm
        uint8_t rw_type; // indicate this is write or read request, 0 is read , 1 is write
        uint8_t qos{0}; // request qos value

        // uint16_t mpam; // mpam filed from request
        uint16_t src_id; // request agent src id
        // uint8_t dbid{0}; // for write transaction, this is the write data buffer index
        // uint8_t rdinfo_tag{0}; // for read transaction, this is the read info index
        union
        {
            uint8_t dbid; // for write transaction, this is the write data buffer index
            uint8_t rdinfo_tag; // for read transaction, this is the read info index
            uint8_t buffer_index;
        };

        bool is_pref_rdtype{false}; // indicate the read request is prefetch read
        uint8_t db_rmodw_info {0}; // indicate which core word need read-modify-write
        uint8_t db_rmodw_num {0}; // single write request need send the number of core words to MC
        bool db_rmodw_full_wr{true}; // for partial write, to show is full write
        ARM::CHI::Payload& payload;

        explicit DBField(const CHIFlit& req_flit);
        DBField(const CHIFlit& req_flit, const unsigned& buffer_index);
        DBField(const CHIFlit& req_flit, const unsigned& buffer_index, const unsigned& rw_type);
        DBField(const DBField& other); // copy operation
        DBField& operator=(const DBField&) = delete; // the default copy value operation forbidden
        ~DBField();
    };

    using P2C_INFO = DBField;

    struct P2cFifo
    {
        std::list<P2C_INFO> P2CFIFO;

        uint16_t P2C_FIFO_SIZE{32};
        explicit P2cFifo(unsigned config_size) : P2C_FIFO_SIZE(config_size) {}
        const unsigned size() const{ return P2CFIFO.size();}
        void Push(const CHIFlit& req_flit) {P2CFIFO.emplace_back(req_flit); }
        void Push(const CHIFlit& req_flit, const unsigned& buffer_index) {P2CFIFO.emplace_back(req_flit,buffer_index);}
        void Push(const CHIFlit& req_flit, const unsigned& buffer_index,unsigned rw_type) {P2CFIFO.emplace_back(req_flit,buffer_index,rw_type);}
    };


    struct ResponseQueues
    {
        ResponseQueues();
        ~ResponseQueues() = default;
        std::optional<CHIFlit> Pcrd_buffer; // temporally store the PCrdGrant when there are race conflit between retry ack and pcrdGrant
        std::vector<std::deque<CHIFlit>> Response_Queues{5};
        int winner_queue_index{-1}; // record
        uint8_t rtq_rd_max_qos{0};
        uint8_t rtq_wr_max_qos{0};
        bool blocked{false}; // to show whether the Retry is harzard with PcrdGrant
        inline bool is_pcrd_buffer_occupied() const {return Pcrd_buffer.has_value();}
        inline bool is_rsp_retry_avail() const { return 32 - Response_Queues.at(static_cast<unsigned>(RespQueueType::Retry)).size() > 2;}

        //
        unsigned retry_rsp_queue_size{32};



        bool HasRspPending();
        int Arbiter();
        void Push(const CHIFlit& rsp_flit, int index);
        [[maybe_unused]] const CHIFlit Pop(int index);
    };

    struct RetryResourceManager //: public sc_core::sc_module
    {
        using Qos_Srcid_Matrix = std::vector<std::vector<unsigned>>; //qos-src_id // use the map to sort the src id order
        std::vector<Qos_Srcid_Matrix> qos_srcid_matrixs; // record the type

        std::optional<unsigned> last_win_type; //
        std::vector<int> last_win_srcid_s;
        unsigned qos_timeout_threshold{2}; // configure the type time out cycles threshold
        unsigned req_type_timeout_threshold{3}; // configure the type time out cycles threshold
        std::vector<unsigned> type_timeout_counter; // timeout for type arbit
        std::vector<unsigned> type_qos_timeout_counters; // timeout for low qos arbit

        CHIPort* CHIPort_ptr;
        RetryResourceManager(CHIPort* CHIPort_ptr);
        ~RetryResourceManager();
        void cnt_inc(unsigned type, unsigned qos, unsigned src_id);//when retry, counter dec
        void cnt_dec(unsigned type, unsigned qos, unsigned src_id);//when P-credit granted, counter dec
        std::optional<unsigned> QosSelection(const unsigned& type_index);
        std::optional<unsigned> SrcIDArbiter(const unsigned& type_index,const unsigned& qos);
        std::optional<unsigned> get_type_max_qos(unsigned type_index) const;
        bool is_empty() const;
        bool is_type_empty(unsigned type) const;
        bool wr_condition{false};
        bool rd_condition{false};
        bool cmo_condition{false};
        void update_wr_condition();
        void update_rd_condition();
        void update_cmo_condition();
        void update_condition_state();
        bool pcrd_available(){return wr_condition || rd_condition || cmo_condition;}

        enum class State
        {
            Write_Grant,
            Read_Grant,
            CMO_Grant
        } state = State::Write_Grant, next_state = State::Write_Grant;
        void state_update();

        void arbiter_state_machine();
        std::tuple<unsigned, unsigned, unsigned> gen_pcrd_rsp();
    };




    struct RdataInfo
    {
        std::set<uint16_t> unused_rdata_info_id;
        std::unordered_map<uint16_t,CHIFlit> rdata_info_buffer;
        const uint8_t RDATA_INFO_SIZE{128};
        // RdataInfo();
        explicit RdataInfo(unsigned config_size = 128);
        uint16_t allocate_infotag();
        void release_infotag(uint16_t id);//need to check id is in 0-31
        const unsigned size() const {return rdata_info_buffer.size();}
    };

    struct WdataBufferEntry
    {
        uint8_t data_words[64];// if allocate, set to 64f ,if del, the mem should be clean
        uint16_t beat_count;
        WdataBufferEntry(const unsigned data_width_bytes);
        WdataBufferEntry(const CHIFlit& req_flit,const unsigned data_width_bytes);
        inline bool is_entry_ready() const { return beat_count == 0; }
    };

    struct WdataBufferArray
    {
        std::set<uint16_t> unallocated_dbid;
        std::unordered_map<uint16_t,WdataBufferEntry> data_buffer;
        std::set<uint16_t> allocated_ptl_dbid;

        const uint8_t WDAT_BUFFER_SIZE{64};
        const unsigned data_width_bytes;
        WdataBufferArray(const unsigned data_width_bytes);
        ~WdataBufferArray() = default;
        const uint16_t allocate_dbid();
        inline void insert_ptl_id(uint16_t id) {allocated_ptl_dbid.insert(id); }
        void allocate_wdat_buffer_entry(const CHIFlit& req_flit, const unsigned& dbid);
        // void allocate_wdat_buffer_entry(const CHIFlit& req_flit, const unsigned& dbid, bool isfull);
        inline void release_dbid(uint16_t id)
        {
            unallocated_dbid.insert(id);
            }//need to check id is in 0-31
        void receive_wdat_flit(const CHIFlit& dat_flit);

        const unsigned size() const{ return data_buffer.size();}
    } ;

    struct DelayCommandQueue
    {
        explicit DelayCommandQueue(WdataBufferArray* wdb_ptr);
        ~DelayCommandQueue() = default;
        std::map<uint16_t,CHIFlit> dcq_info_buffer;
        std::optional<CHIFlit> queue_head;
        std::optional<std::pair<unsigned,CHIFlit>> queue_head_;
        const uint16_t DCQ_INFO_SIZE{32};
        WdataBufferArray* wdb_ptr;

        std::optional<std::pair<unsigned,CHIFlit>> get_head();
        void allocate_dcq_buffer_entry(const CHIFlit& req_flit, const unsigned& dbid);
        void move2head(const unsigned& dbid, const CHIFlit& dcq_flit);
        const unsigned size() const { return dcq_info_buffer.size();}
        void Pop();
        void check_dcq_ready();
        /* need a function that do the dcq timeout check; when dcq is not empty the timeout check must do every cycle;
        when a dcq entry is valid, it must set a event that indicates it will be timeout in delayed cycles*/
        bool dcq_has_ready{false};
        bool dcq_timeout{false};
        unsigned timeout_counter;
        const unsigned config_timeout_num{5};
        inline bool is_timeout() const { return dcq_timeout;}
        inline bool is_ready() const { return dcq_has_ready;}
    };



    struct CMOResponseQueue
    {
        std::unordered_map<uint16_t,CHIFlit> crq_buffer;

        unsigned CRQ_SIZE{32};
        CMOResponseQueue(unsigned config_size): CRQ_SIZE(config_size) {;}
        const unsigned size() const{ return crq_buffer.size();}
    };

    struct ResourceManage
    {

        ResourceManage(CHIPort* CHIPort_ptr);
        ~ResourceManage() = default;
        CHIPort* CHIPort_ptr;
        //
        uint8_t read_pcredit_count{};
        uint8_t write_pcredit_count{};
        uint8_t cmo_pcredit_count{};
        inline void write_pcredit_inc(){++write_pcredit_count;}
        inline void write_pcredit_dec(){--write_pcredit_count;}
        inline void read_pcredit_inc(){++read_pcredit_count;}
        inline void read_pcredit_dec(){--read_pcredit_count;}
        inline void cmo_pcredit_inc(){++cmo_pcredit_count;}
        inline void cmo_pcredit_dec(){--cmo_pcredit_count;}
        //
        uint8_t rd_qos_threshold{};
        uint8_t wr_qos_threshold{};
        uint8_t cq_occupy{};

        // inline uint16_t get_rm_cq_level()
        // {return (CHIPort_ptr->delay_command_queue->size()+read_pcredit_count+CHIPort_ptr->wdata_buffer_array->size()+write_pcredit_count+cmo_pcredit_count+cq_occupy+p2c_fifo_ptr->size());}
        inline uint16_t get_rm_dcq_level()
        {
            return CHIPort_ptr->delay_command_queue->size() + write_pcredit_count;
        }

        inline uint16_t get_rm_wdq_level()
        {
            return CHIPort_ptr->wdata_buffer_array->size() + write_pcredit_count; //+granted write request
        }

        inline uint16_t get_rm_crq_level()
        {
            return CHIPort_ptr->cmo_resp_queue->size() + cmo_pcredit_count + write_pcredit_count; // + granted cmo request
        }

        inline uint16_t get_rdat_info_occupancy_total()
        {
            return CHIPort_ptr->rdata_info_queue->size() + read_pcredit_count; // + granted cmo request
        }
    
    };


}// end dmu
    






#endif // CHI_PORTSTRUCT_H