#ifndef SFP_LOCALITY_DESC_H
#define SFP_LOCALITY_DESC_H

#include "portability.H"
#include "common.H"

#include <cstring>
#include <string.h> // ffsl

class TLocalityDesc {

public:
  
  static inline void set_length(int idx, uint32_t len)
  { TLocalityDesc::lengths[idx] = len; }

  inline void add(TBitset bits, const int& idx, const INT64& val)
  {
    impl[bits*LOCALITY_DESC_MAX_INDEX + idx] += val;
  }

  inline int get(TBitset bits, const int& idx)
  {
    return impl[bits*LOCALITY_DESC_MAX_INDEX + idx];
  }

  TLocalityDesc& operator=(TLocalityDesc other)
  {
    memcpy(impl, other.impl, BITSET_CAP*LOCALITY_DESC_MAX_INDEX*sizeof(uint32_t));
    return *this;
  }

  TLocalityDesc& diff(const TLocalityDesc& other)
  {
    for(int i=0; i<BITSET_CAP*LOCALITY_DESC_MAX_INDEX; i++)
    {
      impl[i] = other.impl[i] - impl[i];
    }
    return *this;
  }
    

  inline static const int     profile_length_to_index( const uint32_t& len )
  {
    /* TODO add logic to search proper index for a given length */
/*
    if ( len > TLocalityDesc::lengths[MAX_INDEX-1] ) return MAX_INDEX-1;

    if ( len < TLocalityDesc::lengths[0] ) return 0;
    
    return (len-TLocalityDesc::lengths[0]) * (MAX_INDEX-1) / (TLocalityDesc::lengths[MAX_INDEX-1]-TLocalityDesc::lengths[0]); 
*/
    if ( len>(1<<(4*LOCALITY_DESC_MAX_INDEX)) ) return (1<<(4*LOCALITY_DESC_MAX_INDEX));
    return ffsl(len) / 4;
  }

  inline static const uint32_t  profile_index_to_length( const int&    idx )
//  { return TLocalityDesc::lengths[idx]; }
  { return 1<<(idx<<2); }

private:

  uint32_t impl[BITSET_CAP * LOCALITY_DESC_MAX_INDEX];
  static uint32_t lengths[LOCALITY_DESC_MAX_INDEX];

};

#endif
