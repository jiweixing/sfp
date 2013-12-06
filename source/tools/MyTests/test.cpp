#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <map>

#include "pin.H"
#include "portability.H"
#include "atomic.h"
#include "instlib.H"

using namespace std;
using namespace INSTLIB;


#include <sys/time.h>
#include <stdio.h>
#include <string.h>

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

LOCALVAR CONTROL control;

#define MAP_SIZE 0x7fffff //todo: if long is 64bit, size should be larger 
#define MAX_THREAD 17    // max thread supported
#define SETSHIFT 6
#define WORDSHIFT1 6

#define SETWIDTH 64  // a lock is of granularity 64 bytes
#define WORDWIDTH1 64  // a word is of granularity 64 bytes

#define SETMASK 63
#define WORDMASK1 63

#define SetIndex(x) (((x)>>SETSHIFT)&MAP_SIZE)
#define WordIndex1(x) ((x)&~WORDMASK1)

#define READ_ACCESS 0
#define WRITE_ACCESS 1

typedef UINT64 stamp_tt;

typedef struct sarray_elem_struct {
  stamp_tt latest;
  char next;
} Sarray_Elem;

struct Stamp {
  int tid;
  bool is_shared;
  UINT64 count;
  Stamp()
  {
    tid = -1;
    is_shared = false;
    count = 0;
  }
};

const  uint32_t              SUBLOG_BITS = 8;
const  uint32_t              MAX_WINDOW = (65-SUBLOG_BITS)*(1<<SUBLOG_BITS);

/* thread related data */ 
typedef struct map_set_struct {
  sfp_lock_t lock;
  map<ADDRINT, Stamp> set;
} Map_Set;

#include "thread_support.H"
/*
 * N: length of trace; 
 * M: data volume; 
 */
volatile static stamp_tt N = 0;
static stamp_tt M = 0;

Map_Set* stamps;


/* ===================================================================== */


VOID RecordMem(THREADID tid, VOID * ip, VOID * addr, UINT32 size, UINT32 type)
{
  /* get tls handle before while loop to save some operations */
  local_stat_t* lstat = get_tls(tid);
  if( !lstat->enabled ) return;

  /*
   * addr1 is the address of coarser granularity, 64 bytes
   * addr2 is the address of finer granularity, 4 bytes
   */
  //ADDRINT laddr2 = (~WORDMASK2)&((ADDRINT)addr);
  ADDRINT laddr1 = (~WORDMASK1)&((ADDRINT)addr);

  ADDRINT raddr2 = laddr1 + size; 

  ADDRINT set_idx;
  ADDRINT base_addr, addr1;

  /* reserve the locks for respective elements */
  for( base_addr = laddr1; base_addr < raddr2; base_addr += SETWIDTH) {
    lock_acquire(&stamps[SetIndex(base_addr)].lock);
  }

  /* atomic increment N, reserve next $size elements 
   * it has to be after all $size elements are reserved
   */

  __sync_add_and_fetch(&N, 1);
  
  for( base_addr = laddr1; base_addr < raddr2; base_addr += SETWIDTH) {

    addr1 = base_addr;

    set_idx = (stamp_tt)SetIndex(base_addr);


    // For coarse granularity
    if( addr1 < raddr2 ) {

      Stamp s;
      /* find current address's stamp */
      s = stamps[set_idx].set[addr1];
      if(s.tid >= 0 && s.tid != (int)tid)
      {
        s.is_shared = true;
      }
      s.tid = tid;
      s.count++;
      stamps[set_idx].set[addr1] = s;
    }
    lock_release(&stamps[SetIndex(base_addr)].lock);

  }
}

//
// activate instrumentation and recording
//
LOCALFUN VOID activate(THREADID tid) {
    local_stat_t* data = get_tls(tid);
    data->enabled = true;
}

//
// deactivate instrumentation and recording
//
LOCALFUN VOID deactivate(THREADID tid) {
    local_stat_t* data = get_tls(tid);
    data->enabled = false;
}

//
// emit the result
//
VOID emit(THREADID tid) {
    //TODO: implement emit stats in the middle
}

//
// reset the recrods
//
VOID reset(THREADID tid) {
    // TODO: implement reset stats in the middle
}

void ThreadStart_hook(THREADID tid, local_stat_t* tdata) {
  // FIXME: the controller starts all threads if no trigger conditions
  // are specified, but currently it only starts TID0. Starting here
  // is wrong if the controller has a nontrivial start condition, but
  // this is what most people want. They can always stop the controller
  // and using markers as a workaround
  if(tid) {
    activate(tid);
  }
}


void ThreadFini_hook(THREADID tid, local_stat_t* tdata) {

}

//
// callbacks of control handler
//

LOCALFUN VOID ControlHandler(CONTROL_EVENT ev, VOID* val, CONTEXT *ctxt, VOID* ip, THREADID tid) {
  switch(ev) {
    case CONTROL_START: // start
      activate(tid);
      break;
    case CONTROL_STOP:  // stop
      deactivate(tid);
      break;
    default:
      ASSERTX(false);
  }
}

/* ============================================================== */
/* instrumentation logic */
/* ============================================================== */

VOID Instruction(INS ins, VOID *v) {
    // the instrumentation is called iff the instruction will actually be executed.
    // The IA-64 architecture has explicitly predicated instructions.
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP
    // prefixed instructions appear as predicated instructions in Pin
    //
    
    UINT32 memOperands = INS_MemoryOperandCount(ins);
    
    for (UINT32 memOp = 0; memOp < memOperands; memOp++) {

      if (INS_MemoryOperandIsRead(ins, memOp)) {
         INS_InsertPredicatedCall(
             ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
             IARG_THREAD_ID,
             IARG_INST_PTR, IARG_MEMORYOP_EA, memOp,
             IARG_MEMORYREAD_SIZE,
             IARG_UINT32, READ_ACCESS,
             IARG_END);
      }
      if (INS_MemoryOperandIsWritten(ins, memOp)) {
         INS_InsertPredicatedCall(
             ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
             IARG_THREAD_ID,
             IARG_INST_PTR, IARG_MEMORYOP_EA, memOp,
             IARG_MEMORYWRITE_SIZE,
             IARG_UINT32, WRITE_ACCESS,
             IARG_END);
      }
    }
}

VOID Trace(TRACE trace, VOID* v) {

   // head of trace
   ADDRINT pc = TRACE_Address(trace);

   for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
     
      // head of basic block
      const INS head = BBL_InsHead(bbl);

      if (!INS_Valid(head)) continue;
      
      for (INS ins = head; INS_Valid(ins); ins = INS_Next(ins)) {

          unsigned int instruction_size = INS_Size(ins);
          Instruction(ins, v);

          pc += instruction_size;
      }
   }
}
          
/* ===================================================================== */

void CollectLastAccesses() {
  int i;
  for(i=0;i<MAP_SIZE+1;i++) {
    map<ADDRINT, Stamp>::iterator iter;
    for(iter = stamps[i].set.begin(); iter!=stamps[i].set.end(); iter++) {
      Stamp s = iter->second;
      if(s.is_shared)
      {
        M += s.count;
      } 
    }
  }
}

VOID Fini(INT32 code, VOID* v){
  
  CollectLastAccesses();

  ThreadEnd();

  std::cout << " N : " << N << " M : " << M << std::endl;
  
  delete[] stamps;
}

/* ===================================================================== */

int main(int argc, char *argv[])
{
    PIN_InitSymbols();
    PIN_Init(argc,argv);

    // check for knobs if region instrumentation is involved
    control.RegisterHandler(ControlHandler, 0, FALSE);
    control.Activate();
    TRACE_AddInstrumentFunction(Trace, 0);

    stamps = new Map_Set[MAP_SIZE+1];
    for(stamp_tt i=0; i<(MAP_SIZE+1); i++)
      lock_release(&stamps[i].lock);

    // instrument
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
    PIN_AddFiniFunction(Fini, 0);

    ThreadInit();

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */

