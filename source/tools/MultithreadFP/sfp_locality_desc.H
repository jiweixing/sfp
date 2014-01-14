#ifndef SFP_LOCALITY_DESC_H
#define SFP_LOCALITY_DESC_H

#include <map>

#include "portability.H"
#include "common.H"

class TLocalityDesc {

private:

  typedef std::vector<int> profile_type;

public:
  
  TLocalityDesc() : nIndices(16)
  {
    lengths = new TStamp[nIndices];
  }

  TLocalityDesc(const int& indices)
  {
    nIndices = indices;
    lengths = new TStamp[nIndices];
  }

  TLocalityDesc(const std::vector<TStamp>& other)
  {
    nIndices = other.size();
    lengths = new TStamp[nIndices];
    for(int i=0;i<nIndices;i++)
    { lengths[i] = other[i]; }
  }

  ~TLocalityDesc()
  {
    delete[] lengths;
  }

  inline void set_length(int idx, TStamp len)
  { lengths[idx] = len; }

  inline void add(TBitset bits, const TStamp& len, const int& val)
  {
    if (impl.find(bits) == impl.end())
    {
      impl[bits] = profile_type(nIndices, 0);
    }
    int idx = profile_length_to_index(len);
    impl[bits][idx] += val;
  }

  inline int get(TBitset bits, const TStamp& len)
  {
    if (impl.find(bits) == impl.end())
    {
      return 0;
    }
    int idx = profile_length_to_index(len);
    return impl[bits][idx];
  }
    
private:

  inline const int     profile_length_to_index( const TStamp& len )
  {
    /* TODO add logic to search proper index for a given length */
    if ( len > lengths[nIndices-1] ) return nIndices-1;
    if ( len < lengths[0] ) return 0;
    
    return (len-lengths[0]) * (nIndices-1) / (lengths[nIndices-1]-lengths[0]); 
  }

  inline const TStamp  profile_index_to_length( const int&    idx )
  { return lengths[idx]; }

private:

  std::map<TBitset, profile_type> impl;
  TStamp* lengths;
  int nIndices;

};

#endif
