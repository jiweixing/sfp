#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <map>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <functional>

#include "pin.H"
#include "common.H"
#include "portability.H"
#include "histo.H"
#include "rdtsc.H"
#include "atomic.H"
#include "instlib.H"
#include "sfp_list.H"
#include "sfp_tokens.H"
#include "thread_support_scheduler.H"

using namespace std;
using namespace histo;
using namespace INSTLIB;

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

/* control variable */
LOCALVAR CONTROL control;

/* token manager */
TTokenManager gTokenMgr;

/* ===================================================================== */
/* Routines */
/* ===================================================================== */


/* =================================================
 * Routines for instrumentation controlling
 * ================================================= */

//
// activate instrumentation and recording
//
inline LOCALFUN VOID activate(THREADID tid) {
    local_stat_t* data = get_tls(tid);
    data->enabled = true;
}

//
// deactivate instrumentation and recording
//
inline LOCALFUN VOID deactivate(THREADID tid) {
    local_stat_t* data = get_tls(tid);
    data->enabled = false;
}

//
// hook at thread launch
//
inline void ThreadStart_hook(THREADID tid, local_stat_t* tdata) {
  // FIXME: the controller starts all threads if no trigger conditions
  // are specified, but currently it only starts TID0. Starting here
  // is wrong if the controller has a nontrivial start condition, but
  // this is what most people want. They can always stop the controller
  // and using markers as a workaround
  if(tid) {
    activate(tid);
  }
}

inline void ThreadFini_hook(THREADID tid, local_stat_t* tdata) {
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

/* ==================================================
 * Rountines for instrumentation
 * ================================================== */

//
// The replacing routine for SFP_TaskStart
//
void BeforeTaskStart(unsigned int own_id, unsigned int parent_id) {
  TTaskDesc* own_td;
  TToken parent_token;

  gTokenMgr.Lock();
  own_td = gTokenMgr.get_task_descriptor(own_id);
  parent_token = gTokenMgr.taskid_to_token(parent_id);
  gTokenMgr.get_token(own_td, parent_token);
  gTokenMgr.Unlock();

  own_td->start_time = SFP_RDTSC();
  own_td->parent = parent_id;

}

//
// The replacing routine for SFP_TaskEnd
//
void BeforeTaskEnd(unsigned int tid) {

  gTokenMgr.Lock();
  TToken token = gTokenMgr.taskid_to_token(tid);
  TTaskDesc* td = gTokenMgr.get_task_descriptor(tid);
  gTokenMgr.release_token(token);
  gTokenMgr.Unlock();
 
  td->end_time = SFP_RDTSC();

}

void GetTaskID(const void* taskid_addr)
{
  std::cout << *(unsigned int*)taskid_addr << std::endl;
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
    RTN taskid_rtn = RTN_FindByName( img, "SFP_GetTaskID" );

    if (RTN_Valid(start_rtn) && RTN_Valid(end_rtn))
    {
      RTN_Replace(start_rtn, AFUNPTR(BeforeTaskStart));
      RTN_Replace(end_rtn,   AFUNPTR(BeforeTaskEnd));
      RTN_Replace(taskid_rtn,AFUNPTR(GetTaskID));
    }

  }
}

//
// Fini routine, called at application exit
//
VOID Fini(INT32 code, VOID* v){
  //gTokenMgr.Lock();
  //gTokenMgr.dump_taskdesc();
  //gTokenMgr.Unlock();
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
    control.RegisterHandler(ControlHandler, 0, FALSE);
    control.Activate();

    /* register callbacks */
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
    PIN_AddFiniFunction(Fini, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);

    /* init thread hooks, implemented in thread_support.H */
    ThreadInit();

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */

