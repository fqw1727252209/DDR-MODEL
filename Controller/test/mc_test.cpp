#include <memory>



#include <chrono>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>


#include "Configure/DDR5MemSpec.hh"
#include "Configure/DDR5MemSpec3ds.hh"

#include "Configure/LoadConfigure.hh"
#include "Configure/AddressDecoder.hh"
#include "Controller/MemoryController.hh"
#include "Controller/MemoryDevice.hh"

#include "Common/UifExtension.hh"
#include "Controller/MrdimmController.hh"
#include "Controller/PhyDelayModel.hh"
#include "Controller/common/MemoryManager.hh"
#include "Configure/Configure.hh"

#include "Common/UifMaster.hh"
#include "sysc/communication/sc_clock.h"
#include "sysc/kernel/sc_simcontext.h"
#include "sysc/kernel/sc_time.h"

SC_MODULE(iSocket)
{
public:
    iSocket(const sc_core::sc_module_name& name): sc_module(name),
    iniSocket("iniSocket")
    {
        std::cout << " iSocket creat" <<std::endl;
    }
    tlm_utils::simple_initiator_socket<iSocket> iniSocket;

};
using namespace dmu;
using namespace dmu::Controller;
using namespace tlm;
using namespace sc_core;
int sc_main(int arg, char** argv)
{
    if (arg < 4) {
        std::cerr << "Usage: " << argv[0] << " <config_path> <traffic_type> <thread_num> [trans_num]" << std::endl;
        std::cerr << "  config_path: path to config json file (e.g., ../../ConfigureFile/mrdimm_map5.json)" << std::endl;
        std::cerr << "  traffic_type: 0=Stream_Rd, 1=Stream_Wr, 2=Random_Rd, 3=Random_Wr, 4=Stream_Copy, 5=Stream_Add, 6=Random_Copy, 7=Random_Add" << std::endl;
        std::cerr << "  thread_num: number of traffic sources (>= 1)" << std::endl;
        std::cerr << "  trans_num: loop count per source, default=10240" << std::endl;
        return -1;
    }

    std::string config_path_str = argv[1];
    std::filesystem::path config_path(config_path_str);
    std::string config_dir = config_path.parent_path().string();
    std::string config_file = config_path.filename().string();
    if (config_dir.empty()) {
        config_dir = ".";
    }

    int traffic_type_input = atoi(argv[2]);
    if (traffic_type_input < 0 || traffic_type_input > 7) {
        std::cerr << "Invalid traffic type. Must be between 0 and 7." << std::endl;
        return -1;
    }

    TrafficType traffic_type = static_cast<TrafficType>(traffic_type_input);

    unsigned thread_num = atoi(argv[3]);
    if (thread_num < 1) {
        std::cerr << "Invalid thread_num. Must be >= 1." << std::endl;
        return -1;
    }

    unsigned trans_num = 10240;
    if (arg >= 5) {
        trans_num = atoi(argv[4]);
    }

    // 创建基于JSON文件名、流量类型、thread_num和trans_num的输出目录
    std::string base_name = config_path.stem().string();

    std::string traffic_names[] = {"Stream_Rd", "Stream_Wr", "Random_Rd", "Random_Wr",
                                   "Stream_Copy", "Stream_Add", "Random_Copy", "Random_Add"};

    std::string output_dir = "Output_Dir/" + base_name + "/" + traffic_names[traffic_type]
        + "_t" + std::to_string(thread_num) + "_n" + std::to_string(trans_num);

    // 创建输出目录
    std::filesystem::create_directories(output_dir);
    std::cout << "Output directory: " << output_dir << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    uint64_t test_address = 0x4fff'ff21'3323'4023;


    LoadConfigure loader(config_dir, config_file);
    loader.ParseJson();
    loader.ParseConfig();

    Configure config(loader,output_dir);
    std::unique_ptr<SdramConstraintDDR5_3ds> sdram_constraint = std::make_unique<SdramConstraintDDR5_3ds>(*config.mem_spec);

    MrdimmController mc("mc",config,sdram_constraint.get(),output_dir);
    PhyDelayModel phy_delay_model("phy_delay_model",config,output_dir);
    MemoryDevice md("md",config,output_dir);
    UifMaster uif_master0("uif_master0",traffic_type,config,thread_num,trans_num);
    UifMaster uif_master1("uif_master1",traffic_type,config,thread_num,trans_num);
    Logger::getInstance("Statisic").initialize("Statisic",output_dir,"Statisic");

    sc_core::sc_clock dfi_clk("clk",config.mem_spec->tCK_mc);

    uif_master0.iSocket.bind(*mc.tSocket[0]);
    uif_master1.iSocket.bind(*mc.tSocket[1]);
    mc.iSocket.bind(phy_delay_model.tSocket);
    phy_delay_model.iSocket.bind(md.tSocket);
    mc.bind_dfi_clock(dfi_clk);

    sc_core::sc_start(5,sc_core::SC_MS,sc_core::SC_EXIT_ON_STARVATION);
    // if (not sc_end_of_simulation_invoked()) {
    //     SC_REPORT_INFO("Test", "ERROR: Simulation stopped without explicit sc_stop()");
    //     sc_stop();
    // }//endif
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "程序运行时间：" << duration.count() << " 秒" << std::endl;
    return 0;
}

// g++ -o mc_test mc_test.cpp -lsystemc -lcci -I$SYSTEMC_HOME/include -I/root/pro_code/DDR_TLM/Controller/include -I$SYSTEMC_HOME/include/rapidjson/include -I$SYSTEMC_HOME/include/cci -L$SYSTEMC_HOME/lib -std=c++17 -rdynamic