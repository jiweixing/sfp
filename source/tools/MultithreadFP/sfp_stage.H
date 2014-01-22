#ifndef SFP_STAGEMGR_H
#define SFP_STAGEMGR_H

#include <set>    // std::stack
#include <vector>   // std::vector
#include <utility>  // std::pair
#include <map>      // std::map
#include "common.H"
#include "pin.H"

class TStageManager
{

private:

  /**
   * Copy Constructor
   * \param [in] other The object from which to copy construct
   */
  TStageManager (const TStageManager& other);

  /**
   * operator=
   * \param [in] other The object from which to copy construct
   */
  void operator= (const TStageManager& other);

public:
  /**
   * Constructor
   * This is the only means of constructing this object
   */
  TStageManager()
  {
    /* RWMutex initialization */
    PIN_RWMutexInit(&rwlock);
  }

  ~TStageManager()
  {
    /* RWMutex Fini */
    PIN_RWMutexFini(&rwlock);

  }

  inline void ReadLock() { PIN_RWMutexReadLock(&rwlock); }
  inline void WriteLock() { PIN_RWMutexWriteLock(&rwlock); }
  inline void Unlock() { PIN_RWMutexUnlock(&rwlock); }

  inline bool TryRegisterThread(short stage, THREADID tid) {
    if(stage_regs.find(stage) == stage_regs.end()) {
      stage_regs[stage] = tid;
      return true;
    }
    return false;
  }

  inline bool TryUnregisterThread(short stage, THREADID tid) {
    if(stage_regs.find(stage) != stage_regs.end()) {
      stage_regs.erase(stage);
      return true;
    }
    return false;
  }

  inline THREADID GetRegisteredThread(short stage) {
    if(stage_regs.find(stage) != stage_regs.end()) {
      return stage_regs[stage];
    }
    return 0xff;
  }

private:

  std::map<short, THREADID> stage_regs;
  PIN_RWMUTEX rwlock;

};

#endif
