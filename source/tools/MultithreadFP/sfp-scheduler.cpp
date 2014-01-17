#include <iostream>
#include <stdlib.h>

#include "pin.H"
#include "portability.H"

#include "common.H"
#include "rdtsc.H"
#include "atomic.H"
#include "sfp_list.H"
#include "sfp_stamp_table.H"
#include "sfp_locality_desc.H"
#include "thread_support_scheduler.H"

using namespace std;

struct TSFPListEntry {
  uint32_t time;
  short group;
};

class TSFPList : public TList<TSFPListEntry>
{
public:
  
  void update(local_stat_t* ldata, THREADID tid, uint32_t now, UINT32 type)
  {
    /* TODO profiling */

    TSFPList::Iterator curr;
    for(int i=0; i<LOCALITY_DESC_MAX_INDEX; i++)
    {
      TBitset bitset = 0;
      uint32_t len = TLocalityDesc::profile_index_to_length(i);
      uint32_t high = now - len;
      uint32_t low = 0;
      curr = begin();
      if ( !is_end(curr) && get(curr).time > len )
      {
        low = get(curr).time - len;
      }
      uint32_t rpoint = high;
      for(curr=begin(); !is_end(curr); curr = next(curr))
      {

        uint32_t c = get(curr).time;
        if ( c > high )
        {
          bitset |= (1<<tid);
          continue;
        }
        if ( c <= low ) break;
        ldata->ld.add(bitset, i, rpoint-c);
     
        rpoint = c;
        bitset |= (1<<tid);

      }

      ldata->ld.add(bitset, i, rpoint-low);
    }
  
    TSFPListEntry e;
    e.time = now;
    set_at(tid, e);
    
    TSFPList::Iterator prev = -1;
    for(curr = begin(); !is_end(curr); prev = curr, curr = next(curr))
    {
      if ( curr == (TSFPList::Iterator)tid) break;
    }
    
    if ( !is_end(curr) ) {

      if ( !is_end(prev) )
      {
        erase(prev, curr);
      }
    }
    set_front(tid);
  
  }

};

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

/* stamp manager */
TStampTblManager<TSFPList> gStampTblMgr;

/* global locality description */
TLocalityDesc gLocalityDesc;

/* ===================================================================== */
/* Routines */
/* ===================================================================== */
VOID InstructionExec(THREADID tid, VOID* ip, VOID* addr, UINT32 size, ADDRINT sp, UINT32 type)
{
  /* no profiling on stack variables */
  if ( (ADDRINT)addr > sp )
  {
    return;
  }

  if ( DONT_SAMPLE((uint32_t)SFP_RDTSC()) ) return;

  local_stat_t* tdata = get_tls(tid);
  if (tdata->is_instrument_enabled())
  {

      /* the base address aligned at cache line boundary */
      ADDRINT base_addr = gStampTblMgr.get_base_addr((ADDRINT)addr);
      
      /* the index of set in stamp table */
      ADDRINT set_idx = gStampTblMgr.get_index(base_addr);

      /* reserve the lock for entry in stamp table before recording time stamp */
      gStampTblMgr.Lock(set_idx);

      /* update time stamp info for the entry  */
      TSFPList& s = gStampTblMgr.get_stamp_list(set_idx, base_addr);

      /* update record */
      s.update(tdata, tid, (uint32_t)(SFP_RDTSC()>>10), type);

      /* release lock */
      gStampTblMgr.Unlock(set_idx);
    
  }
} 

/* =================================================
 * Routines for instrumentation controlling
 * ================================================= */
//
// hook at thread launch
//
inline void ThreadStart_hook(THREADID tid, local_stat_t* tdata) {
}

inline void ThreadFini_hook(THREADID tid, local_stat_t* tdata) {
}

/* ==================================================
 * Rountines for instrumentation
 * ================================================== */

LOCALFUN VOID Instruction(INS ins, void* v) {

  /**
   *  Instruments memory accesses using a predicated call, i.e.
   *  the instrumentation is called iff the instruction will actually be executed.
   *
   *  On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
   *  prefixed instructions appear as predicated instructions in Pin.
   */
  UINT32 memOperands = INS_MemoryOperandCount(ins);

  /* Iterate over each memory operand of the instruction. */
  for (UINT32 memOp = 0; memOp < memOperands; memOp++)
  {
    if (INS_MemoryOperandIsRead(ins, memOp))
    {
      INS_InsertPredicatedCall(
          ins, IPOINT_BEFORE, (AFUNPTR)InstructionExec,
          IARG_THREAD_ID,
          IARG_INST_PTR,
          IARG_MEMORYOP_EA, memOp,
          IARG_MEMORYREAD_SIZE,
          IARG_REG_VALUE, REG_STACK_PTR,
          IARG_UINT32, MEMOP_READ,
          IARG_END);
    }
   /**
    * Note that in some architectures a single memory operand can be 
    * both read and written (for instance incl (%eax) on IA-32)
    * In that case we instrument it once for read and once for write.
    */
    if (INS_MemoryOperandIsWritten(ins, memOp)) {
      INS_InsertPredicatedCall(
          ins, IPOINT_BEFORE, (AFUNPTR)InstructionExec,
          IARG_THREAD_ID,
          IARG_INST_PTR, 
          IARG_MEMORYOP_EA, memOp,
          IARG_MEMORYWRITE_SIZE,
          IARG_REG_VALUE, REG_STACK_PTR,
          IARG_UINT32, MEMOP_WRITE,
          IARG_END);
    }
  }
}
  
//
// The replacing routine for SFP_TaskStart
//
void BeforeTaskStart(short group) {
  
  local_stat_t* tdata = get_tls(PIN_ThreadId());
  tdata->set_group(group);
  tdata->enable_instrument();

}

//
// The replacing routine for SFP_TaskEnd
//
void BeforeTaskEnd() {

  local_stat_t* tdata = get_tls(PIN_ThreadId());
  tdata->disable_instrument();

}

//
// Replacing SFP_TaskStart and SFP_TaskEnd to
// get the user provided task id
//
// These two routines must NOT be inlined
//
VOID ImageLoad( IMG img, VOID* v) {

  if ( IMG_IsMainExecutable(img) )
  {
    RTN start_rtn = RTN_FindByName( img, "SFP_TaskStart" );
    RTN end_rtn = RTN_FindByName( img, "SFP_TaskEnd" );

    if (RTN_Valid(start_rtn) && RTN_Valid(end_rtn))
    {
      RTN_Replace(start_rtn, AFUNPTR(BeforeTaskStart));
      RTN_Replace(end_rtn,   AFUNPTR(BeforeTaskEnd));
    }

  }
}

//
// Fini routine, called at application exit
//
VOID Fini(INT32 code, VOID* v) {
}

/* =====================================================
 * main routine, entry of pin tools
 * ===================================================== */

//
// helper to output error message
//
LOCALFUN INT32 Usage() {
    cerr << "This tool prints out the number of dynamic instructions executed to stderr.\n";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

//
// main
//
int main(int argc, char *argv[])
{
    PIN_InitSymbols();
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    /* check for knobs if region instrumentation is involved */
//    control.RegisterHandler(ControlHandler, 0, FALSE);
//    control.Activate();

    /* register callbacks */
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
    PIN_AddFiniFunction(Fini, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);
    INS_AddInstrumentFunction(Instruction,0);

    /* init thread hooks, implemented in thread_support.H */
    ThreadInit();

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */

