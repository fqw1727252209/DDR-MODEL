
// Filename: tlm2_getting_started_3.cpp

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


// Getting Started with TLM-2.0, Example 3

// Shows an interconnect component between initiator and target
// Shows address translation and transaction routing
// Shows how to use tagged sockets for multiple initiator sockets
// Shows routing for b_transport, DMI, and debug transport

// This example corresponds to Tutorial 3


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
    tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload;
    sc_time delay = sc_time(10, SC_NS);

    cout << "\n*** Starting transactions to multiple memories ***\n" << endl;

    // Generate transactions to different memory locations
    // Address space: Memory[0]: 0x000-0x0FF, Memory[1]: 0x100-0x1FF,
    //                Memory[2]: 0x200-0x2FF, Memory[3]: 0x300-0x3FF
    for (int i = 0; i < 4; i++)  // Iterate through different memories
    {
      for (int j = 0; j < 4; j++)  // Multiple accesses per memory
      {
        int addr = (i << 8) | (j * 4);  // Construct address
        
        tlm::tlm_command cmd = static_cast<tlm::tlm_command>(rand() % 2);
        int data = 0;
        
        if (cmd == tlm::TLM_WRITE_COMMAND) 
          data = 0xAA000000 | addr;

        // Check if we can use DMI
        if (dmi_ptr_valid && addr >= dmi_data.get_start_address() 
                          && addr <= dmi_data.get_end_address())
        {
          // Use DMI for direct memory access
          if ( cmd == tlm::TLM_READ_COMMAND )
          {
            assert( dmi_data.is_read_allowed() );
            memcpy(&data, dmi_data.get_dmi_ptr() + addr, 4);
            
            cout << "DMI READ  at 0x" << hex << addr
                 << " (Memory[" << dec << i << "])"
                 << " data = 0x" << hex << data
                 << " at time " << sc_time_stamp() << endl;
            
            wait( dmi_data.get_read_latency() );
          }
          else if ( cmd == tlm::TLM_WRITE_COMMAND )
          {
            assert( dmi_data.is_write_allowed() );
            memcpy(dmi_data.get_dmi_ptr() + addr, &data, 4);
            
            cout << "DMI WRITE at 0x" << hex << addr
                 << " (Memory[" << dec << i << "])"
                 << " data = 0x" << hex << data
                 << " at time " << sc_time_stamp() << endl;
            
            wait( dmi_data.get_write_latency() );
          }
        }
        else
        {
          // Use b_transport for normal transaction
          trans->set_command( cmd );
          trans->set_address( addr );
          trans->set_data_ptr( reinterpret_cast<unsigned char*>(&data) );
          trans->set_data_length( 4 );
          trans->set_streaming_width( 4 );
          trans->set_byte_enable_ptr( 0 );
          trans->set_dmi_allowed( false );
          trans->set_response_status( tlm::TLM_INCOMPLETE_RESPONSE );

          delay = sc_time(10, SC_NS);

          cout << "b_transport: addr = 0x" << hex << addr
               << " (Memory[" << dec << i << "])"
               << ", cmd = " << (cmd ? "WRITE" : "READ")
               << ", data = 0x" << hex << data
               << " at time " << sc_time_stamp() << endl;

          socket->b_transport( *trans, delay );

          // Check response status
          if ( trans->is_response_error() )
          {
            char txt[100];
            sprintf(txt, "Error from b_transport, response status = %s",
                    trans->get_response_string().c_str());
            SC_REPORT_ERROR("TLM-2", txt);
          }

          // Print read data
          if (cmd == tlm::TLM_READ_COMMAND)
          {
            cout << "    Read data = 0x" << hex 
                 << *reinterpret_cast<int*>(trans->get_data_ptr()) << endl;
          }

          // Check DMI hint and request DMI pointer if available
          if ( trans->is_dmi_allowed() && !dmi_ptr_valid )
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
    }

    cout << "\n*** Using debug transport to dump all memories ***\n" << endl;

    // Use debug transport to read each memory
    for (int i = 0; i < 4; i++)
    {
      int base_addr = i << 8;
      
      trans->set_address(base_addr);
      trans->set_read();
      trans->set_data_length(64);

      unsigned char* debug_data = new unsigned char[64];
      trans->set_data_ptr(debug_data);

      unsigned int n_bytes = socket->transport_dbg( *trans );

      cout << "Memory[" << dec << i << "] debug read " << n_bytes << " bytes:" << endl;
      for (unsigned int j = 0; j < n_bytes && j < 64; j += 4)
      {
        cout << "  mem[0x" << hex << (base_addr + j) << "] = 0x"
             << *(reinterpret_cast<unsigned int*>( &debug_data[j] )) << endl;
      }

      delete[] debug_data;
    }

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
// Router (Interconnect component) with address decoding
// **************************************************************************************

template<unsigned int N_TARGETS>
struct Router: sc_module
{
  // Single target socket (receives from initiator)
  tlm_utils::simple_target_socket<Router> target_socket;
  
  // Multiple tagged initiator sockets (forward to targets)
  tlm_utils::simple_initiator_socket_tagged<Router>* initiator_socket[N_TARGETS];

  SC_CTOR(Router)
  : target_socket("target_socket")
  {
    // Register callbacks for target socket (forward path)
    target_socket.register_b_transport(this, &Router::b_transport);
    target_socket.register_get_direct_mem_ptr(this, &Router::get_direct_mem_ptr);
    target_socket.register_transport_dbg(this, &Router::transport_dbg);

    // Create and register initiator sockets with tags
    for (unsigned int i = 0; i < N_TARGETS; i++)
    {
      char txt[20];
      sprintf(txt, "socket_%d", i);
      initiator_socket[i] = 
          new tlm_utils::simple_initiator_socket_tagged<Router>(txt);

      // Register callback for backward path, with tag i
      initiator_socket[i]->register_invalidate_direct_mem_ptr(
          this, &Router::invalidate_direct_mem_ptr, i);
    }
  }

  // Address decoding: extract target number and local address
  inline unsigned int decode_address( sc_dt::uint64 address,
                                      sc_dt::uint64& masked_address )
  {
    // Address bits [9:8] select target (0-3)
    // Address bits [7:0] give local address within target
    unsigned int target_nr = static_cast<unsigned int>( (address >> 8) & 0x3 );
    masked_address = address & 0xFF;
    
    return target_nr;
  }

  // Address composition: combine target number and local address
  inline sc_dt::uint64 compose_address( unsigned int target_nr,
                                        sc_dt::uint64 address)
  {
    return (target_nr << 8) | (address & 0xFF);
  }

  // TLM-2 blocking transport method
  virtual void b_transport( tlm::tlm_generic_payload& trans, sc_time& delay )
  {
    sc_dt::uint64 address = trans.get_address();
    sc_dt::uint64 masked_address;
    unsigned int target_nr = decode_address( address, masked_address);

    cout << "      Router: Routing to Memory[" << dec << target_nr 
         << "], local addr = 0x" << hex << masked_address << endl;

    // Modify address to local address
    trans.set_address( masked_address );

    // Forward transaction to appropriate target
    ( *initiator_socket[target_nr] )->b_transport( trans, delay );

    // Note: Address remains modified (local) when returning to initiator
    // In a real system, you might want to restore it, but it's not required
  }

  // TLM-2 DMI method (forward path)
  virtual bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                                  tlm::tlm_dmi& dmi_data)
  {
    sc_dt::uint64 masked_address;
    unsigned int target_nr = decode_address( trans.get_address(),
                                             masked_address );
    
    cout << "      Router: DMI request to Memory[" << dec << target_nr << "]" << endl;
    
    // Modify address to local address
    trans.set_address( masked_address );

    // Forward DMI request to target
    bool status = ( *initiator_socket[target_nr] )->
                     get_direct_mem_ptr( trans, dmi_data );

    // Translate returned DMI address range back to global addresses
    dmi_data.set_start_address(
      compose_address( target_nr, dmi_data.get_start_address() ));
    dmi_data.set_end_address(
      compose_address( target_nr, dmi_data.get_end_address() ));

    cout << "      Router: DMI range translated to 0x" << hex 
         << dmi_data.get_start_address() << " - 0x"
         << dmi_data.get_end_address() << endl;

    return status;
  }

  // TLM-2 DMI invalidate method (backward path)
  virtual void invalidate_direct_mem_ptr(int id,
                                         sc_dt::uint64 start_range,
                                         sc_dt::uint64 end_range)
  {
    cout << "      Router: DMI invalidate from Memory[" << dec << id << "]" << endl;
    
    // Translate local address range to global address range
    // The 'id' tag tells us which target is invalidating
    sc_dt::uint64 bw_start_range = compose_address( id, start_range );
    sc_dt::uint64 bw_end_range   = compose_address( id, end_range );
    
    // Forward invalidation to initiator
    target_socket->invalidate_direct_mem_ptr(bw_start_range, bw_end_range);
  }

  // TLM-2 debug transport method
  virtual unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
  {
    sc_dt::uint64 masked_address;
    unsigned int target_nr = decode_address( trans.get_address(), 
                                             masked_address );
    
    // Modify address to local address
    trans.set_address( masked_address );

    // Forward debug transaction to appropriate target
    return ( *initiator_socket[target_nr] )->transport_dbg( trans );
  }
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
    // Register callbacks
    socket.register_b_transport(this, &Memory::b_transport);
    socket.register_get_direct_mem_ptr(this, &Memory::get_direct_mem_ptr);
    socket.register_transport_dbg(this, &Memory::transport_dbg);

    // Initialize memory
    for (int i = 0; i < SIZE; i++)
      mem[i] = 0xBB000000 | (i * 4);
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

    // Check for errors
    if (adr >= sc_dt::uint64(SIZE)) {
      trans.set_response_status( tlm::TLM_ADDRESS_ERROR_RESPONSE );
      return;
    }
    if (byt != 0) {
      trans.set_response_status( tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE );
      return;
    }
    if (len > 4 || wid < len) {
      trans.set_response_status( tlm::TLM_BURST_ERROR_RESPONSE );
      return;
    }

    // Execute transaction
    if ( cmd == tlm::TLM_READ_COMMAND )
    {
      memcpy(ptr, &mem[adr], len);
      cout << "        Memory READ  at local 0x" << hex << (adr * 4)
           << " data = 0x" << mem[adr] << endl;
    }
    else if ( cmd == tlm::TLM_WRITE_COMMAND )
    {
      memcpy(&mem[adr], ptr, len);
      cout << "        Memory WRITE at local 0x" << hex << (adr * 4)
           << " data = 0x" << mem[adr] << endl;
    }

    trans.set_response_status( tlm::TLM_OK_RESPONSE );
    trans.set_dmi_allowed(true);
  }

  // TLM-2 DMI method
  virtual bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                                  tlm::tlm_dmi& dmi_data)
  {
    cout << "        Memory: DMI granted" << endl;

    dmi_data.allow_read_write();
    dmi_data.set_dmi_ptr( reinterpret_cast<unsigned char*>( &mem[0] ) );
    dmi_data.set_start_address( 0 );
    dmi_data.set_end_address( SIZE*4-1 );
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

    unsigned int num_bytes = (len < (SIZE - adr) * 4) ? len : (SIZE - adr) * 4;

    if ( cmd == tlm::TLM_READ_COMMAND )
      memcpy(ptr, &mem[adr], num_bytes);
    else if ( cmd == tlm::TLM_WRITE_COMMAND )
      memcpy(&mem[adr], ptr, num_bytes);

    return num_bytes;
  }
};


// **************************************************************************************
// Top-level module instantiating components and binding sockets
// **************************************************************************************

SC_MODULE(Top)
{
  Initiator* initiator;
  Router<4>* router;
  Memory*    memory[4];

  SC_CTOR(Top)
  {
    // Instantiate components
    initiator = new Initiator("initiator");
    router    = new Router<4>("router");
    
    for (int i = 0; i < 4; i++)
    {
      char txt[20];
      sprintf(txt, "memory_%d", i);
      memory[i] = new Memory(txt);
    }

    // Bind initiator to router's target socket
    initiator->socket.bind( router->target_socket );
    
    // Bind router's initiator sockets to memories
    for (int i = 0; i < 4; i++)
      router->initiator_socket[i]->bind( memory[i]->socket );
  }
};


// **************************************************************************************
// Main function
// **************************************************************************************

int sc_main(int argc, char* argv[])
{
  cout << "**********************************************" << endl;
  cout << "* TLM-2.0 Getting Started Example 3         *" << endl;
  cout << "* Tutorial 3: Routing through Interconnect  *" << endl;
  cout << "**********************************************" << endl;
  cout << "\nAddress Map:" << endl;
  cout << "  Memory[0]: 0x000 - 0x0FF" << endl;
  cout << "  Memory[1]: 0x100 - 0x1FF" << endl;
  cout << "  Memory[2]: 0x200 - 0x2FF" << endl;
  cout << "  Memory[3]: 0x300 - 0x3FF" << endl;

  Top top("top");
  sc_start();

  cout << endl << "Simulation completed successfully!" << endl;
  return 0;
}
