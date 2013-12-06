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

#ifndef _MULTI_THREAD_FP_H_
#define _MULTI_THREAD_FP_H_

#include "pin.H"

/* ======================================= */
/* Global thread variables */
/* ======================================= */

// thread-local-storage key
static TLS_KEY tls_key;

PIN_LOCK thd_num_lock;

#define MAX_THD_NUM 256

// number of threads
unsigned thd_num = 0;

/* ======================================= */
/* Data structure */
/* ======================================= */

class local_stat_t {

  /* Add thread local information and updating method here */
  public:
/*
    UINT64 count[MAX_WINDOW];
    UINT64 count_i[MAX_WINDOW];

    UINT64 noshare_intervals[MAX_WINDOW];
    UINT64 noshare_intervals_i[MAX_WINDOW];

    UINT64 alter_intervals[MAX_WINDOW];
    UINT64 alter_intervals_i[MAX_WINDOW];

    UINT64 first_count[MAX_WINDOW];
    UINT64 first_count_i[MAX_WINDOW];
*/
    bool enabled;    

  public: 
    local_stat_t() {
/*
      memset(first_count, 0, MAX_WINDOW*sizeof(UINT64));
      memset(first_count_i, 0, MAX_WINDOW*sizeof(UINT64));
      memset(count, 0, MAX_WINDOW*sizeof(UINT64));
      memset(count_i, 0, MAX_WINDOW*sizeof(UINT64));
      memset(noshare_intervals, 0, MAX_WINDOW*sizeof(UINT64));
      memset(noshare_intervals_i, 0, MAX_WINDOW*sizeof(UINT64));
      memset(alter_intervals, 0, MAX_WINDOW*sizeof(UINT64));
      memset(alter_intervals_i, 0, MAX_WINDOW*sizeof(UINT64));
*/
      enabled = false;
    }
};

/* ======================================= */
/* Functions */
/* ======================================= */

void ThreadStart_hook(THREADID tid, local_stat_t* tdata);
void ThreadFini_hook(THREADID tid, local_stat_t* tdata);

inline local_stat_t* get_tls(THREADID tid) {
  return static_cast<local_stat_t*>(PIN_GetThreadData(tls_key, tid));
}

// hook at thread spawn
VOID ThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
 
//  GetLock(&thd_num_lock, tid+1);
//  thd_num++;
//  ReleaseLock(&thd_num_lock);
 
  local_stat_t* tdata = new local_stat_t;
  PIN_SetThreadData(tls_key, tdata, tid);

  ThreadStart_hook(tid, tdata);

}

// hook at thread end
VOID ThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 flags, VOID* v) {

//  GetLock(&thd_num_lock, tid+1);
//  thd_num--;
//  ReleaseLock(&thd_num_lock);

  local_stat_t* tdata = get_tls(tid);
  ThreadFini_hook(tid, tdata);  

  //delete tdata;
}

VOID ThreadInit() {
  
  // Initialize tls storeage
  tls_key = PIN_CreateThreadDataKey(0);
  //InitLock(&thd_num_lock);

}

// As a good programmer, I am proud of freeing allocated memory
VOID ThreadEnd() {
  for(unsigned int i=0;i<thd_num;i++) {
     local_stat_t* tdata = get_tls(i);
     delete tdata;
  }
}

#endif
