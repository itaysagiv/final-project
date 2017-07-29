
#ifndef SHARED_DATA_H
#define SHARED_DATA_H

typedef enum eDecryptionState{
	DECRYPTION_ENDED,
	DECRYPTION_STARTED,
	DECRYPTION_IN_PROGRESS,
	DECRYPTION_ERROR,
}eDecryptionState;

typedef struct sSharedMemoryData {
	eDecryptionState state;
	int inMultiply;
	int multiplySampleCount;
	int wasLastMultiplyValid;
	int multiplyCount;
} sSharedMemoryData;

#define SHM_SIZE 1024
#define SHM_PROJ_ID 'R'
#define SHM_FILE_NAME "/home/guyard/workspace/ShmFile.dat"
#define WINDOW_SIZE 6
#define WINDOW_BUFFERS_NUM (64 - 1)

#endif
