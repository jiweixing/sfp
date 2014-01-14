#ifndef SFP_LOCALITY_DESC_H
#define SFP_LOCALITY_DESC_H

#include "portability.H"
#include "common.H"

#include <cstring>

class TLocalityDesc {


public:
  
  static inline void set_length(int idx, TStamp len)
  { TLocalityDesc::lengths[idx] = len; }

  inline void add(TBitset bits, const TStamp& len, const INT64& val)
  {
    int idx = profile_length_to_index(len);
    impl[bits*MAX_INDEX + idx] += val;
  }

  inline int get(TBitset bits, const TStamp& len)
  {
    int idx = profile_length_to_index(len);
    return impl[bits*MAX_INDEX + idx];
  }

  TLocalityDesc& operator=(TLocalityDesc other)
  {
    memcpy(impl, other.impl, BITSET_CAP*MAX_INDEX*sizeof(INT64));
    return *this;
  }

  TLocalityDesc& diff(const TLocalityDesc& other)
  {
    for(int i=0; i<BITSET_CAP*MAX_INDEX; i++)
    {
      impl[i] = other[i] - impl[i];
    }
    return *this;
  }
    
private:

  inline const int     profile_length_to_index( const TStamp& len )
  {
    /* TODO add logic to search proper index for a given length */
    if ( len > TLocalityDesc::lengths[MAX_INDEX-1] ) return MAX_INDEX-1;
    if ( len < TLocalityDesc::lengths[0] ) return 0;
    
    return (len-TLocalityDesc::lengths[0]) * (MAX_INDEX-1) / (TLocalityDesc::lengths[MAX_INDEX-1]-TLocalityDesc::lengths[0]); 
  }

  inline const TStamp  profile_index_to_length( const int&    idx )
  { return TLocalityDesc::lengths[idx]; }

private:

  INT64 impl[BITSET_CAP * MAX_INDEX];
  static TStamp lengths[MAX_INDEX];

};

#endif
