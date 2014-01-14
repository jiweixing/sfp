#include <iostream>
#include <stdlib.h>

#include "pin.H"
#include "portability.H"

#include "common.H"
#include "rdtsc.H"
#include "atomic.H"
#include "sfp_list.H"
#include "sfp_tokens.H"
#include "sfp_stamp_table.H"
#include "sfp_locality_desc.H"
#include "thread_support_scheduler.H"

using namespace std;

class TSFPList : public TList<UINT64>
{
public:
  
  void update(TToken token, TStamp now, TTaskDesc* td)
  {
    TLocalityDesc& ld = td->ldesc;
    TStamp start = td->start_time;
    /* TODO profiling */
  }

};

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

/* token manager */
TTokenManager gTokenMgr;

/* stamp manager */
TStampTblManager<TSFPList> gStampTblMgr;

/* TODO global locality description */
//TLocalityDesc gLocalityDesc;

/* ===================================================================== */
/* Routines */
/* ===================================================================== */
VOID InstructionExec(THREADID tid, VOID* ip, VOID* addr, UINT32 size, ADDRINT sp)
{
  /* no profiling on stack variables */
  if ( (ADDRINT)addr > sp )
  {
    return;
  }

  local_stat_t* tdata = get_tls(tid);
  if (tdata->is_taskid_inspect_enabled())
  {
    unsigned int taskid = tdata->current_taskid();
     
    gTokenMgr.ReadLock();
    if ( gTokenMgr.is_task_running(taskid) ) {

      TTaskDesc* td = gTokenMgr.get_task_descriptor(taskid);

      /* release token manager lock after obtaining the task descriptor */
      gTokenMgr.Unlock();

      TToken current_token = td->token;

      /* the base address aligned at cache line boundary */
      ADDRINT base_addr = gStampTblMgr.get_base_addr((ADDRINT)addr);
      
      /* the index of set in stamp table */
      ADDRINT set_idx = gStampTblMgr.get_index(base_addr);

      /* reserve the lock for entry in stamp table before recording time stamp */
      gStampTblMgr.Lock(set_idx);

      /* update time stamp info for the entry  */
      TSFPList& s = gStampTblMgr.get_stamp_list(set_idx, base_addr);

      /* update record */
      s.update(current_token, SFP_RDTSC(), td);

      /* release lock */
      gStampTblMgr.Unlock(set_idx);
      
      return;
    }

    /* if current task is not running (in TaskStart and TaskEnd region) right now */
    gTokenMgr.Unlock();
    
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
          IARG_END);
    }
  }
}
  
//
// The replacing routine for SFP_TaskStart
//
void BeforeTaskStart(unsigned int own_id, unsigned int parent_id) {
  TTaskDesc* own_td;
  TToken parent_token;

  gTokenMgr.WriteLock();
  own_td = gTokenMgr.get_task_descriptor(own_id);
  parent_token = own_td->parent;
  gTokenMgr.get_token(own_td, parent_token);
  gTokenMgr.Unlock();

  own_td->parent = parent_id;
  own_td->enable_instrument();
  own_td->start_time = SFP_RDTSC();

}

//
// The replacing routine for SFP_TaskEnd
//
void BeforeTaskEnd(unsigned int tid) {

  gTokenMgr.WriteLock();
  TTaskDesc* td = gTokenMgr.get_task_descriptor(tid);
  TToken token = td->token; 
  gTokenMgr.release_token(token, tid);
  gTokenMgr.Unlock();
 
  td->end_time = SFP_RDTSC();
  td->times.push_back(std::make_pair(td->start_time, td->end_time));
  td->disable_instrument();

}

void StoreTaskIDAddr(const void* taskid_addr)
{
  local_stat_t* ldata = get_tls(PIN_ThreadId());
  ldata->set_taskid_ptr(static_cast<const unsigned int*>(taskid_addr));
  ldata->enable_taskid_inspect();
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
    RTN taskid_rtn = RTN_FindByName( img, "SFP_GetTaskIDAddr" );

    if (RTN_Valid(start_rtn) && RTN_Valid(end_rtn))
    {
      RTN_Replace(start_rtn, AFUNPTR(BeforeTaskStart));
      RTN_Replace(end_rtn,   AFUNPTR(BeforeTaskEnd));
      RTN_Replace(taskid_rtn,AFUNPTR(StoreTaskIDAddr));
    }

  }
}

//
// Fini routine, called at application exit
//
VOID Fini(INT32 code, VOID* v) {
  gTokenMgr.dump_taskdesc(std::cout);
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

