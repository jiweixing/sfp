/* This file provide thread support to sample fp tool
 * It includes the thread local data def and some global
 * state information.
 *
 * To implement thread accounting, just add method and state in 
 * local_stat_t class and call them in sample_fp.cpp RecordMem file
 *
 * Written by Hao Luo
 *
 */

#ifndef _THREAD_SUPPORT_SCHEDULER_H_
#define _THREAD_SUPPORT_SCHEDULER_H_

#include <vector>
#include "pin.H"

using namespace std;

/* ======================================= */
/* Global thread variables */
/* ======================================= */

// thread-local-storage key
static TLS_KEY tls_key;

// lock used to protect gThreadNum
static PIN_LOCK thd_num_lock;

// thread count
unsigned gThreadNum = 0;

/* ======================================= */
/* Data structure */
/* ======================================= */

/* thread local data */
struct local_stat_t {

private:

  /* instrumentation start flag */
  bool instrument_enabled;

  /* task ID inspection start flag */
  bool taskid_inspect_enabled;
  
  /* The address where current task id is stored */
  const unsigned int* taskid_ptr;

public:

  /* Constructor */
  local_stat_t() : instrument_enabled(false),
                   taskid_inspect_enabled(false)
  {}

  inline void enable_instrument() { instrument_enabled = true; }
  inline void disable_instrument() { instrument_enabled = false; }
  inline void enable_taskid_inspect() { taskid_inspect_enabled = true; }
  inline void disable_taskid_inspect() { taskid_inspect_enabled = false; }

  inline void set_taskid_ptr(const unsigned int* taskid_addr) 
  { taskid_ptr = taskid_addr; }

  inline bool is_taskid_inspect_enabled() const 
  { return taskid_inspect_enabled; }
  
  inline unsigned int current_taskid() const { return *taskid_ptr; }

};

/* ======================================= */
/* Functions */
/* ======================================= */

inline void ThreadStart_hook(THREADID tid, local_stat_t* tdata);
inline void ThreadFini_hook(THREADID tid, local_stat_t* tdata);

/* interface to get thread local data */
inline local_stat_t* get_tls(THREADID tid) {
  return static_cast<local_stat_t*>(PIN_GetThreadData(tls_key, tid));
}

/* hook at thread spawn */
VOID ThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
 
  PIN_GetLock(&thd_num_lock, tid+1);
  gThreadNum++;
  PIN_ReleaseLock(&thd_num_lock);
 
  local_stat_t* tdata = new local_stat_t;
  PIN_SetThreadData(tls_key, tdata, tid);

  ThreadStart_hook(tid, tdata);

}

/* hook at thread end */
VOID ThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 flags, VOID* v) {

  local_stat_t* tdata = get_tls(tid);
  ThreadFini_hook(tid, tdata);  

}

/* initializing thread local data, must be called in main */
VOID ThreadInit() {
  
  // Initialize tls storeage
  tls_key = PIN_CreateThreadDataKey(0);
  PIN_InitLock(&thd_num_lock);

}

/* deallocation of thread local data */
VOID ThreadEnd() {
  for(unsigned int i=0;i<gThreadNum;i++) {
     local_stat_t* tdata = get_tls(i);
     delete tdata;
  }
}

#endif
