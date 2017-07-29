#include "CacheFuncs.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

__always_inline int ProbeAsm(char* pSet, int SetSize, char* pLine)
{
	int Time;
	int j;
	char* pCurrLine = pSet;

	if (SetSize == 0)
		return 0;

	// Add the line to check to the end of the linked list so we go over it last.
	for (j = 0; j < SetSize - 1; j++)
	{
		pCurrLine = *(char**)pCurrLine;
	}

	*(char**)pCurrLine = pLine;
	*(char**)pLine = 0;

	// First of all, touch the record.
	Time = CACHE_POLL_MAX_HIT_THRESHOLD_IN_TICKS + 1;

	// While the results aren't obvious, keep probing this line.
	while ((Time > CACHE_POLL_MAX_HIT_THRESHOLD_IN_TICKS &&
			Time < CACHE_POLL_MIN_MISS_THRESHOLD_IN_TICKS))
	{
		// Now go over all the records in the set for the first time.
		// We do this 2 times to be very very sure all lines have been loaded to the cache.

		// First load all data into registers to avoid unneccesarily touching the stack.
		asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
		asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
		asm volatile ("mov %0, %%r9;" : : "r" (pLine) : "r9");

		asm volatile ("mov (%r9), %r10;");		// Touch the line we want to test, loading it to the cache.
		asm volatile ("lfence;");
		asm volatile ("2:");
		asm volatile ("mov (%r8), %r8;");		// Move to the next line in the linked list.
		asm volatile ("loop	2b;");

		// This is a copy of the previous lines.
		// We go over all the records for the second time.
		asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
		asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
	//	asm volatile ("mov %0, %%r9;" : : "r" (pLine) : "r9");
	//	asm volatile ("mov (%r9), %r10;");
		asm volatile ("lfence;");
		asm volatile ("1:");
		asm volatile ("mov (%r8), %r8;");
		asm volatile ("loop	1b;");

		// Now check if the original line is still in the cache
		asm volatile ("lfence;");
		asm volatile ("rdtsc;");				// Get the current time in cpu clocks
		asm volatile ("movl %eax, %edi;");		// Save it aside in edi
		asm volatile ("lfence;");
		asm volatile ("mov (%r8), %r8;");		// Touch the last line in the linked list, which is the new line.
		asm volatile ("lfence;");
		asm volatile ("rdtsc;");				// Get the time again.
		asm volatile ("subl %edi, %eax;");		// Calculate how much time has passed.
		asm volatile ("movl %%eax, %0;" : "=r" (Time));  // Save that time to the Time variable
	}

	*(char**)pCurrLine = 0;

	return Time >= CACHE_POLL_MIN_MISS_THRESHOLD_IN_TICKS;
}

int PollAsm(char* pSet, int SetSize)
{
	long int Miss = 0;
	int i=0;
	int PollTimeLimit = CACHE_POLL_MIN_SET_MISS_THRESHOLD_IN_TICKS;

	if (SetSize == 0)
		return 0;
	
	// Read the variables to registers.
	asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
	asm volatile ("movl %0, %%ebx;" : : "r" (PollTimeLimit) : "ebx");
	asm volatile ("mov %0, %%r10;" : : "r" (Miss) : "r10");
	asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
	
	// This loop goes over all lines in the set and checks if they are still in the cache.
	asm volatile ("CHECK_LINE:");

	asm volatile ("rdtsc;");			// Get the current time
	asm volatile ("movl %eax, %edi;");		// Save it in edi
	asm volatile ("mov (%r8), %r8;");		// Read the next line into the cache
	asm volatile ("lfence;");
	asm volatile ("rdtsc;");			// Get time again
	asm volatile ("subl %edi, %eax;");		// Check how much time has passed.
	asm volatile ("cmp %eax, %ebx;");		// Is it too much?
	asm volatile ("jnl HIT");			// If not, the line is still in the cache (Cache Hit)
	asm volatile ("MISS:");				// If yes, then this is a cache miss.
	asm volatile ("inc %r10");			// Count how many cache misses we've had so far.
	
	asm volatile ("HIT:");
	asm volatile ("loop	CHECK_LINE;");
	asm volatile ("mov %%r10, %0;" : "=r" (Miss));	// Save to variable how many misses have we seen.


/*
	asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
	asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
	asm volatile ("SECOND_TRAVEL:");
	asm volatile ("mov (%r8), %r8;");		// Read the next line into the cache
	asm volatile ("loop	SECOND_TRAVEL;");
*/
	
	return Miss;
}


int DoublePollAsm(char* probe, char* prime, int SetSize)
{
	long int Miss = 0;
	int i=0;
	int PollTimeLimit = CACHE_POLL_MIN_SET_MISS_THRESHOLD_IN_TICKS;

	if (SetSize == 0)
		return 0;
	
	// Read the variables to registers.
	asm volatile ("mov %0, %%r8;" : : "r" (probe) : "r8");
	asm volatile ("movl %0, %%ebx;" : : "r" (PollTimeLimit) : "ebx");
	asm volatile ("mov %0, %%r10;" : : "r" (Miss) : "r10");
	asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
	
	// This loop goes over all lines in the set and checks if they are still in the cache.
	asm volatile ("DoublePollAsm_CHECK_LINE:");

	asm volatile ("rdtsc;");			// Get the current time
	asm volatile ("movl %eax, %edi;");		// Save it in edi
	asm volatile ("mov (%r8), %r8;");		// Read the next line into the cache
	asm volatile ("lfence;");
	asm volatile ("rdtsc;");			// Get time again
	asm volatile ("subl %edi, %eax;");		// Check how much time has passed.
	asm volatile ("cmp %eax, %ebx;");		// Is it too much?
	asm volatile ("jnl DoublePollAsm_HIT");		// If not, the line is still in the cache (Cache Hit)
	asm volatile ("DoublePollAsm_MISS:");		// If yes, then this is a cache miss.
	asm volatile ("inc %r10");			// Count how many cache misses we've had so far.
	
	asm volatile ("DoublePollAsm_HIT:");
	asm volatile ("loop	DoublePollAsm_CHECK_LINE;");
	asm volatile ("mov %%r10, %0;" : "=r" (Miss));	// Save to variable how many misses have we seen.

	asm volatile ("mov %0, %%r8;" : : "r" (prime) : "r8");
	asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
	asm volatile ("SECOND_TRAVEL:");
	asm volatile ("mov (%r8), %r8;");		// Read the next line into the cache
	asm volatile ("loop	SECOND_TRAVEL;");

	
	return Miss;
}

int EvictionLine(char *line){
	long int Miss = 0;
	asm volatile ("mov %0, %%r10;" : : "r" (Miss) : "r10");
	asm volatile ("mov %0, %%r8;" : : "r" (line) : "r8");
	asm volatile ("rdtsc;");			// Get the current time
	asm volatile ("movl %eax, %edi;");		// Save it in edi

	asm volatile ("mov (%r8), %r8;");		// Read the eviction line into the cache

	asm volatile ("subl %edi, %eax;");		// Check how much time has passed.
	asm volatile ("cmp %eax, %ebx;");		// Is it too much?
	asm volatile ("jnl EvictionLine_HIT");		// If not, the line is still in the cache (Cache Hit)
	asm volatile ("EvictionLine_MISS:");		// If yes, then this is a cache miss.
	asm volatile ("inc %r10");			// Count how many cache misses we've had so far.
	asm volatile ("mov %%r10, %0;" : "=r" (Miss));	// Save to variable how many misses have we seen.
	asm volatile ("EvictionLine_HIT:");
	return Miss;
}
int PollAsmDebug(char* pSet, int SetSize,FILE* debug)
{
	long int Miss = 0;
	int i=0;
	long int One=1;
	long int indexOfFirstHitMiss[12]={0}; 
	int PollTimeLimit = CACHE_POLL_MIN_SET_MISS_THRESHOLD_IN_TICKS;

	if (SetSize == 0)
		return 0;
	
	// Read the variables to registers.
	asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
	asm volatile ("mov %0, %%r12;" : : "r" (One) : "r12");
	asm volatile ("movl %0, %%ebx;" : : "r" (PollTimeLimit) : "ebx");
	asm volatile ("mov %0, %%r10;" : : "r" (Miss) : "r10");
	asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
	
	// This loop goes over all lines in the set and checks if they are still in the cache.
	asm volatile ("CHECK_LINE_DEBUG:");

	asm volatile ("rdtsc;");			// Get the current time
	asm volatile ("movl %eax, %edi;");		// Save it in edi
	asm volatile ("mov (%r8), %r8;");		// Read the next line into the cache
	asm volatile ("lfence;");
	asm volatile ("rdtsc;");			// Get time again
	asm volatile ("subl %edi, %eax;");		// Check how much time has passed.
	asm volatile ("cmp %eax, %ebx;");		// Is it too much?
	asm volatile ("jnl HIT_DEBUG");			// If not, the line is still in the cache (Cache Hit)
	asm volatile ("MISS_DEBUG:");				// If yes, then this is a cache miss.
	asm volatile ("mov %%r12, %0;" : "=r" (indexOfFirstHitMiss[i++]));

	asm volatile ("inc %r10");			// Count how many cache misses we've had so far.
	
	asm volatile ("HIT_DEBUG:");
	asm volatile ("loop	CHECK_LINE_DEBUG;");

	asm volatile ("mov %%r10, %0;" : "=r" (Miss));	// Save to variable how many misses have we seen.
	
	if(Miss>0){
		for(i=0;i<11;i++){
			fprintf(debug,"%ld, ",indexOfFirstHitMiss[i]);
		}
		fprintf(debug,"%ld\n ",indexOfFirstHitMiss[11]);
	}
	return Miss;
}

int ProbeManyTimes(char* pSet, int SetSize, char* pLine)
{
	int Misses = 4;
	int Result = 0;

	// We don't trust probe, because probe code could conflict with the line we are probing.
	// And there could be noise and prefetcher and whatnot.
	// So we probe many times, then check if we've had enough misses/hits to make a decision.
	// Each call to probe asm duplicates its code, promising they do not sit in the same area in the cache.
	// Thus reducing the chance all calls will conflict with the lines.
	// We also add variables to the stack in between calls, to make sure we also use different stack
	// memory in each call.

	// Current probe count is 15.
	// This code was written when the code was much less stable and many probes were needed.
	// We probably don't need these many probe operations anymore.
	// Still, these are cheap operations, so we can allow ourselves.
	//while (Misses > 2 && Misses < 5) {
		Misses = 0;
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad1[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad2[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad3[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad4[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad5[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad6[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad7[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad8[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad9[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad10[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad11[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad12[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad13[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad14[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		memset(Pad1, 0, sizeof(Pad1));
		memset(Pad2, 0, sizeof(Pad2));
		memset(Pad4, 0, sizeof(Pad4));
		memset(Pad3, 0, sizeof(Pad3));
		memset(Pad5, 0, sizeof(Pad4));
		memset(Pad6, 0, sizeof(Pad3));
		memset(Pad7, 0, sizeof(Pad4));
		memset(Pad8, 0, sizeof(Pad4));
		memset(Pad9, 0, sizeof(Pad4));
		memset(Pad10, 0, sizeof(Pad4));
		memset(Pad11, 0, sizeof(Pad4));
		memset(Pad12, 0, sizeof(Pad4));
		memset(Pad13, 0, sizeof(Pad4));
		memset(Pad14, 0, sizeof(Pad4));
	//}

	return Misses >= 10;
}

__always_inline int MeasureTimeAsm(char* pMem)
{
	int Time;
	asm volatile ("lfence;");
	asm volatile ("rdtsc;");
	asm volatile ("movl %eax, %edi;");
	asm volatile ("movl (%0), %%ebx;" : : "r" (pMem) : "ebx");
	asm volatile ("lfence;");
	asm volatile ("rdtsc;");
	asm volatile ("subl %edi, %eax;");
	asm volatile ("movl %%eax, %0;" : "=r" (Time));
	return Time;
}


int CreateEvictionSet(char* Memory, uint32_t Size, Cache_Mapping* pCacheSets)
{
	int i, j;
	Raw_Cache_Mapping RawMapping;
	int FilledSetIndex = 0;
	int SetsInCurrModulu;
	int CurrSetInSlice;
	int FindSetAttempts;


	printf("Creating Eviction Sets");
	memset(pCacheSets, 0, sizeof(Cache_Mapping));
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
				Miss = ProbeManyTimes(pCacheSets->Sets[CurrSetInSlice].Lines[0], pCacheSets->Sets[CurrSetInSlice].ValidLines, CurrAdress);

				// If the line conflicts with this set, give it up, we don't care about it no more
				if (Miss) break;
			}

			if (Miss) continue;

			// Now go over all the relevant sets and check
			RandomizeRawSet(&RawMapping.Sets[Set]);
			Miss = ProbeManyTimes(RawMapping.Sets[Set].Lines[0], RawMapping.Sets[Set].ValidLines, CurrAdress);

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
				// We try and find the set a few times... mostly because at some point in the past
				// The function wasn't stable enough.
				for (FindSetAttempts = 0; FindSetAttempts < 3; FindSetAttempts++)
				{
					int FoundNewSet = FindSet(&RawMapping.Sets[Set], &pCacheSets->Sets[FilledSetIndex], CurrAdress);
					SetsInCurrModulu += FoundNewSet;
					FilledSetIndex += FoundNewSet;

					if (FoundNewSet && (FilledSetIndex%100==0))
					{
						printf("Found set %d\r\n\033[1A", FilledSetIndex);
						RandomizeSet(&pCacheSets->Sets[FilledSetIndex]);
						break;
					}
				}
			}
		}
	}

	pCacheSets->ValidSets = FilledSetIndex;
	return FilledSetIndex;
}

int AdressToSet(char* Adress)
{
	return (((uint64_t)Adress >> 6) & 0b11111111111);
}

void RandomizeRawSet(Raw_Cache_Set* pSet)
{
	int LineIdx;
	int* LinesInSet = malloc(sizeof(int) * pSet->ValidLines);

	if (pSet->ValidLines == 0)
		return;

	// At the start of the randomization process, every line points to the
	// next line in the array.
	for (LineIdx = 0; LineIdx < pSet->ValidLines - 1; LineIdx++)
	{
		LinesInSet[LineIdx] = LineIdx + 1;
	}

	int LinesLeft = pSet->ValidLines - 1;
	int CurrLine = 0;

	// Now go over the linked list and randomize it
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

int FindSet(Raw_Cache_Set* pBigSet, Cache_Set* pSmallSet, char* pConflictingLine)
{
	char* pNewSetEnd 				= NULL;
	char* pNonConflictingAnchor 			= pBigSet->Lines[0];
	char* pLinesAnchor 				= pBigSet->Lines[0];
	char* pLinesCurrentEnd				= pBigSet->Lines[0];
	int ConflictLineIndex;
	int LineInEvictionSet 				= 0;
	int CurrentConflictSize 			= pBigSet->ValidLines;
	Raw_Cache_Set tmpSet;
	int SetFound					= 0;
	int BigSetIdx, SmallSetIdx;
	int WhileIdx = 0;
	int LinesToCheck;
	static int ElevenLinesSetsFound = 0;

	// Big set is the linked list with the potential lines for the set.
	// If its less then LINES_IN_SET there isn't really anything to do.
	if (pBigSet->Lines[0] == NULL || pBigSet->ValidLines < LINES_IN_SET)
		return 0;

	RandomizeRawSet(pBigSet);

	// Find the end of the set
	while (*(char**)pLinesCurrentEnd != 0)
	{
		pLinesCurrentEnd = *(char**)pLinesCurrentEnd;
	}

	// If we haven't found enough lines and not enough left to check, leave.
	if (pBigSet->ValidLines < LINES_IN_SET)
	{
		return 0;
	}
	// If we have exactly 12 lines, then we already have the full set.
	else if (pBigSet->ValidLines == LINES_IN_SET)
	{
		memcpy(&tmpSet, pBigSet, sizeof(tmpSet));
		LineInEvictionSet = LINES_IN_SET;
	}
	// Many lines are available, need to extract the ones which belong to the set.
	else
	{
		// Due to noise we might not be able to filter all irrelevant lines on first try.
		// So keep trying!
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

			// Eh, if we tried 5 times and still haven't succeeded, just give up.
			// We don't want to work forever.
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
				if (!ProbeManyTimes(pLinesAnchor, CurrentConflictSize - 1, pConflictingLine))
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
				}
			}
		}
	}

	// Lets check wether these lines conflict
	if (LineInEvictionSet >= LINES_IN_SET)			//Allow only 12 lines/set!!!!
	{
		tmpSet.ValidLines = LineInEvictionSet;
		RandomizeRawSet(&tmpSet);

		// Make sure the new set still conflicts with the line.
		if (ProbeManyTimes(tmpSet.Lines[0], tmpSet.ValidLines, pConflictingLine) &&
			ProbeManyTimes(tmpSet.Lines[0], tmpSet.ValidLines, pConflictingLine) &&
			ProbeManyTimes(tmpSet.Lines[0], tmpSet.ValidLines, pConflictingLine))
		{
			// If there are too many lines, keep trimming then down.
			if (LineInEvictionSet > LINES_IN_SET)
				SetFound = DecreaseSetSize(&tmpSet, pConflictingLine);
			else
				SetFound = 1;
		}

		// Yay, the set is valid, lets call it a day.
		if (SetFound)
		{
			if (LineInEvictionSet == 11)
			{
				ElevenLinesSetsFound++;
			}

			// Remove the lines in the new set from the list of potential lines.
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

			return 1;
		} else {
			printf("Bad set!\r\n");
		}
	} else
	{
		printf("Not enough lines %d in set\r\n", LineInEvictionSet);
	}

	return 0;
}

void RandomizeSet(Cache_Set* pSet)
{
	int LineIdx;
	int* LinesInSet = malloc(sizeof(int) * pSet->ValidLines);

	if (pSet->ValidLines == 0){
		free(LinesInSet);
		return;
	}

	// At the start of the randomization process, every line points to the
	// next line in the array.
	for (LineIdx = 0; LineIdx < pSet->ValidLines - 1; LineIdx++)
	{
		LinesInSet[LineIdx] = LineIdx + 1;
	}

	int LinesLeft = pSet->ValidLines - 1;
	int CurrLine = 0;

	// Now go over the linked list and randomize it
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
			if (!ProbeManyTimes(pLinesAnchor, pSmallSet->ValidLines - 1, pConflictingLine))
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
		while (ProbeManyTimes(pSmallSet->Lines[0], pSmallSet->ValidLines, pConflictingLine))
		{
			return 1;
		}
	}

	return 0;
}
