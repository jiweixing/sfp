#ifndef SFP_LOCALITY_DESC_H
#define SFP_LOCALITY_DESC_H

#include "portability.H"
#include "common.H"

#include <cstring>
#include <string.h> // ffsll

class TLocalityDesc {

public:
  
  static inline void set_length(int idx, TStamp len)
  { TLocalityDesc::lengths[idx] = len; }

  inline void add(TBitset bits, const TStamp& len, const INT64& val)
  {
    int idx = profile_length_to_index(len);
    impl[bits*LOCALITY_DESC_MAX_INDEX + idx] += val;
  }

  inline int get(TBitset bits, const TStamp& len)
  {
    int idx = profile_length_to_index(len);
    return impl[bits*LOCALITY_DESC_MAX_INDEX + idx];
  }

  TLocalityDesc& operator=(TLocalityDesc other)
  {
    memcpy(impl, other.impl, BITSET_CAP*LOCALITY_DESC_MAX_INDEX*sizeof(INT64));
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
    
private:

  inline const int     profile_length_to_index( const TStamp& len )
  {
    /* TODO add logic to search proper index for a given length */
/*
    if ( len > TLocalityDesc::lengths[MAX_INDEX-1] ) return MAX_INDEX-1;
    if ( len < TLocalityDesc::lengths[0] ) return 0;
    
    return (len-TLocalityDesc::lengths[0]) * (MAX_INDEX-1) / (TLocalityDesc::lengths[MAX_INDEX-1]-TLocalityDesc::lengths[0]); 
*/
    if ( ffsll(len) > 40 ) return 15;
    if ( ffsll(len) < 24 ) return 24;
    return ffsll(len) - 24;
  }

  inline const TStamp  profile_index_to_length( const int&    idx )
//  { return TLocalityDesc::lengths[idx]; }
  { return (TStamp)1<<(24+idx); }

private:

  INT64 impl[BITSET_CAP * LOCALITY_DESC_MAX_INDEX];
  static TStamp lengths[LOCALITY_DESC_MAX_INDEX];

};

#endif
