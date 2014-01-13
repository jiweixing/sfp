#ifndef SFP_STAMP_TBL_MGR_H
#define SFP_STAMP_TBL_MGR_H

#include <map>
#include "atomic.H"
#include "common.H"
#include "pin.H"

template<typename T>
class TStampTblManager
{

private:

  /**
   * Copy Constructor
   * \param [in] other The object from which to copy construct
   */
  TStampTblManager (const TStampTblManager& other);

  /**
   * operator=
   * \param [in] other The object from which to copy construct
   */
  void operator= (const TStampTblManager& other);

public:
  /**
   * Constructor
   * This is the only means of constructing this object
   */
  TStampTblManager()
  {
    impl = new TEntry[TStampTblManager::nTotalSets];
  }

  ~TStampTblManager()
  {
    delete[] impl;
  }

  /* time stamp table entry, each entry is a set of time stamps */ 
  typedef struct {
    sfp_lock_t lock;
    map<ADDRINT, T> set;
  } TEntry;

  static const int nTotalSets;
  static const int SetWidth;
  static const int WordWidth;

  static inline ADDRINT get_index(const ADDRINT& x) 
  { return (x>>TStampTblManager::SetShift) & TStampTblManager::nTotalSets; }

  static inline ADDRINT get_base_addr(const ADDRINT& x)
  { return x & ~(TStampTblManager::WordWidth-1); }

  inline void Lock(ADDRINT set_idx) { lock_acquire(&impl[set_idx].lock); }
  inline void Unlock(ADDRINT set_idx) { lock_release(&impl[set_idx].lock); }

  T& get_stamp_list(const ADDRINT& set_idx, const ADDRINT& base_addr);

private:

  static const int SetShift;
  static const int WordShift;

  TEntry* impl;

};

template<typename T>
const int TStampTblManager<T>::nTotalSets = 0x7fffff; // if long is 64bit, size should be larger 

template<typename T>
const int TStampTblManager<T>::SetShift = 6;
template<typename T>
const int TStampTblManager<T>::SetWidth = 64;

template<typename T>
const int TStampTblManager<T>::WordShift = 6;
template<typename T>
const int TStampTblManager<T>::WordWidth = 64;


#endif
