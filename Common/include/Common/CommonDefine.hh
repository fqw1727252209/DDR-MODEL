#ifndef __COMMON_DEFINE_HH__
#define __COMMON_DEFINE_HH__

#include <iostream>
#include <cstdio>

#include <systemc>

namespace dmu{

// 定义是否启用调试打印的宏
// #define ENABLE_DEBUG_PRINT

// // debug flag

#define PORT 1
#define TOP_DEBUG 1
#define WR_CAM 1
#define RD_CAM 1
#define BANK_SLICE 1
#define BANK_SLICE_MANAGER 1
#define CMD_SELECT 1
#define MODE_SWITCH 1
#define TIME_CONSTRAINT 1
#define DEVICE 1
#define MEMORY_CONTROLLER 1
#define NTT 1
#define PHY_DELAY_MODEL 1

// 基于调试标志的统一打印实现
#ifdef ENABLE_DEBUG_PRINT
#define PRINT_BUFFER_SIZE 4096
#define DPRINT(debug_flag, name,type, format, ...) \
    do { \
        if (debug_flag) { \
            char __fmt_buf[PRINT_BUFFER_SIZE] = {0}; \
            std::ostringstream __ss; \
            snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__); \
            __ss << "@" << sc_core::sc_time_stamp() << ":" << __fmt_buf; \
            SC_REPORT_##type(name, __ss.str().c_str()); \
        } \
    } while (0)

// 改进的断言宏，当断言失败时会打印额外的信息
#define DPRINT_ASSERT(condition ,name, format, ...) \
    do { \
        if (!(condition)) { \
            char __fmt_buf[PRINT_BUFFER_SIZE] = {0}; \
            std::ostringstream __ss; \
            snprintf(__fmt_buf, sizeof(__fmt_buf), format, ##__VA_ARGS__); \
            __ss << "@" << sc_core::sc_time_stamp() << ":" << __fmt_buf; \
            SC_REPORT_FATAL(name, __ss.str().c_str()); \
        } \
    } while(0)
#else
#define DPRINT(debug_flag, type, format, ...) do {} while (0)
#define DPRINT_ASSERT(condition ,name, format, ...) do {} while (0)
#endif
// example: DPRINTF(DEBUG_DRAMSIM3, "Instantiated DRAMsim3 with clock %d ns and queue size %d\n", 100, 10);
// 不同级别打印的封装宏
#define DPRINT_INFO(debug_flag, name, format, ...) DPRINT(debug_flag, name, INFO, format, ##__VA_ARGS__)
// warning will show the __FILE__ and __LINE__
#define DPRINT_WARNING(debug_flag, name, format, ...) DPRINT(debug_flag, name, WARNING, format, ##__VA_ARGS__)
// error will call the progma error, and exist the progma
#define DPRINT_ERROR(name, format, ...) DPRINT(true, name, ERROR, format, ##__VA_ARGS__)
// fatal will call the progma abort
#define DPRINT_FATAL(name, format, ...) DPRINT(true, name, FATAL, format, ##__VA_ARGS__)

#define ABORT_MESSAGE(msg) \
    std::cerr << "Aborting at " << __FILE__ << ":" << __LINE__ << " : " << msg << std::endl; \
    std::abort();

// #define XREPORT_BASE(type,msg_stream) \
//     XREPORT_PLAIN_BASE(type, "@" << sc_core::sc_time_stamp()<<" , "<< msg_stream)

// #define XREPORT_PLAIN_BASE(type, msg_stream) do { \
//     std::ostringstream __ss; \
//     __ss << msg_stream; \
//     SC_REPORT_##type(name(), __ss.str().c_str()); \
// } while (0)

// #define XREPORT_INFO(msg_stream)     XREPORT_BASE(INFO, msg_stream)
// #define XREPORT_WARNING(msg_stream)  XREPORT_BASE(WARNING, msg_stream)
// #define XREPORT_ERROR(msg_stream)    XREPORT_BASE(ERROR, msg_stream)
// #define XREPORT_FATAL(msg_stream)    XREPORT_BASE(FATAL, msg_stream)
// #define XREPORT(msg_stream)          XREPORT_INFO(msg_stream)
// #define XREPORT_PLAIN(msg_stream)    XREPORT_PLAIN_BASE(INFO, msg_stream)

}

#endif