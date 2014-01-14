#ifndef _SFP_COMMON_H_
#define _SFP_COMMON_H_

#include "portability.H"

/* time stamp type */
typedef UINT64 TStamp;

/* simple bitmap type */
typedef UINT32 TBitset;

#define MAX_TOKENS 8

#define BITSET_CAP (1<<MAX_TOKENS)

#define LOCALITY_DESC_MAX_INDEX 16

#define MAX_LIST_ENTRIES MAX_TOKENS

#endif
