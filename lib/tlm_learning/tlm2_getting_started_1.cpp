
// Filename: tlm2_getting_started_1.cpp

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


// Getting Started with TLM-2.0, Example 1

// Shows the blocking transport interface with the generic payload and simple sockets
// Shows the initiator and target modules connected through sockets
// Shows how to set attributes of the generic payload
// Shows the loosely-timed coding style

// This example corresponds to Tutorial 1


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
  : socket("socket")  // Construct and name socket
  {
    SC_THREAD(thread_process);
  }

  void thread_process()
  {
    // Allocate a single transaction for reuse
    tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload;
    sc_time delay = sc_time(10, SC_NS);

    // Generate a series of random transactions
    for (int i = 32; i < 96; i += 4)
    {
      // Randomize command (read or write)
      tlm::tlm_command cmd = static_cast<tlm::tlm_command>(rand() % 2);
      
      // Initialize data buffer
      int data = 0;
      if (cmd == tlm::TLM_WRITE_COMMAND) 
        data = 0xFF000000 | i;

      // Set all attributes of the generic payload
      trans->set_command( cmd );
      trans->set_address( i );
      trans->set_data_ptr( reinterpret_cast<unsigned char*>(&data) );
      trans->set_data_length( 4 );
      trans->set_streaming_width( 4 ); // = data_length to indicate no streaming
      trans->set_byte_enable_ptr( 0 ); // 0 indicates unused
      trans->set_dmi_allowed( false ); // Mandatory initial value
      trans->set_response_status( tlm::TLM_INCOMPLETE_RESPONSE ); // Mandatory initial value

      // Timing annotation models processing time of initiator prior to call
      delay = sc_time(10, SC_NS);

      // Call b_transport to send the transaction to the target
      cout << "trans: addr = 0x" << hex << i
           << ", cmd = " << (cmd ? "WRITE" : "READ")
           << ", data = 0x" << data
           << " at time " << sc_time_stamp() << endl;

      socket->b_transport( *trans, delay );

      // Check response status
      if ( trans->is_response_error() )
        SC_REPORT_ERROR("TLM-2", "Response error from b_transport");

      // Print read data if this was a read transaction
      if (cmd == tlm::TLM_READ_COMMAND)
      {
        cout << "trans: addr = 0x" << hex << i
             << ", read data = 0x" << *reinterpret_cast<int*>(trans->get_data_ptr())
             << " at time " << sc_time_stamp() << endl;
      }

      // Realize the timing annotation
      wait(delay);
    }

    // Clean up
    delete trans;
  }
};


// **************************************************************************************
// Target module representing a simple memory
// **************************************************************************************

struct Memory: sc_module
{
  // TLM-2 socket, defaults to 32-bits wide, base protocol
  tlm_utils::simple_target_socket<Memory> socket;

  enum { SIZE = 256 };
  int mem[SIZE];

  SC_CTOR(Memory)
  : socket("socket")
  {
    // Register callback for incoming b_transport interface method call
    socket.register_b_transport(this, &Memory::b_transport);

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

    // Check for unsupported features
    if (adr >= sc_dt::uint64(SIZE) || byt != 0 || len > 4 || wid < len)
      SC_REPORT_ERROR("TLM-2", "Target does not support given generic payload transaction");

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

    // Honor the timing annotation
    // In this simple example, we just ignore the delay (loosely-timed)
    // The initiator is responsible for realizing the timing
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
  cout << "* TLM-2.0 Getting Started Example 1         *" << endl;
  cout << "* Tutorial 1: Blocking Transport Interface  *" << endl;
  cout << "**********************************************" << endl;

  Top top("top");
  sc_start();

  cout << endl << "Simulation completed successfully!" << endl;
  return 0;
}
