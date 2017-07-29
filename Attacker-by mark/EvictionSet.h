
#ifndef EVICTION_SET_H
#define EVICTION_SET_H

#include "CacheDefs.h"
#include <stdint.h>

#define BITS_IN_CHAR 8

typedef struct Cache_Line_Node {
	struct Cache_Line_Node* pNext;
	Cache_Line* pLine;
}Cache_Line_Node;

/**
 *
 */
int CreateEvictionSet(LinesBuffer* pBuffer, Cache_Mapping* pEvictionSets);
char** GetMemoryStatistics(Cache_Mapping* pEvictionSets, Cache_Statistics* pCacheStats, int fReturnSamples);
void WriteStatisticsToFile(Cache_Statistics* pCacheStats, char* pFileName, int Sorted);
char* PollForData(Cache_Set* pSet, uint64_t BytesToRead);
int RatePatternsInSet(Cache_Set* pSet, int SetIdx, char OnesPattern[], char ZerosPattern[], int PatternBitsNum, int SamplesNum);
char* PollForDataBits(Cache_Set* pSet, uint64_t BitsToRead);
char* ExtractKeyFromSet(Cache_Set* pSet, char OnesPattern[], char ZerosPattern[], int PatternBitsNum);

#endif
