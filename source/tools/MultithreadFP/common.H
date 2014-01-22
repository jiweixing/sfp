#ifndef _SFP_COMMON_H_
#define _SFP_COMMON_H_

#include "portability.H"

/* time stamp type */
typedef UINT64 TStamp;

/* simple bitmap type */
typedef UINT32 TBitset;

/* type of token */
typedef unsigned short TToken;

#define MEMOP_WRITE 1
#define MEMOP_READ  2

#define MAX_TOKENS 8

#define BITSET_CAP (1<<MAX_TOKENS)

#define LOCALITY_DESC_MAX_INDEX 5

#define MAX_LIST_ENTRIES 16

/*
#define SAMPLE_FREQ 0x100000             // sampling period 2^20 cycles
#define SAMPLE_LEN  (SAMPLE_FREQ >> 5)  // sampling rate, 1/(2^5) = 1/32
#define DONT_SAMPLE(cycles) (((cycles)&(SAMPLE_FREQ-1)) > ((cycles)&(SAMPLE_LEN-1)))
*/
#endif
