
// Filename: tlm2_getting_started_2.cpp

//----------------------------------------------------------------------
//  Copyright (c) 2007-2008 by Doulos Ltd.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//----------------------------------------------------------------------

// Version 2  19-June-2008 - updated for TLM-2.0


// Getting Started with TLM-2.0, Example 2

// Shows response status in the generic payload
// Shows the Direct Memory Interface (DMI)
// Shows the debug transport interface

// This example corresponds to Tutorial 2


// Needed for the simple_target_socket
#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "systemc"
using namespace sc_core;
using namespace sc_dt;
using namespace std;

#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"


// **************************************************************************************
// Initiator module generating generic payload transactions
// **************************************************************************************

struct Initiator: sc_module
{
  // TLM-2 socket, defaults to 32-bits wide, base protocol
  tlm_utils::simple_initiator_socket<Initiator> socket;

  SC_CTOR(Initiator)
  : socket("socket")
  , dmi_ptr_valid(false)
  {
    // Register callback for incoming invalidate_direct_mem_ptr
    socket.register_invalidate_direct_mem_ptr(this, &Initiator::invalidate_direct_mem_ptr);

    SC_THREAD(thread_process);
  }

  void thread_process()
  {
    // Allocate a single transaction for reuse
    tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload;
    sc_time delay = sc_time(10, SC_NS);

    cout << "\n*** Starting normal transactions ***\n" << endl;

    // Generate a series of random transactions
    for (int i = 32; i < 96; i += 4)
    {
      // Randomize command (read or write)
      tlm::tlm_command cmd = static_cast<tlm::tlm_command>(rand() % 2);
      int data = 0;
      
      if (cmd == tlm::TLM_WRITE_COMMAND) 
        data = 0xFF000000 | i;

      // Check if we can use DMI
      if (dmi_ptr_valid)
      {
        // Use DMI for direct memory access
        if ( cmd == tlm::TLM_READ_COMMAND )
        {
          assert( dmi_data.is_read_allowed() );
          memcpy(&data, dmi_data.get_dmi_ptr() + i, 4);
          
          cout << "DMI READ  at 0x" << hex << i
               << " data = 0x" << data
               << " at time " << sc_time_stamp() << endl;
          
          wait( dmi_data.get_read_latency() );
        }
        else if ( cmd == tlm::TLM_WRITE_COMMAND )
        {
          assert( dmi_data.is_write_allowed() );
          memcpy(dmi_data.get_dmi_ptr() + i, &data, 4);
          
          cout << "DMI WRITE at 0x" << hex << i
               << " data = 0x" << data
               << " at time " << sc_time_stamp() << endl;
          
          wait( dmi_data.get_write_latency() );
        }
      }
      else
      {
        // Use b_transport for normal transaction
        trans->set_command( cmd );
        trans->set_address( i );
        trans->set_data_ptr( reinterpret_cast<unsigned char*>(&data) );
        trans->set_data_length( 4 );
        trans->set_streaming_width( 4 );
        trans->set_byte_enable_ptr( 0 );
        trans->set_dmi_allowed( false );
        trans->set_response_status( tlm::TLM_INCOMPLETE_RESPONSE );

        delay = sc_time(10, SC_NS);

        cout << "b_transport: addr = 0x" << hex << i
             << ", cmd = " << (cmd ? "WRITE" : "READ")
             << ", data = 0x" << data
             << " at time " << sc_time_stamp() << endl;

        socket->b_transport( *trans, delay );

        // Check response status using convenience methods
        if ( trans->is_response_error() )
        {
          char txt[100];
          sprintf(txt, "Error from b_transport, response status = %s",
                  trans->get_response_string().c_str());
          SC_REPORT_ERROR("TLM-2", txt);
        }

        // Print read data if this was a read transaction
        if (cmd == tlm::TLM_READ_COMMAND)
        {
          cout << "    Read data = 0x" << hex 
               << *reinterpret_cast<int*>(trans->get_data_ptr()) << endl;
        }

        // Check DMI hint and request DMI pointer if available
        if ( trans->is_dmi_allowed() )
        {
          cout << "    DMI hint received, requesting DMI pointer" << endl;
          dmi_ptr_valid = socket->get_direct_mem_ptr( *trans, dmi_data );
          
          if (dmi_ptr_valid)
          {
            cout << "    DMI granted for range 0x" << hex 
                 << dmi_data.get_start_address() << " to 0x"
                 << dmi_data.get_end_address() << endl;
          }
        }

        wait(delay);
      }
    }

    cout << "\n*** Testing error responses ***\n" << endl;

    // Test address error
    trans->set_command( tlm::TLM_READ_COMMAND );
    trans->set_address( 10000 );  // Out of range
    trans->set_data_ptr( reinterpret_cast<unsigned char*>(&data) );
    trans->set_data_length( 4 );
    trans->set_streaming_width( 4 );
    trans->set_byte_enable_ptr( 0 );
    trans->set_dmi_allowed( false );
    trans->set_response_status( tlm::TLM_INCOMPLETE_RESPONSE );

    delay = sc_time(10, SC_NS);
    socket->b_transport( *trans, delay );

    if ( trans->is_response_error() )
    {
      cout << "Expected error: " << trans->get_response_string().c_str() << endl;
    }

    cout << "\n*** Using debug transport to dump memory ***\n" << endl;

    // Use debug transport to read memory contents
    trans->set_address(0);
    trans->set_read();
    trans->set_data_length(128);

    unsigned char* debug_data = new unsigned char[128];
    trans->set_data_ptr(debug_data);

    unsigned int n_bytes = socket->transport_dbg( *trans );

    cout << "Debug read " << dec << n_bytes << " bytes:" << endl;
    for (unsigned int i = 0; i < n_bytes; i += 4)
    {
      cout << "  mem[" << dec << i << "] = 0x" << hex
           << *(reinterpret_cast<unsigned int*>( &debug_data[i] )) << endl;
    }

    // Clean up
    delete[] debug_data;
    delete trans;
  }

  // Invalidate DMI pointer callback
  virtual void invalidate_direct_mem_ptr(sc_dt::uint64 start_range,
                                         sc_dt::uint64 end_range)
  {
    cout << "DMI invalidated for range 0x" << hex 
         << start_range << " to 0x" << end_range << endl;
    dmi_ptr_valid = false;
  }

  bool dmi_ptr_valid;
  tlm::tlm_dmi dmi_data;
  int data;
};


// **************************************************************************************
// Target module representing a simple memory
// **************************************************************************************

struct Memory: sc_module
{
  // TLM-2 socket, defaults to 32-bits wide, base protocol
  tlm_utils::simple_target_socket<Memory> socket;

  enum { SIZE = 256, LATENCY = 10 };
  int mem[SIZE];

  SC_CTOR(Memory)
  : socket("socket")
  {
    // Register callbacks for incoming interface method calls
    socket.register_b_transport(this, &Memory::b_transport);
    socket.register_get_direct_mem_ptr(this, &Memory::get_direct_mem_ptr);
    socket.register_transport_dbg(this, &Memory::transport_dbg);

    // Initialize memory with some default values
    for (int i = 0; i < SIZE; i++)
      mem[i] = 0xAA000000 | (i * 4);
  }

  // TLM-2 blocking transport method
  virtual void b_transport( tlm::tlm_generic_payload& trans, sc_time& delay )
  {
    tlm::tlm_command cmd = trans.get_command();
    sc_dt::uint64    adr = trans.get_address() / 4;
    unsigned char*   ptr = trans.get_data_ptr();
    unsigned int     len = trans.get_data_length();
    unsigned char*   byt = trans.get_byte_enable_ptr();
    unsigned int     wid = trans.get_streaming_width();

    // Check for address error
    if (adr >= sc_dt::uint64(SIZE)) {
      trans.set_response_status( tlm::TLM_ADDRESS_ERROR_RESPONSE );
      cout << "      Memory: ADDRESS ERROR at 0x" << hex << (adr * 4) << endl;
      return;
    }

    // Check for byte enable error
    if (byt != 0) {
      trans.set_response_status( tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE );
      cout << "      Memory: BYTE ENABLE ERROR" << endl;
      return;
    }

    // Check for burst error
    if (len > 4 || wid < len) {
      trans.set_response_status( tlm::TLM_BURST_ERROR_RESPONSE );
      cout << "      Memory: BURST ERROR" << endl;
      return;
    }

    // Execute the read or write command
    if ( cmd == tlm::TLM_READ_COMMAND )
    {
      memcpy(ptr, &mem[adr], len);
      cout << "      Memory READ  at 0x" << hex << (adr * 4)
           << " data = 0x" << mem[adr]
           << " at time " << sc_time_stamp() << endl;
    }
    else if ( cmd == tlm::TLM_WRITE_COMMAND )
    {
      memcpy(&mem[adr], ptr, len);
      cout << "      Memory WRITE at 0x" << hex << (adr * 4)
           << " data = 0x" << mem[adr]
           << " at time " << sc_time_stamp() << endl;
    }

    // Set response status to indicate successful completion
    trans.set_response_status( tlm::TLM_OK_RESPONSE );

    // Set DMI hint to indicate DMI is available
    trans.set_dmi_allowed(true);
  }

  // TLM-2 DMI method
  virtual bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                                  tlm::tlm_dmi& dmi_data)
  {
    cout << "      Memory: DMI request received" << endl;

    // Grant read and write access
    dmi_data.allow_read_write();

    // Set DMI pointer to base of memory array
    dmi_data.set_dmi_ptr( reinterpret_cast<unsigned char*>( &mem[0] ) );

    // Set DMI address range
    dmi_data.set_start_address( 0 );
    dmi_data.set_end_address( SIZE*4-1 );

    // Set DMI latencies
    dmi_data.set_read_latency( sc_time(LATENCY, SC_NS) );
    dmi_data.set_write_latency( sc_time(LATENCY, SC_NS) );

    return true;
  }

  // TLM-2 debug transport method
  virtual unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
  {
    tlm::tlm_command cmd = trans.get_command();
    sc_dt::uint64    adr = trans.get_address() / 4;
    unsigned char*   ptr = trans.get_data_ptr();
    unsigned int     len = trans.get_data_length();

    // Calculate number of bytes to copy (don't exceed memory bounds)
    unsigned int num_bytes = (len < (SIZE - adr) * 4) ? len : (SIZE - adr) * 4;

    // Execute read or write (no side effects, no time consumed)
    if ( cmd == tlm::TLM_READ_COMMAND )
      memcpy(ptr, &mem[adr], num_bytes);
    else if ( cmd == tlm::TLM_WRITE_COMMAND )
      memcpy(&mem[adr], ptr, num_bytes);

    cout << "      Memory: Debug transport " << (cmd ? "WRITE" : "READ")
         << " " << dec << num_bytes << " bytes" << endl;

    return num_bytes;
  }
};


// **************************************************************************************
// Top-level module instantiating initiator and target and binding sockets
// **************************************************************************************

SC_MODULE(Top)
{
  Initiator *initiator;
  Memory    *memory;

  SC_CTOR(Top)
  {
    // Instantiate components
    initiator = new Initiator("initiator");
    memory    = new Memory   ("memory");

    // Bind initiator socket to target socket
    initiator->socket.bind( memory->socket );
  }
};


// **************************************************************************************
// Main function
// **************************************************************************************

int sc_main(int argc, char* argv[])
{
  cout << "**********************************************" << endl;
  cout << "* TLM-2.0 Getting Started Example 2         *" << endl;
  cout << "* Tutorial 2: Response Status, DMI, Debug   *" << endl;
  cout << "**********************************************" << endl;

  Top top("top");
  sc_start();

  cout << endl << "Simulation completed successfully!" << endl;
  return 0;
}
